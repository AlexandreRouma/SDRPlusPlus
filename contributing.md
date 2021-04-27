# Pull Requests

TODO

# Code Style

## Naming Convention

- Namespaces: `CamelCase`
- Classes: `CamelCase`
- Structs: `CamelCase_t`
- Members: `camelCase`
- Enum: `SNAKE_CASE`
- Macros: `SNAKE_CASE`

## Brace Style

```c++
int myFunction() {
    if (shortIf) { shortFunctionName(); }

    if (longIf) {
        longFunction();
        otherStuff();
        myLongFunction();
    }
}
```

Note: If it makes the code cleaner, remember to use the `?` keyword instead of a `if else` statement.

## Structure

Headers and their associated C++ files shall be in the same directory. All headers must use `#pragma once` instead of other include guards. Only include files in a header that are being used in that header. Include the rest in the associated C++ file.

# Modules

## Module Naming Convention

All modules names must be `snake_case`. If the module is a source, it must end with `_source`. If it is a sink, it must end with `_sink`.

For example, lets take the module named `cool_source`:

- Directory: `cool_source`
- Class: `CoolSourceModule`
- Binary: `cool_source.<os dynlib extension>`

## Integration into main repository

If the module meets the code quality requirements, it may be added to the official repository. A module that doesn't require any external dependencies that the core doesn't already use may be enabled for build by default. Otherwise, they must be disabled for build by default with a `OPT_BUILD_MODULE_NAME` variable set to `OFF`.

# JSON Formatting

The ability to add new radio band allocation identifiers and color maps relies on JSON files. Proper formatting of these JSOn files is important for reference and readability. The folowing guides will show you how to properly format the JSON files for their respective uses.

**IMPORTANT: JSON File cannot contain comments, there are only in this example for clarity**

## Band Frequency Allocation 

Please follow this guide to properly format the JSON files for custom radio band allocation identifiers.

```json
{
    "name": "Short name (has to fit in the menu)",
    "country_name": "Name of country or area, if applicable (Use '--' otherwise)",
    "country_code": "Two letter country code, if applicable (Use '--' otherwise)",
    "author_name": "Name of the original/main creator of the JSON file",
    "author_url": "URL the author wishes to be associated with the file (personal website, GitHub, Twitter, etc)",
    "bands": [ 
        // Bands in this array must be sorted by their starting frequency
        {
            "name": "Name of the band",
            "type": "Type name ('amateur', 'broadcast', 'marine', 'military', or any type decalre in config.json)",
            "start": 148500, //In Hz, must be an integer
            "end": 283500 //In Hz, must be an integer
        },
        {
            "name": "Name of the band",
            "type": "Type name ('amateur', 'broadcast', 'marine', 'military', or any type decalre in config.json)",
            "start": 526500, //In Hz, must be an integer
            "end": 1606500 //In Hz, must be an integer
        }    
    ]
}
```

## Color Maps

Please follow this guide to properly format the JSON files for custom color maps.

```json
{
    "name": "Short name (has to fit in the menu)",
    "author": "Name of the original/main creator of the color map",
    "map": [
        // These are the color codes, in hexidecimal (#RRGGBB) format, for the custom color scales for the waterfall. They must be entered as strings, not integers, with the hastag/pound-symbol proceeding the 6 digit number. 
        "#000020",
        "#000030",
        "#000050",
        "#000091",
        "#1E90FF",
        "#FFFFFF",
        "#FFFF00",
        "#FE6D16",
        "#FE6D16",
        "#FF0000",
        "#FF0000",
        "#C60000",
        "#9F0000",
        "#750000",
        "#4A0000"
    ]
}
```

# Best Practices

* All additions and/or bug fixes to the core must not add additional dependencies.

* Use VSCode for development, VS seems to cause issues.