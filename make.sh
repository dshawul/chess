g++ ./src/*.cc -o $1 -DNDEBUG -std=c++11 -pedantic -O3 -msse4.2 -fno-rtti -flto -s
