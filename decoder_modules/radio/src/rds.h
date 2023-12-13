#pragma once
#include <stdint.h>
#include <string>
#include <chrono>
#include <mutex>

namespace rds {
    enum BlockType {
        BLOCK_TYPE_A,
        BLOCK_TYPE_B,
        BLOCK_TYPE_C,
        BLOCK_TYPE_CP,
        BLOCK_TYPE_D,
        _BLOCK_TYPE_COUNT
    };

    enum GroupVersion {
        GROUP_VER_A,
        GROUP_VER_B
    };

    enum AreaCoverage {
        AREA_COVERAGE_INVALID           = -1,
        AREA_COVERAGE_LOCAL             = 0,
        AREA_COVERAGE_INTERNATIONAL     = 1,
        AREA_COVERAGE_NATIONAL          = 2,
        AREA_COVERAGE_SUPRA_NATIONAL    = 3,
        AREA_COVERAGE_REGIONAL1         = 4,
        AREA_COVERAGE_REGIONAL2         = 5,
        AREA_COVERAGE_REGIONAL3         = 6,
        AREA_COVERAGE_REGIONAL4         = 7,
        AREA_COVERAGE_REGIONAL5         = 8,
        AREA_COVERAGE_REGIONAL6         = 9,
        AREA_COVERAGE_REGIONAL7         = 10,
        AREA_COVERAGE_REGIONAL8         = 11,
        AREA_COVERAGE_REGIONAL9         = 12,
        AREA_COVERAGE_REGIONAL10        = 13,
        AREA_COVERAGE_REGIONAL11        = 14,
        AREA_COVERAGE_REGIONAL12        = 15  
    };

    inline const char* AREA_COVERAGE_TO_STR[] = {
        "Local",
        "International",
        "National",
        "Supra-National",
        "Regional 1",
        "Regional 2",
        "Regional 3",
        "Regional 4",
        "Regional 5",
        "Regional 6",
        "Regional 7",
        "Regional 8",
        "Regional 9",
        "Regional 10",
        "Regional 11",
        "Regional 12",
    };

    enum ProgramType {
        // US Types
        PROGRAM_TYPE_US_NONE = 0,
        PROGRAM_TYPE_US_NEWS = 1,
        PROGRAM_TYPE_US_INFOMATION = 2,
        PROGRAM_TYPE_US_SPORTS = 3,
        PROGRAM_TYPE_US_TALK = 4,
        PROGRAM_TYPE_US_ROCK = 5,
        PROGRAM_TYPE_US_CLASSIC_ROCK = 6,
        PROGRAM_TYPE_US_ADULT_HITS = 7,
        PROGRAM_TYPE_US_SOFT_ROCK = 8,
        PROGRAM_TYPE_US_TOP_40 = 9,
        PROGRAM_TYPE_US_COUNTRY = 10,
        PROGRAM_TYPE_US_OLDIES = 11,
        PROGRAM_TYPE_US_SOFT = 12,
        PROGRAM_TYPE_US_NOSTALGIA = 13,
        PROGRAM_TYPE_US_JAZZ = 14,
        PROGRAM_TYPE_US_CLASSICAL = 15,
        PROGRAM_TYPE_US_RHYTHM_AND_BLUES = 16,
        PROGRAM_TYPE_US_SOFT_RHYTHM_AND_BLUES = 17,
        PROGRAM_TYPE_US_FOREIGN_LANGUAGE = 18,
        PROGRAM_TYPE_US_RELIGIOUS_MUSIC = 19,
        PROGRAM_TYPE_US_RELIGIOUS_TALK = 20,
        PROGRAM_TYPE_US_PERSONALITY = 21,
        PROGRAM_TYPE_US_PUBLIC = 22,
        PROGRAM_TYPE_US_COLLEGE = 23,
        PROGRAM_TYPE_US_UNASSIGNED0 = 24,
        PROGRAM_TYPE_US_UNASSIGNED1 = 25,
        PROGRAM_TYPE_US_UNASSIGNED2 = 26,
        PROGRAM_TYPE_US_UNASSIGNED3 = 27,
        PROGRAM_TYPE_US_UNASSIGNED4 = 28,
        PROGRAM_TYPE_US_WEATHER = 29,
        PROGRAM_TYPE_US_EMERGENCY_TEST = 30,
        PROGRAM_TYPE_US_EMERGENCY = 31,

