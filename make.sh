astyle ./src/* -A4T4LYm0poH
rm ./src/*.orig

g++ ./src/*.cc -o $1 -DNDEBUG -std=c++11 -Wall -pedantic -O3 -msse4.2 -fno-rtti -flto -s
