#ifndef OPENOLT_STATE_H_
#define OPENOLT_STATE_H_

class State {
  public:

    bool is_connected() {
        return connected_to_voltha;
    }

    bool is_activated() {
        return activated;
    }

    void connect() {
        connected_to_voltha = true;
    }

    void disconnect() {
        connected_to_voltha = false;
    }

    void activate() {
        activated = true;
    }

    void deactivate() {
        activated = false;
    }

  private:
    bool connected_to_voltha = false;
    bool activated = false;
};
#endif
