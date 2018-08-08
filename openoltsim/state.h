#ifndef OPENOLT_STATE_H_
#define OPENOLT_STATE_H_

namespace state {
    bool is_activated();
    bool is_connected();
    void connect();
    void disconnect();
    void activate();
    void deactivate();
}

#endif
