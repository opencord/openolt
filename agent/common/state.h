/*
 * Copyright 2018-present Open Networking Foundation

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

    bool previously_connected() {
        return connected_once;
    }

    void connect() {
        connected_to_voltha = true;
        connected_once = true;
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
    bool connected_once = false;
};
#endif
