
CXXFLAGS += -Iinclude
CXXFLAGS += -Ithird_party

CXXFLAGS += -std=c++11
CXXFLAGS += -O3
CXXFLAGS += -Wall
CXXFLAGS += -lc
#CXXFLAGS += -DUSE_SPIRIT

default: debug python

python:
	rm -rf build
	python setup.py build_ext --inplace

debug: CXXFLAGS+=-O0 -g
debug: tests/bench/iteration

release: X=-DNDEBUG
release: tests/bench/iteration tests/fullsum

tests/bench/iteration: tests/bench/iteration.cpp include/csvmonkey.hpp Makefile
	g++ -std=c++11 $(CXXFLAGS) -msse4.2 $(X) -g -o tests/bench/iteration tests/bench/iteration.cpp

tests/fullsum: tests/fullsum.cpp include/csvmonkey.hpp Makefile
	g++ -std=c++11 $(CXXFLAGS) -msse4.2 $(X) -g -o tests/fullsum tests/fullsum.cpp

clean:
	rm -f tests/fullsum tests/bench/iteration cachegrind* perf.data* *.gcda

pgo: X+=-DNDEBUG
pgo:
	g++ -std=c++11 $(CXXFLAGS) -DNDEBUG -fprofile-generate -msse4.2 $(X) -g -o tests/bench/iteration tests/bench/iteration.cpp
	./tests/bench/iteration tests/data/profiledata.csv
	g++ -std=c++11 $(CXXFLAGS) -DNDEBUG -fprofile-use -msse4.2 $(X) -g -o tests/bench/iteration tests/bench/iteration.cpp

grind:
	rm -f cachegrind.out.*
	valgrind --tool=cachegrind --branch-sim=yes ./tests/bench/iteration ram.64mb.csv
	cg_annotate --auto=yes cachegrind.out.*
