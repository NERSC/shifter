CPPUTEST_FLAGS=-I../../cpputest/include ../../cpputest/lib/libCppUTest.a
TEST_FLAGS=-DNO_ROOT_OWN_CHECK=1 -DCONFIG_FILE=\"$(PWD)/udiRoot.conf.test\"


test: test_utility test_UdiRootConfig test_shifter

test_utility: ../utility.h ../utility.c
	g++ -fprofile-arcs -ftest-coverage -O0 -I.. ../utility.c $(CPPUTEST_FLAGS) $(TEST_FLAGS) -D _TESTHARNESS_UTILITY -o test_utility
	./test_utility
	gcov -b ../utility.c
	cat utility.c.gcov | grep "#####:"

test_UdiRootConfig: ../UdiRootConfig.h ../UdiRootConfig.c ../utility.c
	g++ -fprofile-arcs -ftest-coverage -O0 -I.. ../UdiRootConfig.c ../utility.c $(CPPUTEST_FLAGS) $(TEST_FLAGS) -D_TESTHARNESS_UDIROOTCONFIG -o test_UdiRootConfig
	./test_UdiRootConfig
	gcov -b ../UdiRootConfig.c
	cat UdiRootConfig.c.gcov | grep "#####:"

test_shifter: ../shifter.c ../shifter_core.c ../shifter_core.h ../UdiRootConfig.c ../UdiRootConfig.h ../utility.h ../utility.c ../ImageData.h ../ImageData.c
	g++ -fprofile-arcs -ftest-coverage -O0 -I.. ../shifter.c ../shifter_core.c ../UdiRootConfig.c ../utility.c ../ImageData.c $(CPPUTEST_FLAGS) -o test_shifter $(TEST_FLAGS) -D_TESTHARNESS_SHIFTER
	./test_shifter
	gcov -b ../shifter.c
	cat shifter.c.gcov | grep "#####:"

clean:
	if [[ -e test_utility ]]; then rm test_utility; fi
	if [[ -e test_UdiRootConfig ]]; then rm test_UdiRootConfig; fi
	if [[ -e test_shifter ]]; then rm test_shifter; fi
	rm *.gcov *.gcda *.gcno

