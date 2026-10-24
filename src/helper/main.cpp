
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <poll.h>

static volatile pid_t g_child = -1;

static void termHandler(int) {
    pid_t p = g_child;
    if (p > 0) kill(-p, SIGKILL);
    _exit(0);
}

static bool writeAll(int fd, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        off += static_cast<size_t>(n);
    }
    return true;
}

static bool readLine(int fd, std::string &line) {
    line.clear();
    char c;
    for (;;) {
        ssize_t n = read(fd, &c, 1);
        if (n == 0) return false;
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (c == '\n') return true;
        line.push_back(c);
    }
}

int main() {
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (getppid() == 1) return 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = termHandler;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    signal(SIGPIPE, SIG_IGN);

    for (;;) {
        std::string cmd;
        if (!readLine(STDIN_FILENO, cmd)) break;

        int pfd[2];
        if (pipe(pfd) != 0) {
            writeAll(STDOUT_FILENO, "__IRUKA_DONE__ 1\n", 18);
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            setpgid(0, 0);
            dup2(pfd[1], STDOUT_FILENO);
            dup2(pfd[1], STDERR_FILENO);
            close(pfd[0]);
            close(pfd[1]);
            execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)nullptr);
            _exit(127);
        }
        if (pid < 0) {
            close(pfd[0]);
            close(pfd[1]);
            writeAll(STDOUT_FILENO, "__IRUKA_DONE__ 1\n", 18);
            continue;
        }

        g_child = pid;
        close(pfd[1]);

        bool childDone = false;
        bool guiGone   = false;
        std::string pending;

        while (!childDone) {
            struct pollfd fds[2];
            fds[0].fd = pfd[0];         fds[0].events = POLLIN; fds[0].revents = 0;
            fds[1].fd = STDIN_FILENO;   fds[1].events = POLLIN; fds[1].revents = 0;

            int r = poll(fds, 2, -1);
            if (r < 0) {
                if (errno == EINTR) continue;
                break;
            }

            if (fds[1].revents & (POLLIN | POLLHUP)) {
                char b[512];
                ssize_t n = read(STDIN_FILENO, b, sizeof(b));
                if (n <= 0) {
                    guiGone = true;
                    break;
                }
                pending.append(b, static_cast<size_t>(n));
                if (pending.find("CANCEL") != std::string::npos) {
                    kill(-pid, SIGTERM);
                    kill(-pid, SIGKILL);
                    pending.clear();
                }
            }

            if (fds[0].revents & (POLLIN | POLLHUP)) {
                char b[4096];
                ssize_t n = read(pfd[0], b, sizeof(b));
                if (n > 0) {
                    if (!writeAll(STDOUT_FILENO, b, static_cast<size_t>(n))) {
                        guiGone = true;
                        break;
                    }
                } else {
                    childDone = true;
                }
            }
        }

        if (guiGone) {
            kill(-pid, SIGKILL);
            close(pfd[0]);
            waitpid(pid, nullptr, 0);
            return 0;
        }

        close(pfd[0]);
        int status = 0;
        waitpid(pid, &status, 0);
        g_child = -1;

        int code = WIFEXITED(status)
            ? WEXITSTATUS(status)
            : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1);

        char done[64];
        int dl = snprintf(done, sizeof(done), "\n__IRUKA_DONE__ %d\n", code);
        if (dl > 0 && !writeAll(STDOUT_FILENO, done, static_cast<size_t>(dl)))
            return 0;
    }

    return 0;
}
