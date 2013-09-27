g++ ./src/*.cc -o $1 -std=gnu++11 -Wall -DNDEBUG \
	-O3 -msse4.2 -fno-rtti -flto -s
