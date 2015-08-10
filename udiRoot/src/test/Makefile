CPPUTEST_FLAGS=-I../../cpputest/include ../../cpputest/lib/libCppUTest.a
test: test_utility
	./test_utility
	gcov -b ../utility.c
	cat utility.c.gcov | grep "#####:"

test_utility: test_utility.cpp
	g++ -fprofile-arcs -ftest-coverage -O0 test_utility.cpp -I.. ../utility.c $(CPPUTEST_FLAGS) -o test_utility

clean:
	if [[ -e test_utility ]]; then rm test_utility; fi
