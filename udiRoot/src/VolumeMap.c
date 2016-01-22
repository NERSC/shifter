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
#include <string.h>
#include <unistd.h>
#include "VolumeMap.h"
#include "utility.h"
#include "shifter_core.h"

int __validateVolumeMap(
        const char *from,
        const char *to,
        const char *flags,
        const char **toStartsWithDisallowed, 
        const char **toExactDisallowed,
        const char **fromStartsWithDisallowed,
        const char **fromExactDisallowed,
        const char **allowedFlags);

int __parseVolumeMap(const char *input, VolumeMap *volMap,
        int (*_validate_fp)(const char *, const char *, const char *),
        short requireTo);

int parseVolumeMap(const char *input, VolumeMap *volMap) {
    return __parseVolumeMap(input, volMap, validateVolumeMap_userRequest, 1);
}

int parseVolumeMapSiteFs(const char *input, VolumeMap *volMap) {
    return __parseVolumeMap(input, volMap, validateVolumeMap_siteRequest, 0);
}

int validateVolumeMap_userRequest(
        const char *from,
        const char *to,
        const char *flags)
{
    const char *toStartsWithDisallowed[] = {
        "/etc", "/var", "etc", "var", "/opt/udiImage", "opt/udiImage", NULL
    };
    const char *toExactDisallowed[] = {"/opt", "opt", NULL};
    const char *fromStartsWithDisallowed[] = { NULL };
    const char *fromExactDisallowed[] = { NULL };
    const char *allowedFlags[] = { "ro", "", NULL };

    return __validateVolumeMap(
            from, to, flags, toStartsWithDisallowed, toExactDisallowed,
            fromStartsWithDisallowed, fromExactDisallowed, allowedFlags
    );
}

int validateVolumeMap_siteRequest(
        const char *from,
        const char *to,
        const char *flags)
{
    const char *toStartsWithDisallowed[] = { NULL };
    const char *toExactDisallowed[] = {
        "/opt", "opt",
        "/etc", "etc",
        "/var", "var",
        "/etc/passwd", "etc/passwd",
        "/etc/group", "etc/group",
        "/etc/nsswitch.conf", "etc/nsswitch.conf",
        NULL
    };
    const char *fromStartsWithDisallowed[] = { NULL };
    const char *fromExactDisallowed[] = { NULL };
    const char *allowedFlags[] = { "ro", "rec", "", NULL };

    return __validateVolumeMap(
            from, to, flags, toStartsWithDisallowed, toExactDisallowed,
            fromStartsWithDisallowed, fromExactDisallowed, allowedFlags
    );
}

const char *__findEndVolumeMapString(const char *basePtr) {
    const char *ptr = basePtr;
    int inQuote = 0;
    for ( ; ptr && *ptr; ptr++) {
        if (*ptr == '"') {
            if (inQuote) inQuote = 0;
            else inQuote = 1;
            continue;
        }
        if (*ptr == ',' && !inQuote) {
            return ptr;
        }
    }
    return ptr;
}

char **__tokenizeVolumeMapInput(char *input) {
    if (input == NULL) return NULL;

    char **tokens = (char **) malloc(sizeof(char *) * 4);
    char **tk_ptr = NULL;
    char **tk_end = tokens + 3;
    char *ptr = NULL;
    char *sptr = NULL;
    if (tokens == NULL) return NULL;

    /* set all tokens to NULL */
    memset(tokens, 0, sizeof(char *) * 4);

    /* parse the tokens */
    tk_ptr = tokens;
    sptr = input;
    for (ptr = input; ptr && *ptr && tk_ptr < tk_end - 1; ptr++) {
        if (*ptr == '\\') {
            ptr++;
            continue;
        }
        /* delimiter */
        if (*ptr == ':') {

            /* extract the token */
            *ptr = 0;
            *tk_ptr = strdup(sptr);

            /* advance to the next token */
            sptr = ptr + 1;
            tk_ptr++;
        }
    }

    /* get the trailing token */
    if (tk_ptr < tk_end && sptr) {
        /* if there are quotes exactly around the token, strip them */
        if (sptr < ptr && *sptr == '"' && *(ptr - 1) == '"') {
            sptr++;
            *(ptr - 1) = 0;
        }
        *tk_ptr = strdup(sptr);
    }

    return tokens;
}

