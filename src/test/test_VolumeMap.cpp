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

void freeTokens(char **tokens) {
    char **tk_ptr = NULL;
    for (tk_ptr = tokens; tk_ptr && *tk_ptr; tk_ptr++) {
        free(*tk_ptr);
    }
    free(tokens);
}

TEST(VolumeMapTestGroup, FindEndOfVolumeMapInString) {
    const char *input1 = "/volume:/map:test";
    const char *ptr = _findEndVolumeMapString(input1);
    CHECK(ptr != NULL && *ptr == 0);
    CHECK(strncmp(input1, input1, ptr-input1) == 0);

    const char *input2 = "/volume:/map;/input:output";
    ptr = _findEndVolumeMapString(input2);
    CHECK(ptr != NULL && *ptr == ';');
    CHECK(strncmp(input2, "/volume:/map", ptr-input2) == 0);

    const char *sptr = input2;
    const char *limit = input2 + strlen(input2);
    int idx = 0;
    for (sptr = input2, ptr = _findEndVolumeMapString(input2); sptr < limit; ptr = _findEndVolumeMapString(sptr)) {
        switch (idx) {
            case 0:     CHECK(strncmp(sptr, "/volume:/map", ptr - sptr) == 0);
                        break;
            case 1:     CHECK(strncmp(sptr, "/input:output", ptr - sptr) == 0);
                        break;
        }
        sptr = ptr + 1;
        idx++;
    }

    const char *input3 = "\"/volume:/map:ro,rec\"";
    ptr = _findEndVolumeMapString(input3);
    CHECK(ptr && (*ptr == 0));
    CHECK(strlen(input3) == ptr - input3);
}

TEST(VolumeMapTestGroup, VolumeMapTokenize) {

    char *input = strdup("abcd:efgh");
    char **tokens = _tokenizeVolumeMapInput(input);
    size_t ntokens = 0;
    for ( ; tokens && tokens[ntokens]; ntokens++) { }

    CHECK(tokens != NULL && ntokens == 2);
    CHECK(strcmp(tokens[0], "abcd") == 0);
    CHECK(strcmp(tokens[1], "efgh") == 0);

    freeTokens(tokens);
    free(input);

    input = strdup("abcd:efgh:flag");
    tokens = _tokenizeVolumeMapInput(input);
    for (ntokens = 0 ; tokens && tokens[ntokens]; ntokens++) { }
    CHECK(tokens != NULL && ntokens == 3);
    CHECK(strcmp(tokens[0], "abcd") == 0);
    CHECK(strcmp(tokens[1], "efgh") == 0);
    CHECK(strcmp(tokens[2], "flag") == 0);

    freeTokens(tokens);
    free(input);

    input = strdup("abcd:efgh:flag:extra");
    tokens = _tokenizeVolumeMapInput(input);
    for (ntokens = 0 ; tokens && tokens[ntokens]; ntokens++) { }
    CHECK(tokens != NULL && ntokens == 4);
    CHECK(strcmp(tokens[0], "abcd") == 0);
    CHECK(strcmp(tokens[1], "efgh") == 0);
    CHECK(strcmp(tokens[2], "flag") == 0);
    CHECK(strcmp(tokens[3], "extra") == 0);

    freeTokens(tokens);
    free(input);

    input = strdup("no delim test");
    tokens = _tokenizeVolumeMapInput(input);
    for (ntokens = 0 ; tokens && tokens[ntokens]; ntokens++) { }
    //fprintf(stderr," got %lu tokens\n0: %s\n1: %s\n2: %s\n", ntokens, tokens[0], tokens[1], tokens[2]);
    CHECK(tokens != NULL && ntokens == 1);
    CHECK(strcmp(tokens[0], "no delim test") == 0);

    freeTokens(tokens);
    free(input);

}

TEST(VolumeMapTestGroup, VolumeMapParseBytes) {
    ssize_t parsedVal = parseBytes("5");
    CHECK(parsedVal == 5);

    parsedVal = parseBytes("5k");
    CHECK(parsedVal = 5 * 1024);

    parsedVal = parseBytes("500K");
    CHECK(parsedVal == 500 * 1024);

    parsedVal = parseBytes("500KB");
    CHECK(parsedVal == 500 * 1024);
}

