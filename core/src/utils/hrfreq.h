#pragma once
#include <string>

namespace hrfreq {
    /**
     * Convert a frequency to a human-readable string.
     * @param freq Frequency in Hz.
     * @return Human-readable representation of the frequency.
    */
    std::string toString(double freq);

    /**
     * Convert a human-readable representation of a frequency to a frequency value.
     * @param str String containing the human-readable frequency.
     * @param freq Value to write the decoded frequency to.
     * @return True on success, false otherwise.
    */
    bool fromString(const std::string& str, double& freq);
}