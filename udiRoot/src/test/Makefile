CPPUTEST_FLAGS=-I../../cpputest/include ../../cpputest/lib/libCppUTest.a
test: test_utility
	./test_utility

test_utility:
	g++ test_utility.cpp -I.. ../utility.c $(CPPUTEST_FLAGS) -o test_utility

clean:
	if [[ -e test_utility ]]; then rm test_utility; fi
