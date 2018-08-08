#include <mutex>

namespace state {

    bool connected_to_voltha = false;
    bool activated = false;
    std::mutex state_lock;

    bool is_connected() {
        return connected_to_voltha;
    }

    bool is_activated() {
        return activated;
    }

    void connect() {
        state_lock.lock();
        connected_to_voltha = true;
        state_lock.unlock();
    }

    void disconnect() {
        state_lock.lock();
        connected_to_voltha = false;
        state_lock.unlock();
    }

    void activate() {
        state_lock.lock();
        activated = true;
        state_lock.unlock();
    }

    void deactivate() {
        state_lock.lock();
        activated = false;
        state_lock.unlock();
    }

}
