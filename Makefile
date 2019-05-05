
CXXFLAGS += -std=c++11
CXXFLAGS += -O3
CXXFLAGS += -Wall
CXXFLAGS += -lc
#CXXFLAGS += -DUSE_SPIRIT

debug: CXXFLAGS+=-O0 -g
debug: bench/iteration

release: X=-DNDEBUG
release: bench/iteration tests/fullsum

bench/iteration: bench/iteration.cpp csvmonkey.hpp Makefile
	g++ -std=c++11 $(CXXFLAGS) -msse4.2 $(X) -g -o bench/iteration bench/iteration.cpp

tests/fullsum: tests/fullsum.cpp csvmonkey.hpp Makefile
	g++ -std=c++11 $(CXXFLAGS) -msse4.2 $(X) -g -o tests/fullsum tests/fullsum.cpp

clean:
	rm -f tests/fullsum bench/iteration cachegrind* perf.data* *.gcda

pgo: X+=-DNDEBUG
pgo:
	g++ -std=c++11 $(CXXFLAGS) -DNDEBUG -fprofile-generate -msse4.2 $(X) -g -o bench/iteration bench/iteration.cpp
	./bench/iteration tests/data/profiledata.csv
	g++ -std=c++11 $(CXXFLAGS) -DNDEBUG -fprofile-use -msse4.2 $(X) -g -o bench/iteration bench/iteration.cpp

grind:
	rm -f cachegrind.out.*
	valgrind --tool=cachegrind --branch-sim=yes ./bench/iteration ram.64mb.csv
	cg_annotate --auto=yes cachegrind.out.*
