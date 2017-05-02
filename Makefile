

CXXFLAGS+="-std=c++11"
CXXFLAGS+="-O3"
CXXFLAGS+="-DNEED_SSE42"

all:
	g++ -std=c++11 $(CXXFLAGS) -msse4.2 $(X) -g -o test test.cpp


grind:
	rm cachegrind.out.*
	valgrind --tool=cachegrind --branch-sim=yes ./test
	cg_annotate --auto=yes cachegrind.out.*
