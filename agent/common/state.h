/*
    Copyright (C) 2018 Open Networking Foundation

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

    bool previsouly_connected() {
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
