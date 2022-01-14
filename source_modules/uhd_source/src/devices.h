#ifndef DEVICES_H
#define DEVICES_H

#include "device.h"

#include <algorithm>
#include <vector>

class Devices {
public:
    using ContainerType = std::vector<Device>;

    Devices();

    // returns the index at which it was inserted
    int add(Device&& device);
    Device& at(int index);
    int size() const;
    bool empty() const;
    void clear();

    void sortBySerial();

    using iterator = ContainerType::iterator;
    using const_iterator = ContainerType::const_iterator;
    using reverse_iterator = ContainerType::reverse_iterator;
    using const_reverse_iterator = ContainerType::const_reverse_iterator;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;
    reverse_iterator rbegin();
    reverse_iterator rend();
    const_reverse_iterator crbegin() const;
    const_reverse_iterator crend() const;

private:
    void insert_dummy_device();
    static void sort_devices_by_serial(ContainerType& devices);

    ContainerType mDevices;
};

#endif // DEVICES_H
