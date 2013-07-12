astyle ./src/* -A4T4LYm0poH
rm ./src/*.orig

# Find the date of the last commit, in the format: YYYYMMDD. We use this date as automatic version
# number, passed through the preprocessor variable DISCO_VERSION
v=`git log -1 --format="%ci"|sed "s/^\([0-9]*\)\-\([0-9]*\)\-\([0-9]*\)\>.*/\1\2\3/g"`

g++ ./src/*.cc -o $1 -DNDEBUG -DDISCO_VERSION=\"${v}\" -std=c++11 -Wall -pedantic \
	-O3 -msse4.2 -fno-rtti -flto -s

