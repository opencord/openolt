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
#ifndef __BCMCLI_SESSION_H__
#define __BCMCLI_SESSION_H__
#include "bcmos_errno.h"
// This is a stub header file for unit test case only
typedef int bcmcli_session;
typedef char bcmcli_entry[50];
typedef char bcmcli_cmd_parm;
struct bcmos_task {
    char task_name[50];
};

typedef struct bcmcli_session_parm {
    void *get_prompt;
    int access_right;
} bcmcli_session_parm;

#define BCMCLI_ACCESS_ADMIN 0xff
#define BUG_ON //
#define BCMCLI_MAKE_CMD_NOPARM(ptr, command1, command2, command3)
#define BAL_API_VERSION 3


extern bool bcmcli_is_stopped(bcmcli_session *sess);
extern bool bcmcli_parse(bcmcli_session *sess, char *s);
extern bool bcmcli_driver(bcmcli_session *sess);
extern void bcmcli_token_destroy(void *);
extern void bcmcli_session_close(bcmcli_session *sess);
extern bcmos_errno bcm_api_cli_set_commands(bcmcli_session *sess);
extern void bcmcli_stop(bcmcli_session *sess);
extern void bcmcli_session_print(bcmcli_session *sess, const char *s);
extern bcmos_errno bcmcli_session_open(bcmcli_session_parm *mon_sess, bcmcli_session **curr_sess);
#endif

