#!/bin/bash
set -ex

## Shifter, Copyright (c) 2015, The Regents of the University of California,
## through Lawrence Berkeley National Laboratory (subject to receipt of any
## required approvals from the U.S. Dept. of Energy).  All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
##  1. Redistributions of source code must retain the above copyright notice,
##     this list of conditions and the following disclaimer.
##  2. Redistributions in binary form must reproduce the above copyright notice,
##     this list of conditions and the following disclaimer in the documentation
##     and/or other materials provided with the distribution.
##  3. Neither the name of the University of California, Lawrence Berkeley
##     National Laboratory, U.S. Dept. of Energy nor the names of its
##     contributors may be used to endorse or promote products derived from this
##     software without specific prior written permission.`
##
## See LICENSE for full text.

function runTest() {
   test=$1
   source=$2
   sources="$test-$source"
   timeout 90 ./$test -v
   if [[ "x$DO_ROOT_TESTS" == "x1" && -x ${test}_AsRoot ]]; then
      sudo timeout 90 ./${test}_AsRoot
      sources="$sources ${test}_AsRoot-$source"
   fi
   if [[ "x$DO_ROOT_DANGEROUS_TESTS" == "x1" && -x ${test}_AsRootDangerous ]]; then
      sudo timeout 90 ./${test}_AsRootDangerous
      sources="$sources ${test}_AsRootDangerous-$source"
   fi
   valgrind --tool=memcheck --leak-check=full --suppressions=valgrind.suppressions -v ./$test
   gcov -b $sources 
}

runTest test_ImageData ImageData
runTest test_MountList MountList
runTest test_UdiRootConfig UdiRootConfig
runTest test_shifter_core shifter_core
runTest test_shifter shifter
runTest test_utility utility
runTest test_VolumeMap VolumeMap

