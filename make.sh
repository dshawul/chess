astyle ./src/* -A4T4LYm0poH
rm ./src/*.orig

g++ ./src/*.cc -o $1 -std=c++11 -Wall -pedantic -DNDEBUG \
	-O3 -msse4.2 -fno-rtti -flto -s
