#include "stats_collection.h"
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
        if (activated) {
            start_collecting_statistics();
        }
        state_lock.unlock();
    }

    void disconnect() {
        state_lock.lock();
        connected_to_voltha = false;
        stop_collecting_statistics();
        state_lock.unlock();
    }

    void activate() {
        state_lock.lock();
        activated = true;
        if (connected_to_voltha) {
            start_collecting_statistics();
        }
        state_lock.unlock();
    }

    void deactivate() {
        state_lock.lock();
        activated = false;
        stop_collecting_statistics();
        state_lock.unlock();
    }

}
