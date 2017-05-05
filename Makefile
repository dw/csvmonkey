

CXXFLAGS+="-std=c++11"
CXXFLAGS+="-O3"
CXXFLAGS+="-DNEED_SSE42"

X=-DNDEBUG
debug: _test


_test:
	g++ -std=c++11 $(CXXFLAGS) -msse4.2 $(X) -g -o test test.cpp


X=-DNDEBUG
pgo:
	g++ -std=c++11 $(CXXFLAGS) -DNDEBUG -fprofile-generate -msse4.2 $(X) -g -o test test.cpp
	./test profiledata.csv
	g++ -std=c++11 $(CXXFLAGS) -DNDEBUG -fprofile-use -msse4.2 $(X) -g -o test test.cpp



grind:
	rm -f cachegrind.out.*
	valgrind --tool=cachegrind --branch-sim=yes ./test ram.64mb.csv
	cg_annotate --auto=yes cachegrind.out.*
