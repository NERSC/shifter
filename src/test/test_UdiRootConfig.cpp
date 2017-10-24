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
    CHECK(config.n_modules == 2);

    CHECK(strcmp(config.modules[0].name, "mpich") == 0);
    CHECK(strcmp(config.modules[0].userhook, "/path/to/userhook") == 0);
    CHECK(strcmp(config.modules[0].roothook, "/path/to/roothook") == 0);
    CHECK(strcmp(config.modules[0].siteEnvPrepend[0], "LD_LIBRARY_PATH=/opt/udiImage/mpich/lib64") == 0);
    CHECK(strcmp(config.modules[0].siteEnvPrepend[1], "PATH=/opt/udiImage/mpich/bin") == 0);
    CHECK(config.modules[0].siteEnvPrepend[2] == NULL);
    CHECK(strcmp(config.modules[0].siteEnvAppend[0], "PATH=/opt/udiImage/mpich/sbin") == 0);
    CHECK(config.modules[0].siteEnvAppend[1] == NULL);
    CHECK(strcmp(config.modules[0].siteEnv[0], "SHIFTER_MODULE_MPICH=1") == 0);
    CHECK(config.modules[0].siteEnv[1] == NULL);
    CHECK(strcmp(config.modules[0].siteEnvUnset[0], "FAKE_MPI_VARIABLE") == 0);
    CHECK(config.modules[0].siteEnvUnset[1] == NULL);
    CHECK(config.modules[0].siteFs != NULL);
    CHECK(strcmp(config.modules[0].copyPath, "/path/to/localized/mpich/stuff") == 0);
    CHECK(strcmp(config.modules[0].conflict_str[0], "openmpi") == 0);
    CHECK(config.modules[0].conflict_str[1] == NULL);
    CHECK(config.modules[0].conflict[0] == &(config.modules[1].conflict[1]));
    CHECK(config.modules[0].conflict[1] == NULL);

    CHECK(strcmp(config.modules[1].name, "openmpi") == 0);
    CHECK(strcmp(config.modules[1].userhook, "/path/to/openmpi_userhook") == 0);
    CHECK(config.modules[1].roothook == NULL);
    CHECK(strcmp(config.modules[1].siteEnvPrepend[0], "LD_LIBRARY_PATH=/opt/udiImage/openmpi/lib64") == 0);
    CHECK(strcmp(config.modules[1].siteEnvPrepend[1], "PATH=/opt/udiImage/openmpi/bin") == 0);
    CHECK(config.modules[1].siteEnvPrepend[2] == NULL);
    CHECK(strcmp(config.modules[1].siteEnvAppend[0], "PATH=/opt/udiImage/openmpi/sbin") == 0);
    CHECK(config.modules[1].siteEnvAppend[1] == NULL);
    CHECK(strcmp(config.modules[1].siteEnv[0], "SHIFTER_MODULE_OPENMPI=1") == 0);
    CHECK(config.modules[1].siteEnv[1] == NULL);
    CHECK(strcmp(config.modules[1].siteEnvUnset[0], "FAKE_MPI_VARIABLE") == 0);
    CHECK(config.modules[1].siteEnvUnset[1] == NULL);
    CHECK(config.modules[1].siteFs != NULL);
    CHECK(strcmp(config.modules[1].conflict_str[0], "mpich") == 0);
    CHECK(config.modules[1].conflict_str[1] == NULL);
    CHECK(config.modules[1].conflict[0] == &(config.modules[0]));
    CHECK(config.modules[1].conflict[1] == NULL);
    free_UdiRootConfig(&config, 0);
}

TEST(UdiRootConfigTestGroup, ParseUdiRootConfig_display) {
    UdiRootConfig config;
    memset(&config, 0, sizeof(UdiRootConfig));
    char buffer[4096];
    printf("about to parse\n");
    int ret = parse_UdiRootConfig("test_udiRoot.conf", &config, 0);
    CHECK(ret == 0);
    FILE *output = fopen("ParseUdiRootConfig_display.out", "w");
    CHECK(output != NULL);
    size_t nwrite = fprint_UdiRootConfig(output, &config);
    CHECK(nwrite > 0);
    fclose(output);
    FILE *input = fopen("ParseUdiRootConfig_display.out", "r");
    while (fread(buffer, 1, 4096, input) > 0) {
        fprintf(stderr, "%s", buffer);
    }
    fprintf(stderr, "\n");
    fclose(input);
    free_UdiRootConfig(&config, 0);
    unlink("ParseUdiRootConfig_display.out");
}

int main(int argc, char** argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
