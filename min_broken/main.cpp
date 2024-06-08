#include <stdio.h>
#include <mutex>

class TestClass {
public:
    TestClass() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        value = 42;
    }

    int getValue() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        return value;
    }

private:
    std::recursive_mutex mtx;
    int value = 0;
};

TestClass test;

int main() {
    printf("Value: %d\n", test.getValue());
    return 0;
}