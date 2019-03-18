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



clithread_handle_t clithread_init(void) {
    clithread_item_t** head;
    
    head = malloc(sizeof(clithread_item_t*));
    if (head == NULL) {
        return NULL;
    }
    
    *head = NULL;
    return (clithread_handle_t)head;
}


clithread_item_t* clithread_add(clithread_handle_t handle, const pthread_attr_t* attr, int poolsize, void* (*start_routine)(void*), clithread_args_t* arg) {
    clithread_item_t* newitem;
    clithread_item_t* head;
    
    if (handle == NULL) {
        return NULL;
    }

    newitem = malloc(sizeof(clithread_item_t));
    if (newitem != NULL) {
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
            newitem->args.tctx = talloc_pool(NULL, poolsize);
            if (newitem->args.tctx == NULL) {
                goto clithread_add_ERR;
            }
        }
        
        if (pthread_create(&newitem->client, attr, start_routine, (void*)&newitem->args) != 0) {
            goto clithread_add_ERR;
        }
        
        //pthread_detach(newitem->client);
        head            = *(clithread_item_t**)handle;
        newitem->prev   = NULL;
        newitem->next   = head;
        if (head != NULL) {
            head->prev  = newitem;
        }
        head            = newitem;
    }

    return newitem;
    
    clithread_add_ERR:
    free(newitem);
    return NULL;
}



static void sub_clithread_free(void* self) {
    clithread_item_t* item = (clithread_item_t*)self;
    clithread_item_t* previtem;
    clithread_item_t* nextitem;
    
    previtem = item->prev;
    nextitem = item->next;
    
    // This will free all data allocated on this context via talloc
    talloc_free(item->args.tctx);
    
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
            talloc_free(head->args.tctx);
            free(head);
        }
        
        /// Free the handle itself
        free(handle);
    }
}


