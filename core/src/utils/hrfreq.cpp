#include "hrfreq.h"
#include <utils/flog.h>

namespace hrfreq {


    std::string toString(double freq) {
        // Determine the scale
        int maxDecimals = 0;
        const char* suffix = "Hz";
        if (freq >= 1e9) {
            freq /= 1e9;
            maxDecimals = 9;
            suffix = "GHz";
        }
        else if (freq >= 1e6) {
            freq /= 1e6;
            maxDecimals = 6;
            suffix = "MHz";
        }
        else if (freq >= 1e3) {
            freq /= 1e3;
            maxDecimals = 3;
            suffix = "KHz";
        }

        // Convert to string (TODO: Not sure if limiting the decimals rounds)
        char numBuf[128];
        int numLen = sprintf(numBuf, "%0.*lf", maxDecimals, freq);

        // If there is a decimal point, remove the useless zeros
        if (maxDecimals) {
            for (int i = numLen-1; i >= 0; i--) {
                bool dot = (numBuf[i] == '.');
                if (numBuf[i] != '0' && !dot) { break; }
                numBuf[i] = 0;
                if (dot) { break; }
            }
        }

        // Concat the suffix
        char finalBuf[128];
        sprintf(finalBuf, "%s%s", numBuf, suffix);

        // Return the final string
        return finalBuf;
    }

    bool isNumeric(char c) {
        return std::isdigit(c) || c == '+' || c == '-' || c == '.' || c == ',';
    }

    bool fromString(const std::string& str, double& freq) {
        // Skip non-numeric characters
        int i = 0;
        char c;
        for (; i < str.size(); i++) {
            if (isNumeric(str[i])) { break; }
        }

        // Extract the numeric part
        std::string numeric;
        for (; i < str.size(); i++) {
            // Get the character
            c = str[i];

            // If it's a letter, stop
            if (std::isalpha(c)) { break; }

            // If isn't numeric, skip it
            if (!isNumeric(c)) { continue; }

            // If it's a comma, skip it for now. This enforces a dot as a decimal point
            if (c == ',') { continue; }

            // Add the character to the numeric string
            numeric += c;
        }

        // Attempt to parse the numeric part
        double num;
        try {
            num = std::stod(numeric);
        }
        catch (const std::exception& e) {
            flog::error("Failed to parse numeric part: '{}'", numeric);
            return false;
        }

        // If no more text is available, assume the numeric part gives a frequency in Hz
        if (i == str.size()) {
            flog::warn("No unit given, assuming it's Hz");
            freq = num;
            return true;
        }

        // Scale the numeric value depending on the first scale character
        char scale = std::toupper(str[i]);
        switch (scale) {
        case 'G':
            num *= 1e9;
            break;
        case 'M':
            num *= 1e6;
            break;
        case 'K':
            num *= 1e3;
            break;
        case 'H':
            break;
        default:
            flog::warn("Unknown frequency scale: '{}'", scale);
            break;
        }

        // Return the frequency
        freq = num;
        return true; // TODO
    }
}