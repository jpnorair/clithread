# About clithread

`clithread` is a client thread manager library that implements several of the features of [talloc](https://talloc.samba.org/talloc/doc/html/index.html) in conjunction with basic thread group management.  

When a client connects to a server using `clithread`, the server will spawn a new client thread (i.e. a _clithread_). In the _clithread_, the server executes the interaction with the client.  This is pretty normal methodology for daemons and servers that utilize a socket interface.  

`clithread` has a few special features, however:

* `clithread` maintains a linked-list of active threads.  If the server terminates, it can meaningfully terminate all the active client threads using a single library function (_clithread\_deinit()_)
* `clithread` uses **talloc** to maintain a hierarchical memory allocation management system.  If you use _talloc()_ instead of _malloc()_ in your client implementation, all heap allocations done within your client thread will be automatically freed upon its exit.
* `clithread` can work with memory pools.  If you supply a pool-size in _clithread\_add()_, it will create this pool and apply it to this new thread.  All allocation calls should be via talloc to take advantage of this feature.

#### Other features
* Can work with detached threads.  When a thread exits, it is removed from the linked-list
* Can work with managed threads.  _clithread\_del()_ stops a thread and deletes its instance.


