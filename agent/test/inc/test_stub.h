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
#ifndef __TEST_STUB_H__
#define __TEST_STUB_H__

#include <pthread.h>
#include <unistd.h>
#include <stdint.h>

#include "bcmolt_conn_mgr.h"
#include "bcmcli_session.h"
#include "bcmos_errno.h"

// defines some stub definitions that are not available to openolt application
// when compoling in TEST_MODE
//#define BCMOLT_TM_QUEUE_KEY_TM_Q_SET_ID_DEFAULT 0
#define MAX_SUPPORTED_PON 16


void bcmos_usleep(uint32_t us);
void bcmos_fastlock_init(bcmos_fastlock *lock, uint32_t flags);
long bcmos_fastlock_lock(bcmos_fastlock *lock);
void bcmos_fastlock_unlock(bcmos_fastlock *lock, long flags);

#endif //__TEST_STUB_H__

