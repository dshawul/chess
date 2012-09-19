

all:
	g++ *.cc -o ./chess -DNDEBUG -std=c++0x -O4 -flto -Wall
	
debug:
	g++ *.cc -o ./chess -std=c++0x -g -flto -Wall

clean:
	rm -f *.o
	rm -f chess
