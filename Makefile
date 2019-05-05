
CXXFLAGS += -std=c++11
CXXFLAGS += -O3
CXXFLAGS += -Wall
CXXFLAGS += -lc
#CXXFLAGS += -DUSE_SPIRIT

debug: CXXFLAGS+=-O0 -g
debug: test

release: X=-DNDEBUG
release: test tests/fullsum

test: test.cpp csvmonkey.hpp Makefile
	g++ -std=c++11 $(CXXFLAGS) -msse4.2 $(X) -g -o test test.cpp

tests/fullsum: tests/fullsum.cpp csvmonkey.hpp Makefile
	g++ -std=c++11 $(CXXFLAGS) -msse4.2 $(X) -g -o tests/fullsum tests/fullsum.cpp

clean:
	rm -f tests/fullsum test cachegrind* perf.data* *.gcda

pgo: X+=-DNDEBUG
pgo:
	g++ -std=c++11 $(CXXFLAGS) -DNDEBUG -fprofile-generate -msse4.2 $(X) -g -o test test.cpp
	./test tests/data/profiledata.csv
	g++ -std=c++11 $(CXXFLAGS) -DNDEBUG -fprofile-use -msse4.2 $(X) -g -o test test.cpp

grind:
	rm -f cachegrind.out.*
	valgrind --tool=cachegrind --branch-sim=yes ./test ram.64mb.csv
	cg_annotate --auto=yes cachegrind.out.*
