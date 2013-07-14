name="discocheck_"$1
echo ${name}

echo "building linux compiles"
g++ ./src/*.cc -o ./bin/${name}_sse2 -DNDEBUG -DDISCO_VERSION=\"${1}\" -std=c++11 -pedantic \
	-O3 -msse2 -fno-rtti -flto -s
g++ ./src/*.cc -o ./bin/${name}_sse4.2 -DNDEBUG -DDISCO_VERSION=\"${1}\" -std=c++11 -pedantic \
	-O3 -msse4.2 -fno-rtti -flto -s

echo "building windows compiles"
x86_64-w64-mingw32-g++ ./src/*.cc -o ./bin/${name}_sse2.exe -DNDEBUG -DDISCO_VERSION=\"${1}\" -std=c++0x -pedantic \
	-O3 -msse2 -fno-rtti -flto -s -static
x86_64-w64-mingw32-g++ ./src/*.cc -o ./bin/${name}_sse4.2.exe -DNDEBUG -DDISCO_VERSION=\"${1}\" -std=c++0x -pedantic \
	-O3 -msse4.2 -fno-rtti -flto -s -static

echo "make tarball and cleanup"
cd ./bin
tar czf ./${name}.tar.gz ./${name}_sse*
rm ./${name}_sse*
cd ..