        // EU Types
        PROGRAM_TYPE_EU_NONE = 0,
        PROGRAM_TYPE_EU_NEWS = 1,
        PROGRAM_TYPE_EU_CURRENT_AFFAIRS = 2,
        PROGRAM_TYPE_EU_INFORMATION = 3,
        PROGRAM_TYPE_EU_SPORTS = 4,
        PROGRAM_TYPE_EU_EDUCATION = 5,
        PROGRAM_TYPE_EU_DRAMA = 6,
        PROGRAM_TYPE_EU_CULTURE = 7,
        PROGRAM_TYPE_EU_SCIENCE = 8,
        PROGRAM_TYPE_EU_VARIED = 9,
        PROGRAM_TYPE_EU_POP_MUSIC = 10,
        PROGRAM_TYPE_EU_ROCK_MUSIC = 11,
        PROGRAM_TYPE_EU_EASY_LISTENING_MUSIC = 12,
        PROGRAM_TYPE_EU_LIGHT_CLASSICAL = 13,
        PROGRAM_TYPE_EU_SERIOUS_CLASSICAL = 14,
        PROGRAM_TYPE_EU_OTHER_MUSIC = 15,
        PROGRAM_TYPE_EU_WEATHER = 16,
        PROGRAM_TYPE_EU_FINANCE = 17,
        PROGRAM_TYPE_EU_CHILDRENS_PROGRAM = 18,
        PROGRAM_TYPE_EU_SOCIAL_AFFAIRS = 19,
        PROGRAM_TYPE_EU_RELIGION = 20,
        PROGRAM_TYPE_EU_PHONE_IN = 21,
        PROGRAM_TYPE_EU_TRAVEL = 22,
        PROGRAM_TYPE_EU_LEISURE = 23,
        PROGRAM_TYPE_EU_JAZZ_MUSIC = 24,
        PROGRAM_TYPE_EU_COUNTRY_MUSIC = 25,
        PROGRAM_TYPE_EU_NATIONAL_MUSIC = 26,
        PROGRAM_TYPE_EU_OLDIES_MUSIC = 27,
        PROGRAM_TYPE_EU_FOLK_MUSIC = 28,
        PROGRAM_TYPE_EU_DOCUMENTARY = 29,
        PROGRAM_TYPE_EU_ALARM_TEST = 30,
        PROGRAM_TYPE_EU_ALARM = 31
    };

    inline const char* PROGRAM_TYPE_EU_TO_STR[] = {
        "None",
        "News",
        "Current Affairs",
        "Information",
        "Sports",
        "Education",
        "Drama",
        "Culture",
        "Science",
        "Varied",
        "Pop Music",
        "Rock Music",
        "Easy Listening Music",
        "Light Classical",
        "Serious Classical",
        "Other Music",
        "Weather",
        "Finance",
        "Children Program",
        "Social Affairs",
        "Religion",
        "Phone-in",
        "Travel",
        "Leisure",
        "Jazz Music",
        "Country Music",
        "National Music",
        "Oldies Music",
        "Folk Music",
        "Documentary",
        "Alarm Test",
        "Alarm",
    };

    inline const char* PROGRAM_TYPE_US_TO_STR[] = {
        "None",
        "News",
        "Information",
        "Sports",
        "Talk",
        "Rock",
        "Classic Rock",
        "Adult Hits",
        "Soft Rock",
        "Top 40",
        "Country",
        "Oldies",
        "Soft",
        "Nostalgia",
        "Jazz",
        "Classical",
        "Rythm and Blues",
        "Soft Rythm and Blues",
        "Foreign Language",
        "Religious Music",
        "Religious Talk",
        "Personality",
        "Public",
        "College",
        "Unassigned",
        "Unassigned",
        "Unassigned",
        "Unassigned",
        "Unassigned",
        "Weather",
        "Emergency Test",
        "Emergency",
    };

