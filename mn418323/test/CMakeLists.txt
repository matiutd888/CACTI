include_directories(..)

add_executable(test_empty test_empty.c)
add_test(test_empty test_empty)

add_executable(test_janek actor_system_test.c)
add_test(test_janek test_janek)

add_executable(simple simple_test.c)
add_test(simple simple)

set_tests_properties(test_empty PROPERTIES TIMEOUT 1)
