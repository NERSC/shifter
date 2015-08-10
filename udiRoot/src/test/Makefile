CPPUTEST_FLAGS=-I../../cpputest/include ../../cpputest/lib/libCppUTest.a
test: test_utility test_UdiRootConfig

test_utility: test_utility.cpp ../utility.h ../utility.c
	g++ -fprofile-arcs -ftest-coverage -O0 test_utility.cpp -I.. ../utility.c $(CPPUTEST_FLAGS) -o test_utility
	./test_utility
	gcov -b ../utility.c
	cat utility.c.gcov | grep "#####:"

test_UdiRootConfig: test_UdiRootConfig.cpp ../UdiRootConfig.h ../UdiRootConfig.c ../utility.c
	g++ -fprofile-arcs -ftest-coverage -O0 test_UdiRootConfig.cpp -I.. ../UdiRootConfig.c ../utility.c $(CPPUTEST_FLAGS) -o test_UdiRootConfig -DNO_ROOT_OWN_CHECK=1
	./test_UdiRootConfig
	gcov -b ../UdiRootConfig.c
	cat UdiRootConfig.c.gcov | grep "#####:"

clean:
	if [[ -e test_utility ]]; then rm test_utility; fi
	if [[ -e test_UdiRootConfig ]]; then rm test_UdiRootConfig; fi
	rm *.gcov *.gcda *.gcno

