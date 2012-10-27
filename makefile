all:
	g++ *.cc -o ./chess -DNDEBUG -std=c++11 -O4 -flto -fno-rtti -Wall
	
debug:
	g++ *.cc -o ./chess -std=c++11 -g -flto -Wall

clean:
	rm -f *.o
	rm -f chess
