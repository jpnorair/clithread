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


typedef struct {
    bool            predicate;
    pthread_cond_t  cond;
    pthread_mutex_t mutex;
} waiter_t;

static waiter_t add_wait;



clithread_handle_t clithread_init(void) {
    clithread_item_t** head;
    
    if ((pthread_mutex_init(&add_wait.mutex, NULL) != 0)
    ||  (pthread_cond_init(&add_wait.cond, NULL) != 0)) {
        return NULL;
    }
    
    head = malloc(sizeof(clithread_item_t*));
    if (head == NULL) {
        return NULL;
    }
    
    *head = NULL;
    return (clithread_handle_t)head;
}


clithread_item_t* clithread_add(clithread_handle_t handle, const pthread_attr_t* attr, size_t est_allocs, size_t poolsize, void* (*start_routine)(void*), clithread_args_t* arg) {
    clithread_item_t* newitem;
    int rc;

    if (handle == NULL) {
        return NULL;
    }

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
        
        // The first thing alloced to the thread internal heap is the internal data
        if (pthread_create(&newitem->client, attr, start_routine, (void*)&newitem->args) != 0) {
            goto clithread_add_ERR;
        }
        
        // Wait for the thread to release via clithread_release.
        clock_gettime(CLOCK_REALTIME, &alarm);
        alarm.tv_sec += 1;
        pthread_mutex_lock(&add_wait.mutex);
        rc = pthread_cond_timedwait(&add_wait.cond, &add_wait.mutex, &alarm);
        pthread_mutex_unlock(&add_wait.mutex);
        if (rc != 0) {
            goto clithread_add_ERR;
        }
        
        newitem->xid = 0;
        //pthread_detach(newitem->client);
        newitem->prev   = NULL;
        newitem->next   = *(clithread_item_t**)handle;
        if (*(clithread_item_t**)handle != NULL) {
            (*(clithread_item_t**)handle)->prev  = newitem;
        }
        *(clithread_item_t**)handle = newitem;
    }
    
    return newitem;
    
    clithread_add_ERR:
    talloc_free(newitem->args.tctx);
    free(newitem);
    return NULL;
}


int clithread_sigup(clithread_item_t* client) {
    pthread_mutex_lock(&add_wait.mutex);
    pthread_cond_signal(&add_wait.cond);
    pthread_mutex_unlock(&add_wait.mutex);
    return 0;
}



static void sub_clithread_free(void* self) {
    clithread_item_t* item = (clithread_item_t*)self;
    clithread_item_t* previtem;
    clithread_item_t* nextitem;
    
    previtem = item->prev;
    nextitem = item->next;
    
    // This will free all data allocated on this context via talloc
    talloc_free(item->args.tctx);
    
    // Now free the item itself
    free(item);

    /// If previtem==NULL, this item is the head
    /// If nextitem==NULL, this item is the end
    if (previtem != NULL) {
        previtem->next = nextitem;
    }
    if (nextitem != NULL) {
        nextitem->prev = previtem;
    }
}



void clithread_exit(void* self) {
///@note to be used at the end of a thread that's created by clithread_add()
    clithread_item_t* item = (clithread_item_t*)self;
    
    // Thread will detach itself, meaning that no other thread needs to join it
    pthread_detach(item->client);
    
    // This is a cleanup handler that will free the thread from the clithread
    // list, after the thread terminates.
    pthread_cleanup_push(&sub_clithread_free, item);
    pthread_exit(NULL);
    pthread_cleanup_pop(1);
}



void clithread_del(clithread_item_t* item) {
    if (item != NULL) {
        pthread_cancel(item->client);
        pthread_join(item->client, NULL);
        sub_clithread_free(item);
    }
}




void clithread_deinit(clithread_handle_t handle) {
    clithread_item_t* lastitem;
    clithread_item_t* head;
    
    if (handle != NULL) {
        /// Go to end of the list
        lastitem    = NULL;
        head        = *(clithread_item_t**)handle;
        while (head != NULL) {
            lastitem    = head;
            head        = head->next;
        }
        
        /// Cancel Threads from back to front, and free list items
        /// pthread_join() is used instead of pthread_detach(), because the deinit
        /// operation should block until the clithread system is totally
        /// deinitialized.
        while (lastitem != NULL) {
            head        = lastitem;
            lastitem    = lastitem->prev;
            
            pthread_cancel(head->client);
            pthread_join(head->client, NULL);
            
            // These free operations are unnecessary because they are taken care-of in
            // the thread exit routine.
            //talloc_free(head->args.tctx);
            //free(head);
        }
        
        /// Destroy mutex and conds
        pthread_cond_destroy(&add_wait.cond);
        pthread_mutex_destroy(&add_wait.mutex);
        
        /// Free the handle itself
        free(handle);
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


///@todo this fails and hangs in the loop when a thread drops before this gets called
///      or possibly in the middle of it being called.  Need a synchronization hook to
///      know if a thread drops.
void clithread_publish(clithread_handle_t handle, clithread_xid_t xid, uint8_t* msg, size_t msgsize) {
    clithread_item_t* head;
    
    if (handle != NULL) {
        head = *(clithread_item_t**)handle;

        /// Push the message to all clithreads that have active fd_out and matching xid.
        while (head != NULL) {
            if ((head->args.fd_out > 0) && (head->xid == xid)) {
                write(head->args.fd_out, msg, msgsize);
            }
            head = head->next;
        }
    }
}


