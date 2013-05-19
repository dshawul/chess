### Overview

DiscoCheck is a free UCI chess engine. It is not a complete chess program and requires a UCI
compatible interface to be used comfortably. You can also use it directly in command line, but that
is only of interest for programmers.

### Options

DiscoCheck has the following UCI options

* Hash: this is the size, in MB, of the hash table.
* Clear Hash (button): clears the hash table.
* Contempt. Make DiscoCheck avoid draws (by chess rules) by scoring them -Contempt for the engine and
+Contempt for the opponent. For example, if Contempt=25, DiscoCheck will think that a draw by chess
rules (3-repetition, 50-move, stalemate, insufficient material) is worth -25cp for itself, and will
assume that the opponent values such a draw at +25cp (DiscoCheck tries to avoid draws knowing that
its opponent seeks them).

### Compiling it yourself

On Linux, assuming you have g++ installed, simply run './make.sh' to compile. It should also work on
any POSIX system with g++ installed (eg. MacOSX, FreeBSD, etc.)

For other operating systems (ie. Windows), and/or compilers (ie. MSVC, ICC), I don't know. So you
will have to figure it out. That being said, I have tried hard to write code as portable as possible,
but there may be a few things that are GCC specific. If you find something that is not portable and
should be rewritten to improve portability, please let me know (patches and pull requests are
welcome).

### Terms of use

DiscoCheck is free, and distributed under the GNU General Public License (GPL). Essentially, this
means that you are free to do almost exactly what you want with the program, including distributing
it among your friends, making it available for download from your web site, selling it (either by
itself or as part of some bigger software package), or using it as the starting point for a software
project of your own.

The only real limitation is that whenever you distribute DiscoCheck in some way, you must always
include the full source code, or a pointer to where the source code can be found, as well as the GPL
license agreement included with it. If you make any changes to the source code, these changes must
also be made available under the GPL.

For full details, read the copy of the GPL found in the file named `./license`.