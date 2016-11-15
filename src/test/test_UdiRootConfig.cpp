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

#include <stdlib.h>
#include "UdiRootConfig.h"
#include "utility.h"
#include <CppUTest/CommandLineTestRunner.h>

TEST_GROUP(UdiRootConfigTestGroup) {
};

TEST(UdiRootConfigTestGroup, ParseUdiRootConfig_basic) {
    UdiRootConfig config;

    memset(&config, 0, sizeof(UdiRootConfig));
    int ret = parse_UdiRootConfig("test_udiRoot.conf", &config, 0);
    printf("value: %d\n", ret);
    CHECK(ret == 0);
    CHECK(strcmp(config.udiMountPoint, "/var/udiMount") == 0);
    CHECK(strcmp(config.loopMountPoint, "/var/loopUdiMount") == 0);
    CHECK(strcmp(config.rootfsType, "tmpfs") == 0);
    CHECK(strcmp(config.system, "testSystem") == 0);
    CHECK(strcmp(config.gpuBinPath, "/site-resources/gpu/bin") == 0);
    CHECK(strcmp(config.gpuLibPath, "/site-resources/gpu/lib") == 0);
    CHECK(strcmp(config.gpuLib64Path, "/site-resources/gpu/lib64") == 0);
    free_UdiRootConfig(&config, 0);
}

TEST(UdiRootConfigTestGroup, ParseUdiRootConfig_display) {
    UdiRootConfig config;
    memset(&config, 0, sizeof(UdiRootConfig));
    printf("about to parse\n");
    int ret = parse_UdiRootConfig("test_udiRoot.conf", &config, 0);
    CHECK(ret == 0);
    FILE *output = fopen("ParseUdiRootConfig_display.out", "w");
    CHECK(output != NULL);
    size_t nwrite = fprint_UdiRootConfig(output, &config);
    CHECK(nwrite > 0);
    fclose(output);
    free_UdiRootConfig(&config, 0);
    unlink("ParseUdiRootConfig_display.out");
}

int main(int argc, char** argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
