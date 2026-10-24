#ifndef IRUKA_PRIVILEGED_DAEMON_H
#define IRUKA_PRIVILEGED_DAEMON_H


#include <functional>
#include <mutex>
#include <string>
#include <sys/types.h>

class PrivilegedDaemon {
public:
    static PrivilegedDaemon &instance();

    bool start();

    void stop();

    bool isRunning() const { return m_inFd >= 0; }

    int run(const std::string &shellCommand,
            const std::function<void(const std::string &)> &sink);

    void cancel();

private:
    PrivilegedDaemon() = default;

    int     m_inFd  = -1;
    int     m_outFd = -1;
    pid_t   m_pid   = -1;
    std::mutex m_runMutex;
    std::mutex m_startMutex;
};

#endif
