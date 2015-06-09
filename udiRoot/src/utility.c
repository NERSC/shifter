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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "utility.h"


int shifter_parseConfig(const char *filename, char delim, void *obj, int (*assign_fp)(char *, char *, void *)) {
    FILE *fp = NULL;
    char *linePtr = NULL;
    char *ptr = NULL;
    size_t lineSize = 0;
    size_t nRead = 0;
    int multiline = 0;

    char *key = NULL;
    char *value = NULL;
    char *tValue = NULL;
    size_t valueLen = 0;
    size_t tValueLen = 0;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        return 1;
    }
    while (!feof(fp) && !ferror(fp)) {
        nRead = getline(&linePtr, &lineSize, fp);
        if (nRead <= 0) break;

        /* get key/value pair */
        if (!multiline) {
            ptr = strchr(linePtr, delim);
            if (ptr == NULL) continue;
            *ptr++ = 0;
            key = shifter_trim(strdup(linePtr));
            if (key == NULL) {
                goto _parseConfig_errCleanup;
            }
            tValue = shifter_trim(ptr);
        } else {
            tValue = shifter_trim(linePtr);
            multiline = 0;
        }

        /* check to see if value extends over multiple lines */
        if (tValue[strlen(tValue) - 1] == '\\') {
            multiline = 1;
            tValue[strlen(tValue) - 1] = 0;
            tValue = shifter_trim(tValue);
        }

        /* merge value and tValue */
        tValueLen = strlen(tValue);
        value = (char *) realloc(value, sizeof(char)*(valueLen + tValueLen + 2));
        ptr = value + valueLen;
        *ptr = 0;
        strncat(value, " ", valueLen + 2);
        strncat(value, tValue, valueLen + tValueLen + 2);
        valueLen += tValueLen + 1;

        /* if value is complete, assign */
        if (multiline == 0) {
            ptr = shifter_trim(value);

            assign_fp(key, ptr, obj);
            if (value != NULL) {
                free(value);
            }
            if (key != NULL) {
                free(key);
            }
            key = NULL;
            value = NULL;
            valueLen = 0;
        }
    }
    if (linePtr != NULL) {
        free(linePtr);
    }
    return 0;
_parseConfig_errCleanup:
    if (linePtr != NULL) {
        free(linePtr);
    }
    if (value != NULL) {
        free(value);
    }
    if (key != NULL) {
        free(value);
    }
    return 1;
}


char *shifter_trim(char *str) {
    char *ptr = str;
    ssize_t len = 0;
    if (str == NULL) return NULL;
    for ( ; isspace(*ptr) && *ptr != 0; ptr++) {
        /* that's it */
    }
    if (*ptr == 0) return ptr;
    len = strlen(ptr) - 1;
    for ( ; isspace(*(ptr + len)) && len > 0; len--) {
        *(ptr + len) = 0;
    }
    return ptr;
}
