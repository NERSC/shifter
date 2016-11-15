/**
 *  @file shifter.c
 *  @brief setuid utility to setup and interactively enter a shifter env
 * 
 * @author Douglas M. Jacobsen <dmjacobsen@lbl.gov>
 */

/* Shifter, Copyright (c) 2015, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of any
 * required approvals from the U.S. Dept. of Energy).  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. Neither the name of the University of California, Lawrence Berkeley
 *     National Laboratory, U.S. Dept. of Energy nor the names of its
 *     contributors may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 * 
 * See LICENSE for full text.
 */

#include "UdiRootConfig.h"
#include "shifter_core.h"
#include "ImageData.h"
#include "utility.h"
#include "VolumeMap.h"

#include <CppUTest/CommandLineTestRunner.h>

#include "shifter.c"

extern char** environ;

TEST_GROUP(ShifterTestGroup) {
};

TEST(ShifterTestGroup, CopyEnv_basic) {
    MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
    setenv("TESTENV0", "gfedcba", 1);
    char **origEnv = shifter_copyenv(environ, 0);
    CHECK(origEnv != NULL);
    clearenv();
    setenv("TESTENV1", "abcdefg", 1);
    CHECK(getenv("TESTENV0") == NULL);
    CHECK(getenv("TESTENV1") != NULL);
    for (char **ptr = origEnv; *ptr != NULL; ptr++) {
        putenv(*ptr);
        /* not free'ing *ptr, since *ptr is becoming part of the environment
           it is owned by environ now */
    }
    free(origEnv);
    CHECK(getenv("TESTENV0") != NULL);
    CHECK(strcmp(getenv("TESTENV0"), "gfedcba") == 0);
    MemoryLeakWarningPlugin::turnOnNewDeleteOverloads();
}

TEST(ShifterTestGroup, adoptPATH_test) {

    CHECK(adoptPATH(NULL) != 0);

    char **tmpenv = (char **) malloc(sizeof(char *) * 4);
    tmpenv[0] = strdup("fakeEnv=1");
    tmpenv[1] = strdup("LD_LIBRARY_PATH=/test");
    tmpenv[2] = strdup("PATH=/usr/bin:/usr/sbin:/fakePath");
    tmpenv[3] = NULL;
    char *savepath = strdup(getenv("PATH"));

    CHECK(adoptPATH(tmpenv) == 0);
    CHECK(strcmp(getenv("PATH"), "/usr/bin:/usr/sbin:/fakePath") == 0);

    setenv("PATH", savepath, 1);
    free(savepath);
    free(tmpenv[0]);
    free(tmpenv[1]);
    free(tmpenv[2]);
    free(tmpenv);
}

TEST(ShifterTestGroup, parseGPUenv_test)
{
    struct options opts;
    memset(&opts, 0, sizeof(struct options));

    opts.gpu_ids = strdup("0");
    setenv("CUDA_VISIBLE_DEVICES", "0,1", 1);

    parse_gpu_env(&opts);

    CHECK (strcmp(opts.gpu_ids, "0,1") == 0);
}


#if 0
TEST(ShifterTestGroup, LocalPutEnv_basic) {
    setenv("TESTENV0", "qwerty123", 1);
    unsetenv("TESTENV2");
    char **altEnv = copyenv();
    CHECK(altEnv != NULL);
    char *testenv0Ptr = NULL;
    char *testenv2Ptr = NULL;
    char **ptr = NULL;
    int nEnvVar = 0;
    int nEnvVar2 = 0;
    for (ptr = altEnv; *ptr != NULL; ptr++) {
        if (strncmp(*ptr, "TESTENV0", 8) == 0) {
            testenv0Ptr = *ptr;
        }
        nEnvVar++;
    }
    CHECK(testenv0Ptr != NULL);
    CHECK(strcmp(testenv0Ptr, "TESTENV0=qwerty123") == 0);

    int ret = local_putenv(&altEnv, "TESTENV0=abcdefg321");
    CHECK(ret == 0);
    ret = local_putenv(&altEnv, "TESTENV2=asdfghjkl;");
    CHECK(ret == 0);
    ret = local_putenv(&altEnv, NULL);
    CHECK(ret != 0);
    ret = local_putenv(NULL, "TESTENV2=qwerty123");
    CHECK(ret != 0);

    for (ptr = altEnv; *ptr != NULL; ptr++) {
        if (strncmp(*ptr, "TESTENV0", 8) == 0) {
            testenv0Ptr = *ptr;
        } else if (strncmp(*ptr, "TESTENV2", 8) == 0) {
            testenv2Ptr = *ptr;
        } else {
            free(*ptr);
        }
        nEnvVar2++;
    }
    free(altEnv);
    CHECK(testenv0Ptr != NULL);
    CHECK(testenv2Ptr != NULL);
    CHECK(nEnvVar2 - nEnvVar == 1);
    CHECK(strcmp(testenv0Ptr, "TESTENV0=abcdefg321") == 0);
    CHECK(strcmp(testenv2Ptr, "TESTENV2=asdfghjkl;") == 0);

    free(testenv0Ptr);
    free(testenv2Ptr);
}
#endif

int main(int argc, char **argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