int __parseVolumeMap(
        const char *input,
        VolumeMap *volMap,
        int (*_validate_fp)(const char *, const char *, const char *),
        short requireTo
) {
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
    char *to = NULL;
    char *from = NULL;
    char *flags = NULL;
    char *raw = NULL;
    char **tokens = NULL;
    char **tk_ptr = NULL;
    size_t rawLen = 0;

    while (ptr < input + len) {
        const char *cflags;
        size_t ntokens = 0;
        eptr = __findEndVolumeMapString(ptr);
        if (eptr == ptr && eptr != NULL) {
            ptr++;
            continue;
        }
        if (eptr == NULL) {
            break;
        }

        /* make copy for parsing */
        tmp = (char *) malloc(sizeof(char) * (eptr - ptr + 1));
        if (tmp == NULL) {
            fprintf(stderr, "Failed to allocate memory for tmp string\n");
            goto _parseVolumeMap_unclean;
        }
        strncpy(tmp, ptr, eptr - ptr);
        tmp[eptr - ptr] = 0;

        char *volMapStr = tmp;
        if (*volMapStr == '"' && *(volMapStr + (eptr - ptr) - 1) == '"') {
            *(volMapStr + (eptr - ptr) - 1) = 0;
            volMapStr++;
        }

        /* tokenize the the input string */
        tokens = __tokenizeVolumeMapInput(volMapStr);

        /* count how many non-NULL tokens there are */
        for (ntokens = 0; tokens && tokens[ntokens]; ntokens++) {
        }

        if (tokens == NULL || ntokens == 0) {
            fprintf(stderr, "Failed to parse VolumeMap tokens from \"%s\","
                    " aborting!\n", tmp);
            goto _parseVolumeMap_unclean;
        }

        from = userInputPathFilter(tokens[0], 1);
        to = userInputPathFilter(tokens[1], 1);
        flags = userInputPathFilter(tokens[2], 0);

        /* if we only got a from and to is not required 
           assume we are binding a path from outside the container
           and to can be set to from */
        if (from && ntokens == 1 && !requireTo) {
            to = strdup(from);
        }

        /* ensure the user is asking for a legal mapping */
        if (_validate_fp(from, to, flags) != 0) {
            fprintf(stderr, "Invalid Volume Map: %*s, aborting!\n",
                (int) (eptr - ptr),
                ptr
            );
            goto _parseVolumeMap_unclean;
        }

        if (to == NULL || from == NULL) {
            fprintf(stderr, "INVALID format for volume map %*s\n", 
                (int) (eptr - ptr),
                ptr
            );
            goto _parseVolumeMap_unclean;
        }
        if (flags == NULL) cflags = "";
        else cflags = flags;

        /* generate a new "raw" string from the filtered values */
        rawLen = 2 + strlen(from) + strlen(to);
        if (flags != NULL) {
            rawLen += 1 + strlen(flags);
        }
        raw = (char *) malloc(sizeof(char) * rawLen);
        snprintf(raw, rawLen, "%s:%s%c%s", from, to,
                (flags && strlen(flags) > 0 ? ':' : '\0'),
                (flags && strlen(flags) > 0 ? flags : "")
        );


        /* append to raw array */
        ret = strncpy_StringArray(raw, rawLen, &rawPtr, &(volMap->raw),
                &(volMap->rawCapacity), VOLUME_ALLOC_BLOCK);
        if (ret != 0) goto _parseVolumeMap_unclean;

        ret = strncpy_StringArray(to, strlen(to), &toPtr, &(volMap->to),
                &(volMap->toCapacity), VOLUME_ALLOC_BLOCK);
        if (ret != 0) goto _parseVolumeMap_unclean;

        ret = strncpy_StringArray(from, strlen(from), &fromPtr, &(volMap->from),
                &(volMap->fromCapacity), VOLUME_ALLOC_BLOCK);
        if (ret != 0) goto _parseVolumeMap_unclean;

        ret = strncpy_StringArray(cflags, strlen(cflags), &flagsPtr, &(volMap->flags),
                &(volMap->flagsCapacity), VOLUME_ALLOC_BLOCK);
        if (ret != 0) goto _parseVolumeMap_unclean;

        if (from != NULL) free(from);
        if (to != NULL) free(to);
        if (flags != NULL) free(flags);
        if (raw != NULL) free(raw);
        from = NULL;
        to = NULL;
        flags = NULL;
        raw = NULL;

        ptr = eptr + 1;
        volMap->n += 1;
        if (tmp != NULL) free(tmp);
        tmp = NULL;

        for (tk_ptr = tokens; tk_ptr && *tk_ptr; tk_ptr++) {
            free(*tk_ptr);
        }
        if (tokens != NULL) free(tokens);
        tokens = NULL;
    }
    return 0;
_parseVolumeMap_unclean:
    {
        char *freeArray[] = {tmp, from, to, flags, raw, NULL};
        char **freePtr = NULL;
        for (freePtr = freeArray; *freePtr != NULL; freePtr++) {
            if (*freePtr != NULL) {
                free(*freePtr);
            }
        }

        for (tk_ptr = tokens; tk_ptr && *tk_ptr; tk_ptr++) {
            free(*tk_ptr);
        }
        if (tokens != NULL) free(tokens);
        tokens = NULL;
    }
    return 1;
}


