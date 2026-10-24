#ifndef IRUKA_CATEGORY_H
#define IRUKA_CATEGORY_H

#include <string>
#include <vector>

namespace category {

const std::vector<std::string> &all();

std::string classify(const std::string &name, const std::string &text);

}

#endif
