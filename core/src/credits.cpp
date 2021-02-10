#include <credits.h>

namespace sdrpp_credits {
    const char* contributors[] = {
        "Ryzerth (Author)",
        "aosync",
        "Alexsey Shestacov",
        "Benjamin Kyd",
        "Cropinghigh",
        "Howard0su",
        "Martin Hauke",
        "Raov",
        "Szymon Zakrent",
        "Tobias Mädel"
    };

    const char* libraries[] = {
        "Dear ImGui (ocornut)",
        "json (nlohmann)",
        "portaudio (P.A. comm.)",
        "SoapySDR (PothosWare)",
        "spdlog (gabime)",
    };

    const char* patrons[] = {
        "SignalsEverywhere",
        "Lee Donaghy",
        "Daniele D'Agnelli"
    };

    const int contributorCount = sizeof(contributors) / sizeof(char*);
    const int libraryCount = sizeof(libraries) / sizeof(char*);
    const int patronCount = sizeof(patrons) / sizeof(char*);
}