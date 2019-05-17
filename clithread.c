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

// Application Headers
//#include "cliopt.h"
#include "clithread.h"
//#include "debug.h"

// Standard C & POSIX Libraries
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>


int clithread_init(clithread_handle_t* handle) {
    clithread_t* ct;
    
    if (handle == NULL) {
        return -1;
    }
    
    ct = calloc(1, sizeof(clithread_t));
    if (ct == NULL) {
        return -2;
    }
    //ct->size = 0;
    //ct->head = NULL;
    
    if ((pthread_mutex_init(&ct->mutex, NULL) != 0)
    ||  (pthread_cond_init(&ct->cond, NULL) != 0)) {
        goto clithread_init_ERR1;
    }

    *((clithread_t**)handle) = ct;
    return 0;
    
    clithread_init_ERR1:
    free(ct);
    return -3;
}


clithread_item_t* clithread_add(clithread_handle_t handle, const pthread_attr_t* attr, size_t est_allocs, size_t poolsize, void* (*start_routine)(void*), clithread_args_t* arg) {
    clithread_t* cth;
    clithread_item_t* newitem;
    clithread_item_t* head;
    int rc;

    if (handle == NULL) {
        return NULL;
    }

    cth     = (clithread_t*)handle;
    newitem = malloc(sizeof(clithread_item_t));
    if (newitem != NULL) {
        struct timespec alarm;
    
        if (arg == NULL) {
            newitem->args.app_handle= NULL;
            newitem->args.fd_in     = -1;
            newitem->args.fd_out    = -1;
            newitem->args.tctx      = NULL;
        }
        else {
            newitem->args = *arg;
        }
        
        // Link the parent
        newitem->parent = cth;

        // This self linkage enables the thread to use:
        // pthread_cleanup_push(&clithread_selfclean, args->clithread_self);
        // To cleanup after itself
        newitem->args.clithread_self = (void*)newitem;

        // If a talloc context is not provided explicitly, create one
        if (newitem->args.tctx == NULL) {
            newitem->args.tctx = talloc_pooled_object(NULL, void*, (unsigned int)est_allocs, poolsize);
            if (newitem->args.tctx == NULL) {
                goto clithread_add_ERR;
            }
        }
        
        pthread_mutex_lock(&cth->mutex);
        
        // The first thing alloced to the thread internal heap is the internal data
        if (pthread_create(&newitem->client, attr, start_routine, (void*)&newitem->args) != 0) {
            goto clithread_add_ERR1;
        }
        
        // Wait for the thread to release via clithread_release.
        clock_gettime(CLOCK_REALTIME, &alarm);
        alarm.tv_sec += 1;
        cth->predicate = true;
        rc = 0;
        while (cth->predicate && (rc == 0)) {
            rc = pthread_cond_timedwait(&cth->cond, &cth->mutex, &alarm);
        }
        pthread_mutex_unlock(&cth->mutex);
        if (rc != 0) {
            goto clithread_add_ERR2;
        }
        
        //Detaching the thread is probably a bad idea
        //pthread_detach(newitem->client);
        
        // Link the client thread onto front of the list
        newitem->xid    = 0;
        newitem->prev   = NULL;
        newitem->next   = cth->head;
        if (cth->head != NULL) {
            cth->head->prev = newitem;
        }

        // Final step: update the handle parameters
        cth->size++;
        cth->head = newitem;

    }
    
    return newitem;
    
    clithread_add_ERR2:
    if (pthread_cancel(newitem->client) == 0) {
        pthread_join(newitem->client, NULL);
    }
    
    clithread_add_ERR1:
    talloc_free(newitem->args.tctx);
    free(newitem);
    
    clithread_add_ERR:
    return NULL;
}


int clithread_sigup(clithread_item_t* client) {
    clithread_t* cth;

    if (client != NULL) {
        cth = client->parent;
        if (cth != NULL) {
            pthread_mutex_lock(&cth->mutex);
            cth->predicate = false;
            pthread_cond_signal(&cth->cond);
            pthread_mutex_unlock(&cth->mutex);
            return 0;
        }
        return -2;
    }
    return -1;
}



static void sub_unlink_item(clithread_t* cth, clithread_item_t* item) {
    clithread_item_t* previtem;
    clithread_item_t* nextitem;
    
    if (cth != NULL) {
        previtem    = item->prev;
        nextitem    = item->next;
        item->prev  = NULL;
        item->next  = NULL;
        cth->size  -= (cth->size != 0);
        
        if ((previtem == NULL) && (nextitem == NULL)) {
            cth->head = NULL;
        }
        else {
            if (previtem != NULL) {
                previtem->next = nextitem;
            }
            if (nextitem != NULL) {
                nextitem->prev = previtem;
            }
        }
    }
}


static void sub_clithread_free(void* item) {
    // This will free all data allocated on this context via talloc
    talloc_free( ((clithread_item_t*)item)->args.tctx);
    
    // Now free the item itself
    free(item);
}



void clithread_exit(void* self) {
///@note to be used at the end of a thread that's created by clithread_add()
    clithread_item_t* item;

    if (self != NULL) {
        item = (clithread_item_t*)self;

        // If there's a parent object (there should be), adjust linkage to remove this thread.
        sub_unlink_item((clithread_t*)item->parent, item);
    
        // Thread will detach itself, meaning that no other thread needs to join it
        pthread_detach(item->client);
    
        // This is a cleanup handler that will free the thread from the clithread
        // list, after the thread terminates.
        pthread_cleanup_push(&sub_clithread_free, item);
        pthread_exit(NULL);
        pthread_cleanup_pop(1);
    }
}



void clithread_del(clithread_item_t* item) {
    if (item != NULL) {
        sub_unlink_item((clithread_t*)item->parent, item);
    
        pthread_cancel(item->client);
        pthread_join(item->client, NULL);
        sub_clithread_free(item);
    }
}




void clithread_deinit(clithread_handle_t handle) {
    clithread_t* cth;
    
    if (handle != NULL) {
        cth = handle;
        
        while (cth->head != NULL) {
            clithread_del(cth->head);
        }
        
        /// Destroy mutex and conds
        pthread_cond_destroy(&cth->cond);
        pthread_mutex_destroy(&cth->mutex);
        
        /// Free the handle itself
        free(cth);
    }
}


clithread_xid_t clithread_chxid(clithread_item_t* item, clithread_xid_t new_xid) {
    clithread_xid_t old_xid = 0;

    if (item != NULL) {
        old_xid     = item->xid;
        item->xid   = new_xid;
    }
    
    return old_xid;
}



void clithread_publish(clithread_handle_t handle, clithread_xid_t xid, uint8_t* msg, size_t msgsize) {
    clithread_t* cth;
    clithread_item_t* item;

    if (handle != NULL) {
        cth     = handle;
        item    = cth->head;

        /// Push the message to all clithreads that have active fd_out and matching xid.
        while (item != NULL) {
            if ((item->args.fd_out > 0) && (item->xid == xid)) {
                write(item->args.fd_out, msg, msgsize);
            }
            item = item->next;
        }
    }
}


