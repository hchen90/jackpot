cmake_minimum_required(VERSION 2.6)
project(jackpot)

set(PACKAGE_NAME "jackpot")
set(PACKAGE_VERSION "1.2")
set(PACKAGE_URL "https://github.com/hchen90/jackpot.git")
set(PACKAGE_BUGREPORT "a.chenxiang.z@gmail.com")
configure_file(config.h.in config.h @ONLY)

add_definitions("-Wall -g")

set(DEPLIBS "-std=c++11")

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

set(CMAKE_INSTALL_BINDIR "/usr/bin")
install(TARGETS jackpot DESTINATION ${CMAKE_INSTALL_BINDIR})
