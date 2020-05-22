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

## Building clithread

### External Dependencies

The only external dependecies are listed below.  talloc is easy to download via most package managers.

* [talloc](https://talloc.samba.org/talloc/doc/html/index.html)

### Building with hbgw_middleware

`clithread` is part of the HBuilder Middleware group, so the easiest way to build it is via the `hbgw_middleware` repository.  

1. Install external dependencies.
2. Clone/Download hbgw_middleware repository, and `cd` into it.
3. Do the normal: `make all; sudo make install` 
4. Being a library, the `clithread` lib files will be in the _hbpkg directory.

### Building without hbgw_middleware

If you want to build `clithread` outside of the hbgw_middleware repository framework, you'll need to clone/download the following HBuilder repositories.  You should have all these repo directories stored flat inside a root directory.

* _hbsys
* argtable
* cJSON
* bintex

From this point:

```
$ cd clithread
$ make pkg
```

You can find the binary inside `clithread/bin/`