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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "utility.h"


int shifter_parseConfig(const char *filename, char delim, void *obj, int (*assign_fp)(const char *, const char *, void *)) {
    FILE *fp = NULL;
    char *linePtr = NULL;
    char *ptr = NULL;
    size_t lineSize = 0;
    size_t nRead = 0;
    int multiline = 0;

    char *key = NULL;
    char *key_alloc = NULL;
    char *value = NULL;
    char *tValue = NULL;
    size_t valueLen = 0;
    size_t tValueLen = 0;
    int ret = 0;

    if (filename == NULL || obj == NULL || assign_fp == NULL) {
        return 1;
    }

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
            key_alloc = strdup(linePtr);
            key = shifter_trim(key_alloc);
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

            ret = assign_fp(key, ptr, obj);
            if (ret != 0) goto _parseConfig_errCleanup;

            if (value != NULL) {
                free(value);
            }
            if (key_alloc != NULL) {
                free(key_alloc);
            }
            key = NULL;
            key_alloc = NULL;
            value = NULL;
            valueLen = 0;
        }
    }
    if (linePtr != NULL) {
        free(linePtr);
        linePtr = NULL;
    }
_parseConfig_errCleanup:
    if (fp != NULL) {
        fclose(fp);
    }
    if (linePtr != NULL) {
        free(linePtr);
    }
    if (value != NULL) {
        free(value);
    }
    if (key_alloc != NULL) {
        free(key_alloc);
    }
    return ret;
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

/**
 * strncpy_StringArray
 * Append a string to the specified string array.  Assumes a relatively small
 * number of items.  Uses a block allocation scheem to expand array.  String
 * is destructively appended at the wptr position.  Pointers are naivly
 * verified to at least make sense.
 *
 * Affect: Space is allocated in array if necessary; then n bytes of string
 * str are appended to end of array.  A NULL terminator is appended to the
 * end of array.  wptr is left pointing to the NULL terminator for the next
 * append.
 *
 * Parameters:
 * str:  input string
 * n  :  bytes to copy
 * wptr: pointer to write position; value may be changed upon array
 *       reallocation, or successful append.  Left pointing to NULL terminator.
 * array: pointer to base of String Array. value may be updated upon
 *       reallocation
 * capacity: pointer to array capacity; updated upon reallocation
 * allocationBlock: number of elements to add per reallocation event, must be
 *       greater than 0.
 *
 * Returns: 0 upon success, 1 upon failure
 */
int strncpy_StringArray(const char *str, size_t n, char ***wptr,
        char ***array, size_t *capacity, size_t allocationBlock) {

    size_t count = 0;
    if (str == NULL || wptr == NULL || array == NULL
            || capacity == NULL || allocationBlock == 0 ||
            *wptr < *array || *wptr - *capacity > *array) {
        fprintf(stderr, "ERROR: invalid input to strncpy_StringArray\n");
        return 1;
    }

    /* allocate more space at a time */
    count = *wptr - *array;
    if (*capacity - count < 2) {
        size_t new_capacity = *capacity + allocationBlock;
        char **tmp = (char **) realloc(*array, sizeof(char *) * new_capacity);
        if (tmp == NULL) {
            fprintf(stderr, "ERROR: failed to allocate memory, append failed\n");
            return 1;
        }
        *array = tmp;
        *wptr = *array + count;
        *capacity += allocationBlock;
    }

    /* append string to array, add ternminated NULL */
    **wptr = (char *) malloc(sizeof(char) * (n + 1));
    memcpy(**wptr, str, n);
    (**wptr)[n] = 0;
    (*wptr)++;
    **wptr = NULL;
    return 0;
}

/**
 * alloc_strcatf appends to an existing string (or creates a new one) based on
 * user-supplied format in printf style.  The current capacity and length of
 * the string are used to determine where to write in the string and when to
 * allocate.  An eager-allocation strategy is used where the string capacity is
 * doubled at each reallocation.  This is done to improve performance of
 * repeated calls
 * Parameters:
 *      string - pointer to string to be extended, can be NULL
 *      currLen - pointer to integer representing current limit of used portion
 *                of string
 *      capacity - pointer to integer representing current capacity of string
 *      format - printf style formatting string
 *      ...    - variadic arguments for printf
 *
 * The dereferenced value of currLen is modified upon successful concatenation
 * of string.
 * The dereferenced value of capacity is modified upon successful reallocation
 * of string.
 *
 * Returns pointer to string, possibly realloc'd.  Caller should treat this
 * function like realloc (test output for NULL before assigning).
 * Returns NULL upon vsnprintf error or failure to realloc.
 */
