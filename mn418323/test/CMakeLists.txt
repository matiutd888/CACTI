include_directories(..)

add_executable(test_empty test_empty.c)
add_test(test_empty test_empty)

add_executable(test_janek actor_system_test.c)
add_test(test_janek test_janek)

add_executable(simple simple_test.c)
add_test(simple simple)

add_executable(test_deadlock test_deadlock.c)
add_test(test_deadlock test_deadlock)

add_executable(test_queue queue.c)
add_test(test_queue test_queue)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -pthread")
add_executable(chaos chaos.cpp)

set_tests_properties(test_empty PROPERTIES TIMEOUT 1)
