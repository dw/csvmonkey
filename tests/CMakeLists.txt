cmake_minimum_required(VERSION 3.11)

#SET_SOURCE_FILES_PROPERTIES( nosse_stringspanner_test.cpp PROPERTIES COMPILE_FLAGS -Wderp )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wunused -msse4.2")


add_executable(main
    main.cpp
    sse42_stringspanner_test.cpp
    fallback_stringspanner_test.cpp
)

set_property(TARGET main PROPERTY CXX_STANDARD 11)
