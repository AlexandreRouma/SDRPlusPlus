#pragma once
#include <string>

namespace utils {
    std::string formatFreq(double freq) {
        char str[128];
        if (freq >= 1000000.0) {
            sprintf(str, "%.06lf", freq / 1000000.0);
            int len = strlen(str) - 1;
            while ((str[len] == '0' || str[len] == '.') && len > 0) {
                len--;
                if (str[len] == '.') {
                    len--;
                    break;
                }
            }
            return std::string(str).substr(0, len + 1) + "MHz";
        }
        else if (freq >= 1000.0) {
            sprintf(str, "%.06lf", freq / 1000.0);
            int len = strlen(str) - 1;
            while ((str[len] == '0' || str[len] == '.') && len > 0) {
                len--;
                if (str[len] == '.') {
                    len--;
                    break;
                }
            }
            return std::string(str).substr(0, len + 1) + "KHz";
        }
        else {
            sprintf(str, "%.06lf", freq);
            int len = strlen(str) - 1;
            while ((str[len] == '0' || str[len] == '.') && len > 0) {
                len--;
                if (str[len] == '.') {
                    len--;
                    break;
                }
            }
            return std::string(str).substr(0, len + 1) + "Hz";
        }
    }
}