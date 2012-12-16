DiscoCheck
==========

DiscoCheck is a free UCI chess engine. It is not a complete chess program, but requires an UCI
compatible GUI in order to be used comfortably.

Installing
----------

Read the documentation for your GUI of choice for information about how to use DiscoCheck with your
GUI.

If you're using 64-bit Windows or Linux, you should use the official compiles provided. Those are in
the root directory and named `DCxyz_lin64` (Linux) or `DCxyz_win64` for Windows, where `xyz` is the
version number `x.y.z`.

Otherwise, you'll have to compile it yourself (see below).

Compiling
---------

POSIX, with g++ installed:

	$ ./make.sh

Windows: unfortunately, you'll have to figure it out for yourself, as I don't use Windows.

Terms of use
------------

DiscoCheck is free, and distributed under the GNU General Public License (GPL). Essentially, this
means that you are free to do almost exactly what you want with the program, including distributing
it among your friends, making it available for download from your web site, selling it (either by
itself or as part of some bigger software package), or using it as the starting point for a software
project of your own.

The only real limitation is that whenever you distribute DiscoCheck in some way, you must always
include the full source code, or a pointer to where the source code can be found. If you make any
changes to the source code, these changes must also be made available under the GPL.

For full details, read the copy of the GPL found in the file named `./license`.

Enjoy!

Lucas
