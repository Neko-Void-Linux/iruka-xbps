#include "utils.h"
#include "constants.h"
#include "i18n.h"
#include <cstdlib>
#include <cstdio>
#include <climits>
#include <unistd.h>
#include <dirent.h>
#include <algorithm>

namespace utils {

std::string resolveLocaledir() {
  const char *env = getenv("IRUKA_LOCALEDIR");
  if (env && env[0]) return env;

  char buf[PATH_MAX];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n > 0) {
    buf[n] = '\0';
    std::string exe(buf);
    std::string dir = exe.substr(0, exe.find_last_of('/'));
    std::string po = dir + "/po";
    if (access((po + "/es/LC_MESSAGES/" + IRUKA_TEXTDOMAIN + ".mo").c_str(), R_OK) == 0)
      return po;
  }

  return LOCALEDIR;
}

std::string getConfigDir() {
  const char *home = getenv("HOME");
  if (!home) home = "/tmp";
  return std::string(home) + "/" + IRUKA_CONFIG_DIR_NAME;
}

bool hasInternetConnection() {
  FILE *pipe = popen("ping -c 1 -W 2 voidlinux.org 2>/dev/null", "r");
  if (!pipe) return false;
  int ret = pclose(pipe);
  return (ret == 0);
}

bool hasBinary(const std::string &path) {
  return access(path.c_str(), X_OK) == 0;
}

std::string getShell() {
  const char *shell = getenv("SHELL");
  if (!shell) shell = "/bin/sh";
  return shell;
}

std::string detectX11Display() {
  const char *d = getenv("DISPLAY");
  if (d && d[0]) {
    std::string sock("/tmp/.X11-unix/X");
    std::string display(d);

    size_t colon = display.find(':');
    if (colon != std::string::npos) {
      std::string num = display.substr(colon + 1);
      size_t dot = num.find('.');
      if (dot != std::string::npos) num = num.substr(0, dot);
      if (!num.empty() && access((sock + num).c_str(), X_OK) == 0) {
        return display;
      }
    }
  }

  DIR *dir = opendir("/tmp/.X11-unix");
  if (!dir) return "";

  int best = -1;
  struct dirent *ent;
  while ((ent = readdir(dir)) != nullptr) {
    if (ent->d_name[0] != 'X') continue;
    int num = atoi(ent->d_name + 1);
    if (num >= 0 && (best == -1 || num < best)) best = num;
  }
  closedir(dir);

  if (best >= 0) return ":" + std::to_string(best);
  return "";
}

}
