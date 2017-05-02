

all:
	g++ -std=c++11 -O3 -msse4.2 $(X) -g -o test test.cpp

grind:
	rm cachegrind.out.*
	valgrind --tool=cachegrind --branch-sim=yes ./test
	cg_annotate --auto=yes cachegrind.out.*
