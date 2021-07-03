#include <credits.h>

namespace sdrpp_credits {
    const char* contributors[] = {
        "Alexandre Rouma (Author)",
        "Aang23",
        "Alexsey Shestacov",
        "Aosync",
        "Benjamin Kyd",
        "Benjamin Vernoux",
        "Cropinghigh",
        "Fred F4EED",
        "Howard0su",
        "Joshua Kimsey",
        "Martin Hauke",
        "Marvin Sinister",
        "Paulo Matias",
        "Raov",
        "Starman0620",
        "Szymon Zakrent",
        "Tobias Mädel",
        "Zimm"
    };

    const char* libraries[] = {
        "Dear ImGui (ocornut)",
        "json (nlohmann)",
        "RtAudio",
        "SoapySDR (PothosWare)",
        "spdlog (gabime)",
        "Portable File Dialogs"
    };

    const char* patrons[] = {
        "Croccydile",
        "Daniele D'Agnelli",
        "W4IPA",
        "Lee Donaghy",
        "ON4MU",
        "Passion-Radio.com",
        "Scanner School",
        "SignalsEverywhere",
        "Syne Ardwin (WI9SYN)"
    };

    const int contributorCount = sizeof(contributors) / sizeof(char*);
    const int libraryCount = sizeof(libraries) / sizeof(char*);
    const int patronCount = sizeof(patrons) / sizeof(char*);
}