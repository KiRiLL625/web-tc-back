cmake_minimum_required(VERSION 3.27)
project(web_tc_back)

set(CMAKE_CXX_STANDARD 17)

find_package(Crow REQUIRED)
find_package(CURL REQUIRED)
find_package(jsoncpp REQUIRED)

include_directories(${CURL_INCLUDE_DIRS} ${JSONCPP_INCLUDE_DIRS} ${Crow_INCLUDE_DIRS})

add_executable(web_tc_back main.cpp)
target_link_libraries(web_tc_back Crow::Crow ${CURL_LIBRARIES} jsoncpp_lib)