int __validateVolumeMap(
        const char *from,
        const char *to,
        const char *flags,
        const char **toStartsWithDisallowed, 
        const char **toExactDisallowed,
        const char **fromStartsWithDisallowed,
        const char **fromExactDisallowed,
        const char **allowedFlags)
{
    const char **ptr = NULL;

    if (from == NULL || to == NULL) return 1;

    /* verify that the specified flags are acceptable */
    if (flags != NULL) {
        int found = 0;
        for (ptr = allowedFlags; *ptr != NULL; ptr++) {
            if (strcmp(*ptr, flags) == 0) {
                found = 1;
                break;
            }
        }
        if (found == 0) {
            return 2;
        }
    }

    for (ptr = toStartsWithDisallowed; *ptr != NULL; ptr++) {
        size_t len = strlen(*ptr);
        if (strncmp(to, *ptr, len) == 0) {
            return 1;
        }
    }
    for (ptr = toExactDisallowed; *ptr != NULL; ptr++) {
        if (strcmp(to, *ptr) == 0) {
            return 1;
        }
    }
    for (ptr = fromStartsWithDisallowed; *ptr != NULL; ptr++) {
        size_t len = strlen(*ptr);
        if (strncmp(from, *ptr, len) == 0) {
            return 1;
        }
    }
    for (ptr = fromExactDisallowed; *ptr != NULL; ptr++) {
        if (strcmp(from, *ptr) == 0) {
            return 1;
        }
    }

    return 0;
}

/** fprint_volumeMap - write formatted output to specified FILE pointer */
size_t fprint_VolumeMap(FILE *fp, VolumeMap *volMap) {
    if (fp == NULL) return 0;

    size_t count = 0;
    size_t nBytes = 0;
    if (volMap != NULL) count = volMap->n;
    nBytes += fprintf(fp, "Volume Map: %lu maps\n", count);
    if (volMap == NULL) return nBytes;

    for (count = 0; count < volMap->n; count++) {
        char *from = volMap->from[count];
        char *to = volMap->to[count];
        char *flags = volMap->flags[count];
        const char *cflags = (flags == NULL ? "None" : flags);
        if (from == NULL || to == NULL) continue;

        nBytes += fprintf(fp, "FROM: %s, TO: %s, FLAGS: %s\n", from, to, cflags);
    }
    return nBytes;
}

/* _vstrcmp: adaptor function to dereference points, cast and run strcmp
 *    this is for qsort call below
 */
static int _vstrcmp(const void *a, const void *b) {
    return strcmp(*((const char **) a), *((const char **) b));
}

char *getVolMapSignature(VolumeMap *volMap) {
    char **ptr = NULL;
    size_t len = 0;
    char *ret = NULL;
    char *wptr = NULL;
    char *limit = NULL;

    if (volMap == NULL || volMap->raw == NULL || volMap->n == 0) {
        return NULL;
    }

    qsort(volMap->raw, volMap->n, sizeof(char *), _vstrcmp);
    for (ptr = volMap->raw; *ptr != NULL; ptr++) {
        len += strlen(*ptr);
    }

    ret = (char *) malloc(sizeof(char) * (len + volMap->n));
    wptr = ret;
    limit = ret + (len + volMap->n);
    for (ptr = volMap->raw; *ptr != NULL; ptr++) {
        wptr += snprintf(wptr, (limit - wptr), "%s,", *ptr);
    }
    wptr--;
    *wptr = 0;

    return ret;
}

/**
 * free_VolumeMap
 * Release memory allocated for VolumeMap structure. Optionally
 * release the memory for the struct itself (and not just the
 * instance members).
 */
void free_VolumeMap(VolumeMap *volMap, int freeStruct) {
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
            free(*iptr);
        }
        free(*ptr);
        *ptr = NULL;
    }
    if (freeStruct == 1) {
        free(volMap);
    }
}