char *alloc_strcatf(char *string, size_t *currLen, size_t *capacity, const char *format, ...) {
    int status = 0;
    int n = 0;

    if (currLen == 0 || capacity == 0 || format == NULL) {
        return NULL;
    }
    if (*currLen > *capacity) {
        return NULL;
    }

    if (string == NULL || *capacity == 0) {
        string = (char *) malloc(sizeof(char) * 128);
        *capacity = 128;
    }

    while (1) {
        char *wptr = string + *currLen;
        va_list ap;

        va_start(ap, format);
        n = vsnprintf(wptr, *capacity - (*currLen), format, ap);
        va_end(ap);

        if (n == -1) {
            /* error, break */
            status = 1;
            break;
        } else if (n >= (*capacity - *currLen)) {
            /* if vsnprintf returns larger than allowed buffer, need more space
             * allocating eagerly to reduce cost for successive strcatf 
             * operations */
            size_t newCapacity = *capacity * 2 + 1;
            char *tmp = NULL;
            if (newCapacity < n + 1) {
                newCapacity = n + 1;
            }
            
            tmp = (char *) realloc(string, sizeof(char) * newCapacity);
            if (tmp == NULL) {
                status = 2;
                break;
            }
            string = tmp;
            *capacity = newCapacity;
        } else {
            /* success */
            status = 0;
            *currLen += n;
            break;
        }
    }
    if (status == 0) {
        return string;
    }
    return NULL;
}

#ifdef _TESTHARNESS_UTILITY
#include <CppUTest/CommandLineTestRunner.h>

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

static int _assignTestConfig(const char *key, const char *value, void *_testConfig) {
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

TEST(UtilityTestGroup, strncpyStringArray_basic) {
    char **array = NULL;
    char **wptr = NULL;
    size_t capacity = 0;
    int ret = 0;

    ret = strncpy_StringArray("abcdefg", 4, &wptr, &array, &capacity, 10);
    CHECK(ret == 0);
    CHECK(*wptr == NULL);
    CHECK(capacity == 10);

    const char *triples = "abc;def;ghi;jkl;mno;pqr;stu;vxy;z01";
    const char *ptr = triples;
    const char *eptr = NULL;
    while (ptr < triples + strlen(triples)) {
        eptr = strchr(ptr, ';');
        if (eptr == NULL) eptr = triples + strlen(triples);
        ret = strncpy_StringArray(ptr, eptr - ptr, &wptr, &array, &capacity, 10);
        CHECK(ret == 0);
        CHECK(*wptr == NULL);
        ptr = eptr + 1;
    }
    CHECK(wptr - array == 10);

    /* check myriad error handling */
    ret = strncpy_StringArray(NULL, 20, &wptr, &array, &capacity, 10);
    CHECK(ret == 1);

    ret = strncpy_StringArray(*array, 0, &wptr, &array, &capacity, 10);
    CHECK(ret == 0);
    CHECK(strlen((*(wptr - 1))) == 0);

    ret = strncpy_StringArray("test", 4, NULL, &array, &capacity, 10);
    CHECK(ret == 1);

    ret = strncpy_StringArray("test", 4, &wptr, NULL, &capacity, 10);
    CHECK(ret == 1);

    ret = strncpy_StringArray("test", 4, &wptr, &array, NULL, 10);
    CHECK(ret == 1);

    ret = strncpy_StringArray("test", 4, &wptr, &array, &capacity, 0);
    CHECK(ret == 1);

    for (wptr = array; *wptr != NULL; wptr++) {
        free(*wptr);
    }
    free(array);
}

char *alloc_strcatf(char *string, size_t *currLen, size_t *capacity, const char *format, ...);
TEST(UtilityTestGroup, allocStrcatf_basic) {
    size_t len = 0;
    size_t capacity = 0;
    char *string = NULL;
    char *tmp = NULL;
    char buffer[2048];

    tmp = alloc_strcatf(string, NULL, &capacity, "%s", "hello");
    CHECK(tmp == NULL);

    tmp = alloc_strcatf(string, &len, NULL, "%s", "hello");
    CHECK(tmp == NULL);

    tmp = alloc_strcatf(string, &len, &capacity, NULL);
    CHECK(tmp == NULL);

    /* first real run */
    tmp = alloc_strcatf(string, &len, &capacity, "%s", "hello");
    CHECK(tmp != NULL);
    CHECK(len == 5);
    CHECK(*(tmp + len) == 0)
    string = tmp;

    /* extend string */
    tmp = alloc_strcatf(string, &len, &capacity, ", %s. %d.", "world", 42);
    CHECK(tmp != NULL);
    printf("%s\n", tmp);
    CHECK(strcmp(tmp, "hello, world. 42.") == 0)
    CHECK(capacity == 128)
    CHECK(*(tmp + len) == 0)
    string = tmp;

    memset(buffer, 'A', sizeof(char) * 2048);
    buffer[2047] = 0;
    tmp = alloc_strcatf(string, &len, &capacity, "%s", buffer);
    CHECK(tmp != NULL);
    CHECK(strncmp(tmp, "hello, world. 42.AAAAAAA", strlen("hello, world. 42.AAAAAAA")) == 0)
    CHECK(capacity > 2050)
    string = tmp;

    free(string);
}

int main(int argc, char** argv) {
        return CommandLineTestRunner::RunAllTests(argc, argv);
}

#endif
