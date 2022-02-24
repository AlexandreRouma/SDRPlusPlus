#pragma once
#include <string>
#include <map>
#include <stdexcept>

enum CLIArgType {
    CLI_ARG_TYPE_INVALID,
    CLI_ARG_TYPE_VOID,
    CLI_ARG_TYPE_BOOL,
    CLI_ARG_TYPE_INT,
    CLI_ARG_TYPE_FLOAT,
    CLI_ARG_TYPE_STRING
};

class CommandArgsParser;

class CLIArg {
public:
    CLIArg() {
        type = CLI_ARG_TYPE_INVALID;
    }

    CLIArg(char al, std::string desc) {
        alias = al;
        description = desc;
        type = CLI_ARG_TYPE_VOID;
    }

    CLIArg(char al, std::string desc, bool b) {
        alias = al;
        description = desc;
        type = CLI_ARG_TYPE_BOOL;
        bval = b;
    }

    CLIArg(char al, std::string desc, int i) {
        alias = al;
        description = desc;
        type = CLI_ARG_TYPE_INT;
        ival = i;
    }

    CLIArg(char al, std::string desc, double f) {
        alias = al;
        description = desc;
        type = CLI_ARG_TYPE_FLOAT;
        fval = f;
    }

    CLIArg(char al, std::string desc, std::string s) {
        alias = al;
        description = desc;
        type = CLI_ARG_TYPE_STRING;
        sval = s;
    }

    CLIArg(char al, std::string desc, const char* s) {
        alias = al;
        description = desc;
        type = CLI_ARG_TYPE_STRING;
        sval = s;
    }

    operator bool() const {
        if (type != CLI_ARG_TYPE_BOOL && type != CLI_ARG_TYPE_VOID) { throw std::runtime_error("Not a bool"); }
        return bval;
    }

    operator int() const {
        if (type != CLI_ARG_TYPE_INT) { throw std::runtime_error("Not an int"); }
        return ival;
    }

    operator float() const {
        if (type != CLI_ARG_TYPE_FLOAT) { throw std::runtime_error("Not a float"); }
        return (float)fval;
    }

    operator double() const {
        if (type != CLI_ARG_TYPE_FLOAT) { throw std::runtime_error("Not a float"); }
        return fval;
    }

    operator std::string() const {
        if (type != CLI_ARG_TYPE_STRING) { throw std::runtime_error("Not a string"); }
        return sval;
    }

    bool b() {
        if (type != CLI_ARG_TYPE_BOOL && type != CLI_ARG_TYPE_VOID) { throw std::runtime_error("Not a bool"); }
        return bval;
    }

    int i() {
        if (type != CLI_ARG_TYPE_INT) { throw std::runtime_error("Not an int"); }
        return ival;
    }

    float f() {
        if (type != CLI_ARG_TYPE_FLOAT) { throw std::runtime_error("Not a float"); }
        return (float)fval;
    }

    double d() {
        if (type != CLI_ARG_TYPE_FLOAT) { throw std::runtime_error("Not a float"); }
        return fval;
    }

    const std::string& s() {
        if (type != CLI_ARG_TYPE_STRING) { throw std::runtime_error("Not a string"); }
        return sval;
    }

    friend CommandArgsParser;

    CLIArgType type;
    char alias;
    std::string description;

private:
    bool bval;
    int ival;
    std::string sval;
    double fval;
};

class CommandArgsParser {
public:
    void define(char shortName, std::string name, std::string desc) {
        args[name] = CLIArg(shortName, desc);
        aliases[shortName] = name;
    }

    void defineAll();   

    template<class T>
    void define(char shortName, std::string name, std::string desc, T defValue) {
        args[name] = CLIArg(shortName, desc, defValue);
        aliases[shortName] = name;
    }

    int parse(int argc, char* argv[]);
    void showHelp();

    CLIArg operator[](std::string name) {
        return args[name];
    }

private:
    std::map<std::string, CLIArg> args;
    std::map<char, std::string> aliases;
};