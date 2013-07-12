# Find the date of the last commit, in the format: YYYYMMDD. We use this date as automatic version
# number, passed through the preprocessor variable DISCO_VERSION
vd=`git log -1 --format="%ci"|sed "s/^\([0-9]*\)\-\([0-9]*\)\-\([0-9]*\)\>.*/\1\2\3/g"`
name="discocheck_"${vd}
echo "version number: ${vd}"

echo "building linux compiles"
g++ ./src/*.cc -o ./bin/${name}_sse2 -DNDEBUG -DDISCO_VERSION=\"${vd}\" -std=c++11 -pedantic \
	-O3 -msse2 -fno-rtti -flto -s
g++ ./src/*.cc -o ./bin/${name}_sse4.2 -DNDEBUG -DDISCO_VERSION=\"${vd}\" -std=c++11 -pedantic \
	-O3 -msse4.2 -fno-rtti -flto -s

echo "building windows compiles"
x86_64-w64-mingw32-g++ ./src/*.cc -o ./bin/${name}_sse2.exe -DNDEBUG -DDISCO_VERSION=\"${vd}\" -std=c++0x -pedantic \
	-O3 -msse2 -fno-rtti -flto -s -static
x86_64-w64-mingw32-g++ ./src/*.cc -o ./bin/${name}_sse4.2.exe -DNDEBUG -DDISCO_VERSION=\"${vd}\" -std=c++0x -pedantic \
	-O3 -msse4.2 -fno-rtti -flto -s -static

echo "make tarball and cleanup"
tar czf ./bin/${name}.tar.gz ./bin/${name}_sse*
rm ./bin/${name}_sse*

