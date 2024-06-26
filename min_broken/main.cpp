#include <mutex>

int main() {
    std::recursive_mutex mtx;
    std::lock_guard<std::recursive_mutex> lck(mtx);
    return 0;
}