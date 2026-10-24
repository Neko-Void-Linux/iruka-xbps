#ifndef IRUKA_UTILS_H
#define IRUKA_UTILS_H

#include <string>
#include <vector>

namespace utils {

std::string getConfigDir();
bool hasInternetConnection();
bool hasBinary(const std::string &path);
std::string getShell();
std::string detectX11Display();

std::string resolveLocaledir();

}

#endif
