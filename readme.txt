0/ Pre-requisites
-----------------
download some UCI compatible engine. example:
	http://wbec-ridderkerk.nl/html/details1/DoubleCheck.html

understand the FEN notation and long algebraic notation of moves.

understand the basics of the UCI chess engine protocol, and experiment by running manually the engine
in command line.

C Coding standard:
	http://www.jetcafe.org/jim/c-style.html
	let's try to adhere to it, without being too rigid about it either!

1/ Hierarchy
------------

types.h
	defines types used by bitboard and board modules

bitboard.h
	bitboard module (low-level)
	implementation in bitboard.c

board.h
	board module (high level, any useful chess functions on a chess board)
	implementation:
		board.c (general stuff)
		move.c (move properties and move conversion functions)
		movegen.c (move generators and perft)

process.h
	process functions (spawn/kill processes, and talking to them via pipes)
	implementation in process.c

2/ compilation
--------------

gcc *.c -o ./chess -std=c99 -O4 -flto -DNDEBUG -Wall

makefile is probably not necessary and premature at this early stage, but definitely to do later.
