#include "privileged_daemon.h"
#include "constants.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <unistd.h>
#include <sys/wait.h>

namespace {

std::string helperPath() {
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "iruka-helper";
    buf[n] = '\0';
    std::string p(buf);
    size_t slash = p.find_last_of('/');
    if (slash != std::string::npos) p.erase(slash + 1);
    else p.clear();
    return p + "iruka-helper";
}

const char *kDoneMarker = "__IRUKA_DONE__";

}

PrivilegedDaemon &PrivilegedDaemon::instance() {
    static PrivilegedDaemon inst;
    return inst;
}

bool PrivilegedDaemon::start() {
    std::lock_guard<std::mutex> lock(m_startMutex);
    if (m_inFd >= 0) return true;

    std::string helper = helperPath();
    if (access(helper.c_str(), X_OK) != 0) return false;

    int inPipe[2]  = { -1, -1 };
    int outPipe[2] = { -1, -1 };
    if (pipe(inPipe) != 0) return false;
    if (pipe(outPipe) != 0) {
        close(inPipe[0]);
        close(inPipe[1]);
        return false;
    }

    pid_t pid = fork();
    if (pid == 0) {
        dup2(inPipe[0],  STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(outPipe[1], STDERR_FILENO);
        close(inPipe[0]);
        close(inPipe[1]);
        close(outPipe[0]);
        close(outPipe[1]);
        execl(PKEXEC_BIN, "pkexec", helper.c_str(), (char*)nullptr);
        _exit(127);
    }

    close(inPipe[0]);
    close(outPipe[1]);

    if (pid < 0) {
        close(inPipe[1]);
        close(outPipe[0]);
        return false;
    }

    m_inFd  = inPipe[1];
    m_outFd = outPipe[0];
    m_pid   = pid;
    return true;
}

void PrivilegedDaemon::stop() {
    if (m_inFd >= 0) { close(m_inFd);  m_inFd = -1; }
    if (m_outFd >= 0) { close(m_outFd); m_outFd = -1; }
    if (m_pid > 0) {
        waitpid(m_pid, nullptr, WNOHANG);
        m_pid = -1;
    }
}

void PrivilegedDaemon::cancel() {
    if (m_inFd < 0) return;
    const char *msg = "CANCEL\n";
    ssize_t r = write(m_inFd, msg, 7);
    (void)r;
}

int PrivilegedDaemon::run(const std::string &shellCommand,
                          const std::function<void(const std::string &)> &sink) {
    std::lock_guard<std::mutex> lock(m_runMutex);

    if (!start() || m_inFd < 0 || m_outFd < 0) return -1;

    std::string line = shellCommand;
    line += '\n';
    size_t off = 0;
    while (off < line.size()) {
        ssize_t n = write(m_inFd, line.data() + off, line.size() - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            stop();
            return -1;
        }
        off += static_cast<size_t>(n);
    }

    const std::string marker(kDoneMarker);
    std::string buf;
    char chunk[4096];
    int code = -1;

    for (;;) {
        ssize_t n = read(m_outFd, chunk, sizeof(chunk));
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;

        buf.append(chunk, static_cast<size_t>(n));

        size_t pos = buf.find(marker);
        if (pos != std::string::npos) {
            std::string out = buf.substr(0, pos);
            if (!out.empty() && sink) sink(out);

            size_t eol = buf.find('\n', pos);
            std::string num = (eol != std::string::npos)
                ? buf.substr(pos + marker.size(), eol - (pos + marker.size()))
                : buf.substr(pos + marker.size());
            code = std::atoi(num.c_str());
            break;
        }

        size_t keep = marker.size() + 16;
        if (buf.size() > keep) {
            size_t fwd = buf.size() - keep;
            if (sink) sink(buf.substr(0, fwd));
            buf.erase(0, fwd);
        }
    }

    if (code < 0) {
        if (!buf.empty() && sink && buf.find(marker) == std::string::npos) sink(buf);
        stop();
    }
    return code;
}
