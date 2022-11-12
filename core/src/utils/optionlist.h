#pragma once
#include <string>
#include <vector>
#include <stdexcept>

template <class K, class T>
class OptionList {
public:
    OptionList() { updateText(); }

    void define(K key, std::string name, T value) {
        if (keyExists(key)) { throw std::runtime_error("Key already exists"); }
        if (nameExists(name)) { throw std::runtime_error("Name already exists"); }
        if (valueExists(value)) { throw std::runtime_error("Value already exists"); }
        keys.push_back(key);
        names.push_back(name);
        values.push_back(value);
        updateText();
    }

    void define(std::string name, T value) {
        define(name, name, value);
    }

    void undefined(int id) {
        keys.erase(keys.begin() + id);
        names.erase(names.begin() + id);
        values.erase(values.begin() + id);
        updateText();
    }

    void undefineKey(K key) {
        undefined(keyId(key));
    }

    void undefineName(std::string name) {
        undefined(nameId(name));
    }

    void undefineValue(T value) {
        undefined(valueId(value));
    }

    void clear() {
        keys.clear();
        names.clear();
        values.clear();
        updateText();
    }

    int size() {
        return keys.size();
    }

    bool keyExists(K key) {
        if (std::find(keys.begin(), keys.end(), key) != keys.end()) { return true; }
        return false;
    }

    bool nameExists(std::string name) {
        if (std::find(names.begin(), names.end(), name) != names.end()) { return true; }
        return false;
    }

    bool valueExists(T value) {
        if (std::find(values.begin(), values.end(), value) != values.end()) { return true; }
        return false;
    }

    int keyId(K key) {
        auto it = std::find(keys.begin(), keys.end(), key);
        if (it == keys.end()) { throw std::runtime_error("Key doesn't exists"); }
        return std::distance(keys.begin(), it);
    }

    int nameId(std::string name) {
        auto it = std::find(names.begin(), names.end(), name);
        if (it == names.end()) { throw std::runtime_error("Name doesn't exists"); }
        return std::distance(names.begin(), it);
    }

    int valueId(T value) {
        auto it = std::find(values.begin(), values.end(), value);
        if (it == values.end()) { throw std::runtime_error("Value doesn't exists"); }
        return std::distance(values.begin(), it);
    }

    K key(int id) {
        return keys[id];
    }

    std::string name(int id) {
        return names[id];
    }

    T value(int id) {
        return values[id];
    }

    T operator[](int& id) {
        return value(id);
    }

    const char* txt = NULL;

private:
    void updateText() {
        _txt.clear();
        for (auto& name : names) {
            _txt += name;
            _txt += '\0';
        }
        txt = _txt.c_str();
    }

    std::vector<K> keys;
    std::vector<std::string> names;
    std::vector<T> values;
    std::string _txt;
};