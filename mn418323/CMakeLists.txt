# cmake_minimum_required (VERSION 3.13)
# project (CACTI)

# enable_testing()

# #set(CMAKE_C_STANDARD ...)
# set(CMAKE_C_FLAGS "-g -Wall -Wextra -pthread")

# # http://stackoverflow.com/questions/10555706/
# macro (add_executable _name)
#   # invoke built-in add_executable
#   _add_executable(${ARGV})
#   if (TARGET ${_name})
#     target_link_libraries(${_name} cacti)
#   endif()
# endmacro()

# add_library(cacti STATIC cacti.c queue.h queue.c)
# add_executable(macierz macierz.c)
# add_executable(silnia silnia.c)
# add_subdirectory(test)

# install(TARGETS cacti DESTINATION .)

# set_property(TARGET cacti PROPERTY C_STANDARD 11)

project(PW_C)
cmake_minimum_required (VERSION 3.13)

set(CMAKE_CXX_COMPILER "gcc")

set(CMAKE_CXX_STANDARD 11)

add_compile_options(-g -Wall -Wextra -pthread)

add_executable(test_q main.c queue.h queue.c cacti.c cacti.h)
