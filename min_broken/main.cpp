#include <mutex>

std::recursive_mutex mtx;

int main() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    return 0;
}