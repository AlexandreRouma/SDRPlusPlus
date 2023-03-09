#pragma once
#include <string>
#include <vector>
#include <stdexcept>

template <class K, class T>
class OptionList {
public:
    OptionList() { updateText(); }

    void define(const K& key, const std::string& name, const T& value) {
        if (keyExists(key)) { throw std::runtime_error("Key already exists"); }
        if (nameExists(name)) { throw std::runtime_error("Name already exists"); }
        if (valueExists(value)) { throw std::runtime_error("Value already exists"); }
        keys.push_back(key);
        names.push_back(name);
        values.push_back(value);
        updateText();
    }

    void define(const std::string& name, const T& value) {
        define(name, name, value);
    }

    void undefine(int id) {
        keys.erase(keys.begin() + id);
        names.erase(names.begin() + id);
        values.erase(values.begin() + id);
        updateText();
    }

    void undefineKey(const K& key) {
        undefine(keyId(key));
    }

    void undefineName(const std::string& name) {
        undefine(nameId(name));
    }

    void undefineValue(const T& value) {
        undefine(valueId(value));
    }

    void clear() {
        keys.clear();
        names.clear();
        values.clear();
        updateText();
    }

    int size() const {
        return keys.size();
    }

    bool empty() const {
        return keys.empty();
    }

    bool keyExists(const K& key) const {
        if (std::find(keys.begin(), keys.end(), key) != keys.end()) { return true; }
        return false;
    }

    bool nameExists(const std::string& name) const {
        if (std::find(names.begin(), names.end(), name) != names.end()) { return true; }
        return false;
    }

    bool valueExists(const T& value) const {
        if (std::find(values.begin(), values.end(), value) != values.end()) { return true; }
        return false;
    }

    int keyId(const K& key) const {
        auto it = std::find(keys.begin(), keys.end(), key);
        if (it == keys.end()) { throw std::runtime_error("Key doesn't exists"); }
        return std::distance(keys.begin(), it);
    }

    int nameId(const std::string& name) const {
        auto it = std::find(names.begin(), names.end(), name);
        if (it == names.end()) { throw std::runtime_error("Name doesn't exists"); }
        return std::distance(names.begin(), it);
    }

    int valueId(const T& value) const {
        auto it = std::find(values.begin(), values.end(), value);
        if (it == values.end()) { throw std::runtime_error("Value doesn't exists"); }
        return std::distance(values.begin(), it);
    }

    inline const K& key(int id) const {
        return keys[id];
    }

    inline const std::string& name(int id) const {
        return names[id];
    }

    inline const T& value(int id) const {
        return values[id];
    }

    inline const T& operator[](int& id) const {
        return values[id];
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