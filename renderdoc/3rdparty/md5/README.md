Fetched from https://openwall.info/wiki/people/solar/software/public-domain-source-code/md5 on 2021-07-07

Public domain licensed:

> This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
> MD5 Message-Digest Algorithm (RFC 1321).
>
> Homepage:
> http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
>
> Author:
> Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
>
> This software was written by Alexander Peslyak in 2001.  No copyright is
> claimed, and the software is hereby placed in the public domain.
> In case this attempt to disclaim copyright and place the software in the
> public domain is deemed null and void, then the software is
> Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
> general public under the following terms:
>
> Redistribution and use in source and binary forms, with or without
> modification, are permitted.
>
> There's ABSOLUTELY NO WARRANTY, express or implied.
>
> (This is a heavily cut-down "BSD license".)
>
> This differs from Colin Plumb's older public domain implementation in that
> no exactly 32-bit integer data type is required (any 32-bit or wider
> unsigned integer data type will do), there's no compile-time endianness
> configuration, and the function prototypes match OpenSSL's.  No code from
> Colin Plumb's implementation has been reused; this comment merely compares
> the properties of the two independent implementations.
>
> The primary goals of this implementation are portability and ease of use.
> It is meant to be fast, but not as fast as possible.  Some known
> optimizations are not included to reduce source code size and avoid
> compile-time configuration.
