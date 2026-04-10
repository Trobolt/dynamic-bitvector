cmake_minimum_required(VERSION 3.16)
project(ADS)

SET(GCC_COVERAGE_COMPILE_FLAGS "-m64 -mbmi2 -O3 -Wimplicit-fallthrough -Wno-all")#-Winline -Wall -Wextra")#-fprofile-arcs -ftest-coverage -fsanitize=address")
#SET(GCC_COVERAGE_LINK_FLAGS "-lgcov")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")
set(CMAKE_CXX_STANDARD 17)
set(TESTFILE_FOLDER ${CMAKE_SOURCE_DIR}/testfiles)
set(TESTGENERATOR_FOLDER ${CMAKE_SOURCE_DIR}/testgenerators)


file(MAKE_DIRECTORY ${TESTFILE_FOLDER})
file(MAKE_DIRECTORY ${TESTGENERATOR_FOLDER})

add_executable(TestGenerator GenerateTests.cpp)
add_executable(ADS main.cpp)
#add_executable(Tester Tester/test1.cpp)
add_executable(RealTester DynamicBitVector/Test.cpp)
add_executable(Data DynamicBitVector/Data.cpp)