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

# Best Practices

* All additions and/or bug fixes to the core must not add additional dependencies.