TEST(VolumeMapTestGroup, VolumeMapParseFlag) {
    char *flag = strdup("ro");
    VolumeMapFlag *flags = NULL;
    size_t flagsCapacity = 0;

    int ret = _parseFlag(flag, &flags, &flagsCapacity);
    CHECK(ret == 0);
    CHECK(flagsCapacity == 1);
    CHECK(flags[0].type == VOLMAP_FLAG_READONLY);
    free(flag);

    flag = strdup("rec");
    ret = _parseFlag(flag, &flags, &flagsCapacity);
    CHECK(ret == 0);
    CHECK(flagsCapacity == 2);
    CHECK(flags[0].type == VOLMAP_FLAG_READONLY);
    CHECK(flags[1].type == VOLMAP_FLAG_RECURSIVE);
    free(flag);

    flag = strdup("rec=key1=value1,key2=value2");
    ret = _parseFlag(flag, &flags, &flagsCapacity);
    CHECK(ret != 0);
    CHECK(flagsCapacity == 2);
    CHECK(flags[0].type == VOLMAP_FLAG_READONLY);
    CHECK(flags[1].type == VOLMAP_FLAG_RECURSIVE);
    free(flag);

    flag = strdup("perNodeCache");
    ret = _parseFlag(flag, &flags, &flagsCapacity);
    CHECK(ret != 0); /* no default size */
    free(flag);

    flag = strdup("perNodeCache=size=4T,bs=4M");
    ret = _parseFlag(flag, &flags, &flagsCapacity);
    CHECK(ret == 0);
    CHECK(flagsCapacity == 3);
    CHECK(flags[0].type == VOLMAP_FLAG_READONLY);
    CHECK(flags[1].type == VOLMAP_FLAG_RECURSIVE);
    CHECK(flags[2].type == VOLMAP_FLAG_PERNODECACHE);
    free(flag);

    free_VolumeMapFlag(flags, 1);

}

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

    ret = parseVolumeMap("/scratch1/test:/input:rec", &volMap);
    CHECK(ret != 0);
    CHECK(volMap.n == 2);

    ret = parseVolumeMap("/scratch1/output:/output", &volMap);
    CHECK(ret == 0);
    CHECK(volMap.n == 3);

    ret = parseVolumeMap("/no/to/specified", &volMap);
    CHECK(ret != 0);
    CHECK(volMap.n == 3);

    char tempfname[] = "checkprint.XXXXXX";
    int tempfd = mkstemp(tempfname);
    FILE *tempfp = fdopen(tempfd, "w");
    size_t nbytes = fprint_VolumeMap(tempfp, &volMap);
    CHECK(nbytes == 167);
    fclose(tempfp);

    free_VolumeMap(&volMap, 0);
}

TEST(VolumeMapTestGroup, VolumeMapParse_site) {
    VolumeMap volMap;
    memset(&volMap, 0, sizeof(VolumeMap));

    int ret = parseVolumeMapSiteFs("/global/cscratch1", &volMap);
    CHECK(ret == 0);
    CHECK(volMap.n == 1);

    ret = parseVolumeMapSiteFs("/global/cscratch1:/global/cscratch1:ro", &volMap);
    CHECK(ret == 0);
    CHECK(volMap.n == 2);

    ret = parseVolumeMapSiteFs("/global/cscratch1:/global/cscratch1:rec", &volMap);
    CHECK(ret == 0);
    CHECK(volMap.n == 3);

    fprint_VolumeMap(stderr, &volMap);

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
    ret = parseVolumeMapSiteFs("/d:/c;/a:/b;/q:/r;/a:/c:rec:perNodeCache=size=100M:ro", &volMap);
    CHECK(ret == 0);
    CHECK(volMap.n == 4);

    char *sig = getVolMapSignature(&volMap);
    fprintf(stderr, "%s\n", sig);

    CHECK(strcmp(sig, "/a:/b;/a:/c:ro:rec:perNodeCache=size=104857600,bs=1048576,method=loop,fstype=xfs;/d:/c;/q:/r") == 0);

    free(sig);


    free_VolumeMap(&volMap, 0);
}

int main(int argc, char** argv) {
        return CommandLineTestRunner::RunAllTests(argc, argv);
}
