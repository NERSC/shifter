/* Shifter, Copyright (c) 2015, The Regents of the University of California,
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
##     software without specific prior written permission.
## 
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
## AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
## LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
## CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
## SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
## INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
## CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
## ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
## POSSIBILITY OF SUCH DAMAGE.
##  
## You are under no obligation whatsoever to provide any bug fixes, patches, or
## upgrades to the features, functionality or performance of the source code
## ("Enhancements") to anyone; however, if you choose to make your Enhancements
## available either publicly, or directly to Lawrence Berkeley National
## Laboratory, without imposing a separate written license agreement for such
## Enhancements, then you hereby grant the following license: a  non-exclusive,
## royalty-free perpetual license to install, use, modify, prepare derivative
## works, incorporate into other computer software, distribute, and sublicense
## such enhancements or derivative works thereof, in binary and source code
## form.
*/

#include <CppUTest/CommandLineTestRunner.h>
#include "VolumeMap.h"
#include <stdio.h>

TEST_GROUP(VolumeMapTestGroup) {
};

TEST(VolumeMapTestGroup, VolumeMapParse_basic) {
    VolumeMap volMap;
    memset(&volMap, 0, sizeof(VolumeMap));

    int ret = parseVolumeMap("/path1/is/here:/target1", &volMap);
    CHECK(ret == 0);
    CHECK(volMap.n == 1);

    ret = parseVolumeMap("/evilPath:/etc", &volMap);
    CHECK(ret != 0);
    CHECK(volMap.n == 1);

    ret = parseVolumeMap("/scratch1/test:/input:ro", &volMap);
    CHECK(ret == 0);
    CHECK(volMap.n == 2);

    ret = parseVolumeMap("/scratch1/output:/output", &volMap);
    CHECK(ret == 0);
    CHECK(volMap.n == 3);

    free_VolumeMap(&volMap, 0);
}

TEST(VolumeMapTestGroup, ValidateVolumeMap_basic) {
    int ret = 0;

    ret = validateVolumeMap_userRequest("/test1Loc", "/etc/passwd", NULL);
    CHECK(ret != 0);

    ret = validateVolumeMap_userRequest("/test1Loc", "/var/log", NULL);
    CHECK(ret != 0);

    ret = validateVolumeMap_userRequest("/test1Loc", "/opt", NULL);
    CHECK(ret != 0);

    ret = validateVolumeMap_userRequest("/test1Loc", "opt", NULL);
    CHECK(ret != 0);

    ret = validateVolumeMap_userRequest("/test1Loc", "etc", NULL);
    CHECK(ret != 0);

    ret = validateVolumeMap_userRequest("/testLoc", "mnt", NULL);
    CHECK(ret == 0);

    ret = validateVolumeMap_userRequest("/test1Loc", "/opt/myStuff", NULL);
    CHECK(ret == 0);

    ret = validateVolumeMap_userRequest("/scratch1/data", "/input", NULL);
    CHECK(ret == 0);
}

TEST(VolumeMapTestGroup, GetVolumeMapSignature_basic) {
    int ret = 0;
    VolumeMap volMap;

    memset(&volMap, 0, sizeof(VolumeMap));
    ret = parseVolumeMap("/d:/c,/a:/b,/q:/r,/a:/c", &volMap);
    CHECK(ret == 0);
    CHECK(volMap.n == 4);

    char *sig = getVolMapSignature(&volMap);
    fprintf(stderr, "%s\n", sig);

    free(sig);


    free_VolumeMap(&volMap, 0);
}

int main(int argc, char** argv) {
        return CommandLineTestRunner::RunAllTests(argc, argv);
}
