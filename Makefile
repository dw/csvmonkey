
CXXFLAGS += -std=c++11
CXXFLAGS += -O3
CXXFLAGS += -Wall
CXXFLAGS += -lc
#CXXFLAGS += -DUSE_SPIRIT

debug: CXXFLAGS+=-O0 -g
debug: test

release: X=-DNDEBUG
release: test fullsum

test: test.cpp csvmonkey.hpp Makefile
	g++ -std=c++11 $(CXXFLAGS) -msse4.2 $(X) -g -o test test.cpp

fullsum: fullsum.cpp csvmonkey.hpp Makefile
	g++ -std=c++11 $(CXXFLAGS) -msse4.2 $(X) -g -o fullsum fullsum.cpp

clean:
	rm -f fullsum test cachegrind* perf.data* *.gcda

pgo: X+=-DNDEBUG
pgo:
	g++ -std=c++11 $(CXXFLAGS) -DNDEBUG -fprofile-generate -msse4.2 $(X) -g -o test test.cpp
	./test testdata/profiledata.csv
	g++ -std=c++11 $(CXXFLAGS) -DNDEBUG -fprofile-use -msse4.2 $(X) -g -o test test.cpp

grind:
	rm -f cachegrind.out.*
	valgrind --tool=cachegrind --branch-sim=yes ./test ram.64mb.csv
	cg_annotate --auto=yes cachegrind.out.*
