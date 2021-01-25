cmake_minimum_required(VERSION 3.15)
project(BilibiliDanMu)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

# include_directories(/usr/local/include)

include_directories(poco)

include_directories(jsoncpp)
# link_directories(/usr/local/lib)
# link_libraries(libPocoNet.dylib)

# add_library(PocoNet)
add_subdirectory(poco)

# add_library(jsoncpp)
add_subdirectory(jsoncpp)

find_package(Poco REQUIRED Foundation Net Util XML JSON)
# find_package(Json REQUIRED jsoncpp)

add_executable(BilibiliDanMu main.cpp)
target_link_libraries(BilibiliDanMu PRIVATE
    Poco::Net
    Poco::Foundation
    libz.a
    jsoncpp_lib
    )
