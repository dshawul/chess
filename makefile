all:
	g++ *.cc -o ./chess -DNDEBUG -std=c++11 -O3 -flto -Wall
	
debug:
	g++ *.cc -o ./chess -std=c++0x -g -flto -Wall

clean:
	rm -f *.o
	rm -f chess
