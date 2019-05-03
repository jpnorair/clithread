/* Copyright 2014, JP Norair
  *
  * Licensed under the OpenTag License, Version 1.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * http://www.indigresso.com/wiki/doku.php?id=opentag:license_1_0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  */

#ifndef clithread_h
#define clithread_h

// Configuration Header
//#include "otdb_cfg.h"
//#include "cliopt.h"

// talloc library from /usr/local/include
#include <talloc.h>

#include <stdint.h>
#include <pthread.h>


typedef struct {
    int fd_in;
    int fd_out;
    void* clithread_self;
    void* app_handle;
    TALLOC_CTX* tctx;
} clithread_args_t;

typedef uint32_t clithread_xid_t;

typedef struct ptlist {
    clithread_xid_t     xid;
    pthread_t           client;
    clithread_args_t    args;
    struct ptlist*      prev;
    struct ptlist*      next;
    void*               internal;
} clithread_item_t;

typedef clithread_item_t** clithread_handle_t;


clithread_handle_t clithread_init(void);

clithread_item_t* clithread_add(clithread_handle_t handle, const pthread_attr_t* attr, size_t est_allocs, size_t poolsize, void* (*start_routine)(void*), clithread_args_t* arg);

int clithread_sigup(clithread_item_t*);


/** @brief Client thread must call this upon exit or return
  * @param self     (void*) Must be the clithread_self arg element
  * @retval None
  *
  * Rather than use pthread_exit() or return(), any client thread created by
  * clithread_add() must use clithread_exit() instead.
  *
  * clithread_exit() should not be used by a supervisory thread, or any other
  * thread except the client thread itself.
  */
void clithread_exit(void* self);

void clithread_del(clithread_item_t* item);

void clithread_deinit(clithread_handle_t handle);

clithread_xid_t clithread_chxid(clithread_item_t* item, clithread_xid_t new_xid);

void clithread_publish(clithread_handle_t handle, clithread_xid_t xid, uint8_t* msg, size_t msgsize);



#endif
