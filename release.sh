echo "building linux compiles"
g++ ./src/*.cc -o ./bin/${1}_sse2 -DNDEBUG -std=gnu++11 -Wall \
	-O3 -msse2 -fno-rtti -flto -s
g++ ./src/*.cc -o ./bin/${1}_sse4.2 -DNDEBUG -std=gnu++11 -Wall \
	-O3 -msse4.2 -fno-rtti -flto -s

echo "building windows compiles"
# Note: currently mingw uses an old GCC, which only supports -std=gnu++0x, and -flto is *broken*
x86_64-w64-mingw32-g++ ./src/*.cc -o ./bin/${1}_sse2.exe -DNDEBUG -std=gnu++0x -Wall \
	-O3 -msse2 -fno-rtti -s -static
x86_64-w64-mingw32-g++ ./src/*.cc -o ./bin/${1}_sse4.2.exe -DNDEBUG -std=gnu++0x -Wall \
	-O3 -msse4.2 -fno-rtti -s -static

echo "make tarball and cleanup"
cd ./bin
tar czf ./${1}.tar.gz ./${1}_sse*
rm ./${1}_sse*
cd ..
