CPPUTEST_FLAGS=-I../../cpputest/include ../../cpputest/lib/libCppUTest.a
TEST_FLAGS=-DNO_ROOT_OWN_CHECK=1 -DCONFIG_FILE=\"$(PWD)/udiRoot.conf.test\"
USE_GCOV=0
USE_VALGRIND=1

UID=$(shell id -u)
ifneq ($(UID),0)
	TEST_FLAGS += -DNOTROOT
endif

test: test_utility test_VolumeMap test_UdiRootConfig test_shifter test_shifter_core

define RUNTEST =
timeout 30 ./$@ -v
if [[ $(USE_GCOV) == 1 ]]; then \
	gcov -b $(firstword $^); \
	cat $(firstword $^).gcov | grep "#####:"; \
fi
if [[ $(USE_VALGRIND) == 1 ]]; then \
	valgrind --tool=memcheck --leak-check=full --suppressions=valgrind.suppressions ./$@ ; \
fi
endef

test_utility: ../utility.h ../utility.c
	g++ -fprofile-arcs -ftest-coverage -O0 -I.. ../utility.c $(CPPUTEST_FLAGS) $(TEST_FLAGS) -D _TESTHARNESS_UTILITY -o test_utility
	$(RUNTEST)

test_VolumeMap: ../VolumeMap.c ../utility.c ../shifter_core.c ../MountList.c
	g++ -fprofile-arcs -ftest-coverage -O0 -I.. $^ $(CPPUTEST_FLAGS) $(TEST_FLAGS) -D _TESTHARNESS_VOLMAP -o $@
	$(RUNTEST)

test_UdiRootConfig: ../UdiRootConfig.c ../utility.c
	g++ -fprofile-arcs -ftest-coverage -O0 -I.. $^ $(CPPUTEST_FLAGS) $(TEST_FLAGS) -D_TESTHARNESS_UDIROOTCONFIG -o $@
	$(RUNTEST)

test_shifter: ../shifter.c ../shifter_core.c ../UdiRootConfig.c ../utility.c ../ImageData.c ../VolumeMap.c ../MountList.c
	g++ -fprofile-arcs -ftest-coverage -O0 -I.. $^ $(CPPUTEST_FLAGS) -o $@ $(TEST_FLAGS) -D_TESTHARNESS_SHIFTER
	$(RUNTEST)

test_ImageData: ../ImageData.c ../utility.c
	g++ -fprofile-arcs -ftest-coverage -O0 -I.. $^ $(CPPUTEST_FLAGS) -o $@ $(TEST_FLAGS) -D_TESTHARNESS_IMAGEDATA
	$(RUNTEST)

test_shifter_core: ../shifter_core.c ../utility.c ../UdiRootConfig.c ../VolumeMap.c ../MountList.c
	g++ -fprofile-arcs -ftest-coverage -O0 -I.. $^ $(CPPUTEST_FLAGS) -o $@ $(TEST_FLAGS) -D_TESTHARNESS_SHIFTERCORE
	$(RUNTEST)

test_MountList: ../MountList.c ../utility.c
	g++ -fprofile-arcs -ftest-coverage -O0 -I.. $^ $(CPPUTEST_FLAGS) -o $@ $(TEST_FLAGS) -D_TESTHARNESS_MOUNTLIST
	$(RUNTEST)

clean:
	if [[ -e test_utility ]]; then rm test_utility; fi
	if [[ -e test_UdiRootConfig ]]; then rm test_UdiRootConfig; fi
	if [[ -e test_shifter ]]; then rm test_shifter; fi
	if [[ -e test_VolumeMap ]]; then rm test_VolumeMap; fi
	if [[ -e test_shifter_core ]]; then rm test_shifter_core; fi
	if [[ -e test_MountList ]]; then rm test_MountList; fi
	rm *.gcov *.gcda *.gcno

