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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "VolumeMap.h"
#include "utility.h"

static int _validateVolumeMap(const char *to, const char *from, const char *flags);

int parseVolumeMap(const char *input, struct VolumeMap *volMap) {
    if (input == NULL || volMap == NULL) return 1;
    char **rawPtr = volMap->raw + volMap->n;
    char **toPtr = volMap->to + volMap->n;
    char **fromPtr = volMap->from + volMap->n;
    char **flagsPtr = volMap->flags + volMap->n;

    const char *ptr = input;
    const char *eptr = NULL;
    char *tmp = NULL;
    size_t len = strlen(input);
    int ret = 0;

    while (ptr < input + len) {
        char *to, *from, *flags;
        const char *cflags;
        eptr = strchr(ptr, ',');
        if (eptr == NULL) eptr = input + len;

        /* make copy for parsing */
        tmp = (char *) malloc(sizeof(char) * (eptr - ptr + 1));
        strncpy(tmp, ptr, eptr - ptr);
        tmp[eptr - ptr] = 0;

        to = strtok(tmp, ":");
        from = strtok(NULL, ":");
        flags = strtok(NULL, ":");

        if (_validateVolumeMap(to, from, flags) != 0) {
            fprintf("Invalid Volume Map: %*s, aborting!\n", (eptr - ptr), ptr);
            goto _parseVolumeMap_unclean;
        }

        if (to == NULL || from == NULL) {
            fprintf(stderr, "INVALID format for volume map %*s\n", (eptr - ptr), ptr);
            goto _parseVolumeMap_unclean;
        }
        if (flags == NULL) cflags = "";
        else cflags = flags;

        /* append to raw array */
        ret = strncpy_StringArray(ptr, eptr - ptr, &(volMap->raw),
                &rawPtr, &(volMap->rawCapacity), VOLUME_ALLOC_BLOCK);
        if (ret != 0) goto _parseVolumeMap_unclean;

        ret = strncpy_StringArray(to, strlen(to), &(volMap->to),
                &toPtr, &(volMap->toCapacity), VOLUME_ALLOC_BLOCK);
        if (ret != 0) goto _parseVolumeMap_unclean;

        ret = strncpy_StringArray(from, strlen(from), &(volMap->from),
                &fromPtr, &(volMap->fromCapacity), VOLUME_ALLOC_BLOCK);
        if (ret != 0) goto _parseVolumeMap_unclean;

        ret = strncpy_StringArray(cflags, strlen(cflags), &(volMap->flags),
                &flagsPtr, &(volMap->flagsCapacity), VOLUME_ALLOC_BLOCK);
        if (ret != 0) goto _parseVolumeMap_unclean;

        ptr = eptr + 1;
        volMap->n += 1;
        free(tmp);
        tmp = NULL;
    }
    return 0;
_parseVolumeMap_unclean:
    if (tmp != NULL) {
        free(tmp);
        tmp = NULL;
    }
    return 1;
}

static int _validateVolumeMap(const char *to, const char *from, const char *flags) {
    return 0;
}

char *getVolMapSignature(struct VolumeMap *volMap) {
}

/**
 * free_VolumeMap
 * Release memory allocated for VolumeMap structure. Optionally
 * release the memory for the struct itself (and not just the
 * instance members).
 */
void free_VolumeMap(struct VolumeMap *volMap, int freeStruct) {
    if (volMap == NULL) return;
    char **arrays[] = {
        volMap->raw,
        volMap->to,
        volMap->from,
        volMap->flags,
        NULL
    };

    char ***ptr = NULL;
    for (ptr = arrays; *ptr != NULL; ptr++) {
        char **iptr = NULL;
        for (iptr = *ptr; *iptr != NULL; iptr++) {
            fprintf(stderr, "value: %s\n" *iptr);
        }
    }
    if (freeStruct == 1) {
        free(volMap);
    }
}

#ifdef _TESTHARNESS_VOLMAP
#include <CppUTest/CommandLineTestRunner.h>

TEST_GROUP(VolumeMapTestGroup) {
};

TEST(VolumeMapTestGroup, VolumeMapParse_basic) {
    struct VolumeMap volMap;
    memset(&volMap, 0, sizeof(struct VolumeMap));

    int ret = parseVolumeMap("/path1/is/here:/target1", &volMap);
    CHECK(ret == 0);
    CHECK(volMap.n == 1);

    free_VolumeMap(&volMap, 0);
}

int main(int argc, char** argv) {
        return CommandLineTestRunner::RunAllTests(argc, argv);
}

#endif
