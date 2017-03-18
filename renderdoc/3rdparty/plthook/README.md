PLT Hook
========

What is plthook.
----------------

A utility library to hook library function calls issued by
specified object files (executable and libraries).

Usage
-----

If you have a library `libfoo.so.1` and want to intercept
a function call `recv()` without modifying the library,
put `plthook.c` (or `plthook_win32.c` for Windows) and `plthook.h`
in your source tree and add the following code.


    static ssize_t my_recv(int sockfd, void *buf, size_t len, int flags)
    {
        ssize_t rv;
    
        ... do your task: logging, etc. ...
        rv = recv(sockfd, buf, len, flags); /* call real recv(). */
        ... do your task: logging, check received data, etc. ...
        return rv;
    }
    
    int install_hook_function()
    {
        plthook_t *plthook;
    
        if (plthook_open(&plthook, "libfoo.so.1") != 0) {
            printf("plthook_open error: %s\n", plthook_error());
            return -1;
        }
        if (plthook_replace(plthook, "recv", (void*)my_recv, NULL) != 0) {
            printf("plthook_replace error: %s\n", plthook_error());
            plthook_close(plthook);
            return -1;
        }
        plthook_close(plthook);
        return 0;
    }

Supported Platforms
-------------------

* Linux i386 and x86_64 by plthook_elf.c
* Windows 32-bit and x64 (MSVC, Mingw32 and Cygwin) by plthook_win32.c
* OS X (tested on Mavericks) by plthook_osx.c
* Solaris x86_64 by plthook_elf.c
* FreeBSD i386 and x86_64 except i386 program on x86_64 OS by plthook_elf.c

License
-------

2-clause BSD-style license.