    enum DecoderIdentification {
        DECODER_IDENT_STEREO = (1 << 0),
        DECODER_IDENT_ARTIFICIAL_HEAD = (1 << 1),
        DECODER_IDENT_COMPRESSED = (1 << 2),
        DECODER_IDENT_VARIABLE_PTY = (1 << 0)
    };

    class RDSDecoder {
    public:
        void process(uint8_t* symbols, int count);

        bool piCodeValid() { std::lock_guard<std::mutex> lck(groupMtx); return blockAValid(); }
        uint16_t getPICode() { std::lock_guard<std::mutex> lck(groupMtx); return piCode; }
        uint8_t getCountryCode() { std::lock_guard<std::mutex> lck(groupMtx); return countryCode; }
        uint8_t getProgramCoverage() { std::lock_guard<std::mutex> lck(groupMtx); return programCoverage; }
        uint8_t getProgramRefNumber() { std::lock_guard<std::mutex> lck(groupMtx); return programRefNumber; }
        std::string getCallsign() { std::lock_guard<std::mutex> lck(groupMtx); return callsign; }
        
        bool programTypeValid() { std::lock_guard<std::mutex> lck(groupMtx); return blockBValid(); }
        ProgramType getProgramType() { std::lock_guard<std::mutex> lck(groupMtx); return programType; }

        bool musicValid() { std::lock_guard<std::mutex> lck(groupMtx); return group0Valid(); }
        bool getMusic() { std::lock_guard<std::mutex> lck(groupMtx); return music; }
        bool PSNameValid() { std::lock_guard<std::mutex> lck(groupMtx); return group0Valid(); }
        std::string getPSName() { std::lock_guard<std::mutex> lck(groupMtx); return programServiceName; }

        bool radioTextValid() { std::lock_guard<std::mutex> lck(groupMtx); return group2Valid(); }
        std::string getRadioText() { std::lock_guard<std::mutex> lck(groupMtx); return radioText; }

    private:
        static uint16_t calcSyndrome(uint32_t block);
        static uint32_t correctErrors(uint32_t block, BlockType type, bool& recovered);
        void decodeBlockA();
        void decodeBlockB();
        void decodeGroup();

        void decodeCallsign();

        bool blockAValid();
        bool blockBValid();
        bool group0Valid();
        bool group2Valid();

        // State machine
        uint32_t shiftReg = 0;
        int sync = 0;
        int skip = 0;
        BlockType lastType = BLOCK_TYPE_A;
        int contGroup = 0;
        uint32_t blocks[_BLOCK_TYPE_COUNT];
        bool blockAvail[_BLOCK_TYPE_COUNT];

        // Block A (All groups)
        std::mutex groupMtx;
        std::chrono::time_point<std::chrono::high_resolution_clock> blockALastUpdate{};  // 1970-01-01
        uint16_t piCode;
        uint8_t countryCode;
        AreaCoverage programCoverage;
        uint8_t programRefNumber;
        std::string callsign;

        // Block B (All groups)
        std::chrono::time_point<std::chrono::high_resolution_clock> blockBLastUpdate{};  // 1970-01-01
        uint8_t groupType;
        GroupVersion groupVer;
        bool trafficProgram;
        ProgramType programType;

        // Group type 0
        std::chrono::time_point<std::chrono::high_resolution_clock> group0LastUpdate{};  // 1970-01-01
        bool trafficAnnouncement;
        bool music;
        uint8_t decoderIdent;
        uint16_t alternateFrequency;
        std::string programServiceName = "        ";

        // Group type 2
        std::chrono::time_point<std::chrono::high_resolution_clock> group2LastUpdate{};  // 1970-01-01
        bool rtAB = false;
        std::string radioText = "                                                                ";

    };
}