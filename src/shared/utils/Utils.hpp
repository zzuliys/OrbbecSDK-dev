#pragma once

#include <stdint.h>
#include <vector>
#include <memory>
#include <algorithm>

// Include all the headers in the utils directory for convenience
#include "StringUtils.hpp"
#include "FileUtils.hpp"
#include "PublicTypeHelper.hpp"

namespace libobsensor {
namespace utils {

uint64_t getNowTimesMs();
uint64_t getNowTimesUs();
void     sleepMs(uint64_t msec);

#pragma pack(push, 1)
template <class T> class big_endian {
    T be_value;

public:
    operator T() const {
        T le_value = 0;
        for(unsigned int i = 0; i < sizeof(T); ++i)
            reinterpret_cast<char *>(&le_value)[i] = reinterpret_cast<const char *>(&be_value)[sizeof(T) - i - 1];
        return le_value;
    }
};
#pragma pack(pop)

template <typename T>
void unusedVar(T& var){
    (void)var;
}

template <class T> bool contains(const T &first, const T &second) {
    return *first == *second;
}

template <class T> std::vector<std::shared_ptr<T>> subtract_sets(const std::vector<std::shared_ptr<T>> &first, const std::vector<std::shared_ptr<T>> &second) {
    std::vector<std::shared_ptr<T>> results;
    std::for_each(first.begin(), first.end(), [&](std::shared_ptr<T> data) {
        if(std::find_if(second.begin(), second.end(), [&](std::shared_ptr<T> new_dev) { return contains(data, new_dev); }) == second.end()) {
            results.push_back(data);
        }
    });
    return results;
}

template <class T> std::vector<T> subtract_sets(const std::vector<T> &first, const std::vector<T> &second) {
    std::vector<T> results;
    std::for_each(first.begin(), first.end(), [&](T data) {
        if(std::find_if(second.begin(), second.end(), [&](T new_dev) { return contains(data, new_dev); }) == second.end()) {
            results.push_back(data);
        }
    });
    return results;
}

bool checkJpgImageData(const uint8_t *data, size_t dataLen);

}  // namespace utils
}  // namespace libobsensor