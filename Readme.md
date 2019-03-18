# About clithread (library)

clithread is a client thread manager library that implements several of the features of talloc in conjunction with basic thread group management.  

The way it is used, is for a server thread to spawn a new client thread (clithread), when a client connects.  In the client thread, the server executes the interaction with the client.  This is pretty normal methodology for daemons and servers that utilize a socket interface.  

clithread has a few special features, however:

* clithread maintains a linked-list of active threads.  If the server terminates, it can meaningfully terminate all the active client threads using a single library function (clithread_deinit)
* clithread uses "talloc" to maintain a hierarchical memory allocation management system.  If you use talloc() instead of malloc() in your client thread implementation, clithread will automatically garbage collect all heap allocations done within that thread, upon its exit.
* clithread can work with memory pools.  If you supply a pool-size in clithread_add(), it will create this pool and apply it to this new thread.  All allocation calls should be via talloc to take advantage of this feature.
* Can work with detached threads.  When a thread exits, it is removed from the linked-list
* Can work with managed threads.  clithread_del() stops a thread and deletes its instance.
* Can utilize memory pools for fast allocations within threads.  c

