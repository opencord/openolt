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
#ifndef __BCMOLT_CONN_MGR_H__
#define __BCMOLT_CONN_MGR_H__
// This is stub header file for unit tets case only
//
extern "C" {
typedef char bcmolt_cm_addr[100];

typedef int bcmolt_goid;

typedef struct bcmos_fastlock {
    pthread_mutex_t lock;
} bcmos_fastlock;

}
#endif
