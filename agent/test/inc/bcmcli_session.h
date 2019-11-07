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

