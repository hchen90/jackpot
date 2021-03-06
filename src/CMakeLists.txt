cmake_minimum_required(VERSION 3.4)
project(jackpot)

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.8)
  message(FATAL_ERROR "GCC version is too old, 4.8 or higher required")
endif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.8)

option(USE_SMARTPOINTER "Use smart pointer (shared_ptr)" OFF)

set(PACKAGE_NAME "jackpot")
set(PACKAGE_VERSION "1.4.1")
set(PACKAGE_URL "https://github.com/hchen90/jackpot.git")
set(PACKAGE_BUGREPORT "a.chenxiang.z@gmail.com")
set(IFCONFIG_PATH "/usr/bin/ifconfig")
set(IPROUTE_PATH "/usr/bin/ip")
set(ROUTE_PATH "/usr/bin/route")
configure_file(config.h.in config.h @ONLY)

if(${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
  include_directories("/usr/local/include")
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  include_directories("/usr/local/Cellar/openssl\@1.1/1.1.1k/include" "/usr/local/Cellar/libev/4.33/include")
endif()

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
  add_definitions("-std=c++11 -Wall")
else()
  add_definitions("-std=c++11 -Wall -rdynamic")
endif()

set(DEPLIBS "-pthread")

function(check_library LiBnM FaTaL)
  find_library(_${LiBnM}_ NAMES ${LiBnM})
  if(_${LiBnM}_)
    message(STATUS "Searching library: ${LiBnM} - done")
    string(APPEND DEPLIBS " -l${LiBnM}")
    set(DEPLIBS ${DEPLIBS} PARENT_SCOPE)
  else(_${LiBnM}_)
    message(${FaTaL} "Searching library: ${LiBnM} - not found")
  endif(_${LiBnM}_)
endfunction(check_library)

check_library(ev FATAL_ERROR)
check_library(ssl FATAL_ERROR)
check_library(crypto FATAL_ERROR)

file(GLOB SRCFILES "*.cpp")
file(GLOB HDRFILES "*.h")
add_executable(jackpot ${SRCFILES} ${HDRFILES})
target_link_libraries(jackpot ${DEPLIBS})

if(NOT ${CMAKE_VERSION} VERSION_LESS 3.13)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    target_link_directories(jackpot PUBLIC "/usr/local/lib")
  elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    target_link_directories(jackpot PUBLIC "/usr/local/Cellar/openssl\@1.1/1.1.1k/lib" "/usr/local/Cellar/libev/4.33/lib")
  endif()
endif()

set(CMAKE_INSTALL_BINDIR "/usr/bin")
install(TARGETS jackpot DESTINATION ${CMAKE_INSTALL_BINDIR})
