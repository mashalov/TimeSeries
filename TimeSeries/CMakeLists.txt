cmake_minimum_required (VERSION 3.8)
project ("TimeSeries")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(ExternalProject)

# ExternalProject_Add(fmtlib GIT_REPOSITORY https://github.com/fmtlib/fmt.git)


add_compile_definitions(TIMESERIES_TEST_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
set(FMT_INCLUDES "../fmt/include")
set(FMT_SOURCES  "../fmt/src/format.cc" "../fmt/src/os.cc")

set(INCLUDES ${FMT_INCLUDES})


set (SOURCES  
	Tests.cpp
	TimeSeries.cpp
	${FMT_SOURCES}

)

add_executable(TimeSeriesTest ${SOURCES})

target_include_directories(TimeSeriesTest PRIVATE  ${INCLUDES})
if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    target_link_libraries(TimeSeriesTest stdc++fs)
endif()



