#include "command_args.h"
#include <filesystem>

void CommandArgsParser::defineAll() {
#if defined(_WIN32)
        std::string root = ".";
        define('c', "con", "Show console on Windows");
#elif defined(IS_MACOS_BUNDLE)
        std::string root = (std::string)getenv("HOME") + "/Library/Application Support/sdrpp";
#elif defined(__ANDROID__)
        std::string root = "/storage/self/primary/sdrpp";
#else
        std::string root = (std::string)getenv("HOME") + "/.config/sdrpp";
#endif

        define('a', "addr", "Server mode address", "0.0.0.0");
        define('h', "help", "Show help");
        define('p', "port", "Server mode port", 5259);
        define('r', "root", "Root directory, where all config files are stored", std::filesystem::absolute(root).string());
        define('s', "server", "Run in server mode");
        define('\0', "autostart", "Automatically start the SDR after loading");
}

int CommandArgsParser::parse(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        // Check for long and short name arguments
        if (!arg.rfind("--", 0)) {
            arg = arg.substr(2);
        }
        else if (!arg.rfind("-", 0)) {
            if (aliases.find(arg[1]) == aliases.end()) {
                printf("Unknown argument\n");
                showHelp();
                return -1;
            }
            arg = aliases[arg[1]];
        }
        else {
            printf("Invalid argument\n");
            showHelp();
            return -1;
        }

        // Make sure the argument exists
        if (args.find(arg) == args.end()) {
            printf("Unknown argument\n");
            showHelp();
            return -1;
        }

        // Parse depending on type
        CLIArg& carg = args[arg];
        
        // If not void, make sure an argument is available and retrieve it
        if (carg.type != CLI_ARG_TYPE_VOID && i + 1 >= argc) {
            printf("Missing argument\n");
            showHelp();
            return -1;
        }

        // Parse void since arg won't be needed
        if (carg.type == CLI_ARG_TYPE_VOID) {
            carg.bval = true;
            continue;
        }

        // Parse types that require parsing
        arg = argv[++i];
        if (carg.type == CLI_ARG_TYPE_BOOL) {
            // Enforce lower case
            for (int i = 0; i < arg.size(); i++) { arg[i] = std::tolower(arg[i]); }

            if (arg == "true" || arg == "on" || arg == "1") {
                carg.bval = true;
            }
            else if (arg == "false" || arg == "off" || arg == "0") {
                carg.bval = true;
            }
            else {
                printf("Invalid argument, expected bool (true, false, on, off, 1, 0)\n");
                showHelp();
                return -1;
            }
        }
        else if (carg.type == CLI_ARG_TYPE_INT) {
            try {
                carg.ival = std::stoi(arg);
            }
            catch (const std::exception& e) {
                printf("Invalid argument, failed to parse integer\n");
                showHelp();
                return -1;
            }
        }
        else if (carg.type == CLI_ARG_TYPE_FLOAT) {
            try {
                carg.fval = std::stod(arg);
            }
            catch (const std::exception& e) {
                printf("Invalid argument, failed to parse float\n");
                showHelp();
                return -1;
            }
        }
        else if (carg.type == CLI_ARG_TYPE_STRING) {
            carg.sval = arg;
        }
    }

    return 0;
}

void CommandArgsParser::showHelp() {
    for (auto const& [ln, arg] : args) {
        if (arg.alias) {
            printf("-%c --%s\t\t%s\n", arg.alias, ln.c_str(), arg.description.c_str());
        }
        else {
            printf("   --%s\t\t%s\n", ln.c_str(), arg.description.c_str());
        }
    }
}