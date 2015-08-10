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
#include <stdlib.h>
#include <string.h>
#include "utility.h"

TEST_GROUP(UtilityTestGroup) {
};

TEST(UtilityTestGroup, ShifterTrim_NoTrim) {
    char *noNeedToTrim = strdup("This is a dense string.");
    size_t orig_len = strlen(noNeedToTrim);
    char *trimmed = shifter_trim(noNeedToTrim);
    CHECK(trimmed == noNeedToTrim); // pointer values match, no beginning change
    CHECK(orig_len == strlen(trimmed)); // lengths match
    CHECK(strcmp(trimmed, "This is a dense string.") == 0); // strings match
    free(noNeedToTrim);
}

TEST(UtilityTestGroup, ShifterTrim_Early) {
    char *earlyTrim = strdup(" \t   12345 Early, no whitespace end");
    char *trimmed = shifter_trim(earlyTrim);
    CHECK(strcmp(trimmed, "12345 Early, no whitespace end") == 0);
    free(earlyTrim);
}

TEST(UtilityTestGroup, ShifterTrim_Late) {
    char *lateTrim = strdup("NothingEarly, something late\n");
    char *trimmed = shifter_trim(lateTrim);
    CHECK(strcmp(trimmed, "NothingEarly, something late") == 0);
    free(lateTrim);
}

TEST(UtilityTestGroup, ShifterTrim_Both) {
    char *bothTrim = strdup(" 1 Early, 1 Late\n");
    char *trimmed = shifter_trim(bothTrim);
    CHECK(strcmp(trimmed, "1 Early, 1 Late") == 0);
    free(bothTrim);
}

TEST(UtilityTestGroup, ShifterTrim_BothSeveral) {
    char *bothTrim = strdup("\n\n\nabcdef    fedcba\t\t \n   ");
    char *trimmed = shifter_trim(bothTrim);
    CHECK(strcmp(trimmed, "abcdef    fedcba") == 0);
    free(bothTrim);
}

struct testConfig {
    char *first;
    int second;
    double third;
};

static int _assignTestConfig(char *key, char *value, void *_testConfig) {
    struct testConfig *config = (struct testConfig *) _testConfig;
    if (strcmp(key, "first") == 0) {
        config->first = strdup(value);
        return 0;
    }
    if (strcmp(key, "second") == 0) {
        config->second = atoi(value);
        return 0;
    }
    if (strcmp(key, "third") == 0) {
        config->third = atof(value);
        return 0;
    }
    return 1;
}

TEST(UtilityTestGroup, ShifterParseConfig_Basic) {
    const char *filename = "data_config1.conf";
    struct testConfig config;
    int ret = 0;
    memset(&config, 0, sizeof(struct testConfig));
   
    ret = shifter_parseConfig(filename, ':', &config, _assignTestConfig);
    CHECK(ret == 0);
    CHECK(config.second == 10);
    CHECK(config.third == 3.14159);
    CHECK(strcmp(config.first, "abcdefg") == 0);
    free(config.first);
}

TEST(UtilityTestGroup, ShifterParseConfig_InvalidKey) {
    const char *filename = "data_config2.conf";
    struct testConfig config;
    int ret = 0;
    memset(&config, 0, sizeof(struct testConfig));
   
    ret = shifter_parseConfig(filename, ':', &config, _assignTestConfig);
    CHECK(ret == 1);
    free(config.first);
}

TEST(UtilityTestGroup, ShifterParseConfig_MultiLine) {
    const char *filename = "data_config3.conf";
    struct testConfig config;
    int ret = 0;
    memset(&config, 0, sizeof(struct testConfig));
   
    ret = shifter_parseConfig(filename, ':', &config, _assignTestConfig);
    CHECK(ret == 0);
    CHECK(strcmp(config.first, "abcdefg qed feg") == 0);
    CHECK(config.second == 10);
    CHECK(config.third == 3.14159);
    free(config.first);
}

TEST(UtilityTestGroup, ShifterParseConfig_InvalidFilename) {
    const char *filename = "fake.conf";
    struct testConfig config;
    memset(&config, 0, sizeof(struct testConfig));
    int ret = 0;

    ret = shifter_parseConfig(filename, ':', &config, _assignTestConfig);
    CHECK(ret == 1);
    CHECK(config.first == NULL);
    CHECK(config.second == 0);
    CHECK(config.third == 0.0);
}

TEST(UtilityTestGroup, ShifterParseConfig_NullKey) {
    const char *filename = "data_config4.conf";
    struct testConfig config;
    memset(&config, 0, sizeof(struct testConfig));
    int ret = 0;

    ret = shifter_parseConfig(filename, ':', &config, _assignTestConfig);
    CHECK(ret == 1);
    CHECK(config.second == 10);
    CHECK(config.third == 3.14159);
    CHECK(strcmp(config.first, "abcdefg qed feg") == 0);
    free(config.first);
}

TEST(UtilityTestGroup, ShifterParseConfig_NullConfig) {
    const char *filename = "data_config4.conf";
    int ret = 0;

    ret = shifter_parseConfig(filename, ':', NULL, _assignTestConfig);
    CHECK(ret == 1);
}

int main(int argc, char** argv) {
        return CommandLineTestRunner::RunAllTests(argc, argv);
}
