# Linux (dynamic compiles)
g++ ./src/*.cc -o ./bin/${1}_sse2 -DNDEBUG -std=c++11 -pedantic -O3 -msse2 -fno-rtti -flto -s
g++ ./src/*.cc -o ./bin/${1}_sse4.2 -DNDEBUG -std=c++11 -pedantic -O3 -msse4.2 -fno-rtti -flto -s

# Windows (static mingw compiles)
x86_64-w64-mingw32-g++ ./src/*.cc -o ./bin/${1}_sse2.exe -DNDEBUG -std=c++0x -pedantic -O3 -msse2 -fno-rtti -flto -s -static
x86_64-w64-mingw32-g++ ./src/*.cc -o ./bin/${1}_sse4.2.exe -DNDEBUG -std=c++0x -pedantic -O3 -msse4.2 -fno-rtti -flto -s -static

# Make compressed tarball
tar czf ./bin/${1}.tar.gz ./bin/${1}_sse*
rm ./bin/${1}_sse*
