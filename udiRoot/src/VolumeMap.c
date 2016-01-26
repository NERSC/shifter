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
        VolumeMapFlag *flags,
        const char **toStartsWithDisallowed, 
        const char **toExactDisallowed,
        const char **fromStartsWithDisallowed,
        const char **fromExactDisallowed,
        size_t allowedFlags);

int __parseVolumeMap(const char *input, VolumeMap *volMap,
        int (*_validate_fp)(const char *, const char *, VolumeMapFlag *),
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
        VolumeMapFlag *flags)
{
    const char *toStartsWithDisallowed[] = {
        "/etc", "/var", "etc", "var", "/opt/udiImage", "opt/udiImage", NULL
    };
    const char *toExactDisallowed[] = {"/opt", "opt", NULL};
    const char *fromStartsWithDisallowed[] = { NULL };
    const char *fromExactDisallowed[] = { NULL };
    size_t allowedFlags = VOLMAP_FLAG_READONLY;

    return __validateVolumeMap(
            from, to, flags, toStartsWithDisallowed, toExactDisallowed,
            fromStartsWithDisallowed, fromExactDisallowed, allowedFlags
    );
}

int validateVolumeMap_siteRequest(
        const char *from,
        const char *to,
        VolumeMapFlag *flags)
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
    size_t allowedFlags = VOLMAP_FLAG_READONLY | VOLMAP_FLAG_RECURSIVE;

    return __validateVolumeMap(
            from, to, flags, toStartsWithDisallowed, toExactDisallowed,
            fromStartsWithDisallowed, fromExactDisallowed, allowedFlags
    );
}

const char *__findEndVolumeMapString(const char *basePtr) {
    const char *ptr = basePtr;
    for ( ; ptr && *ptr; ptr++) {
        if (*ptr == ';') {
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
    char *limit = input + strlen(input);
    if (tokens == NULL) {
        fprintf(stderr, "FAILED to allocate memory\n");
        goto ___tokenizeVolumeMapInput_unclean_exit;
    }

    /* set all tokens to NULL */
    memset(tokens, 0, sizeof(char *) * 4);

    /* parse the tokens */
    tk_ptr = tokens;
    sptr = input;
    for (ptr = input, sptr = input; sptr < limit; ptr++) {
        /* delimiter */
        if (*ptr == ':' || *ptr == '\0') {

            /* extract the token */
            *ptr = 0;
            *tk_ptr = strdup(sptr);

            /* advance to the next token */
            sptr = ptr + 1;
            tk_ptr++;

            /* allocate and move memory if need be */
            if (tk_ptr >= tk_end) {
                char **tmp = (char **) realloc(tokens, sizeof(char *) * ((tk_ptr - tokens) + 4));
                if (tmp == NULL) {
                    fprintf(stderr, "FAILED to allocate memory\n");
                    goto ___tokenizeVolumeMapInput_unclean_exit;
                }
                tk_ptr = tmp + (tk_ptr - tokens);
                tokens = tmp;
                tk_end = tk_ptr + 3;
                memset(tk_ptr, 0, sizeof(char *) * 4);
            }
        }
    }

    return tokens;
___tokenizeVolumeMapInput_unclean_exit:
    return NULL;
}

ssize_t __parseBytes(const char *input) {
    const char *scale = "bkmgtpe";
    char *ptr = NULL;
    const char *sptr = NULL;
    double a = strtod(input, &ptr);
    char expLetter = 0;
    if (ptr == NULL || *ptr == 0) {
        expLetter = 'b';
    } else {
        expLetter = tolower(*ptr);
    }
    int found = 0;
    for (sptr = scale; *sptr != 0; sptr++) {
        if (expLetter == *sptr) {
            found = 1;
            break;
        }
    }
    if (!found) {
        return -1;
    }
    for ( ; sptr != scale; sptr--) {
        a *= 1024;
    }
    return (ssize_t) a;
}

int __parseFlag(char *flagStr, VolumeMapFlag **flags, size_t *flagCapacity) {
    if (flagStr == NULL) return 0;
    if (flags == NULL) return 1;

    size_t flagIdx = 0;
    char *ptr = flagStr;
    char *sptr = flagStr;
    char *limit = flagStr + strlen(flagStr);
    char *flagName = NULL;
    char **kvArray = NULL;
    size_t kvCount = 0;

    VolumeMapFlag flag;

    memset(&flag, 0, sizeof(VolumeMapFlag));

    if (sptr == limit) return 0;

    /* advance flagPtr to end of list */
    for (flagIdx = 0; flagIdx < *flagCapacity; flagIdx++) {
        if ((*flags)[flagIdx].type == 0) break;
    }

    ptr = strchr(sptr, '=');
    if (ptr == NULL) ptr = limit;
    *ptr = 0;

    /* now sptr is pointing to the name of the flag;
       ptr is either pointing to the end of the string (equal to limit) or
       is pointing to the character before the first key in a comma
       delimited list of key/value pairs */
    flagName = strdup(sptr);
    sptr = ptr + 1;
    while (sptr < limit) {
        char *vptr = strchr(sptr, '=');
        ptr = strchr(sptr, ',');
        if (ptr == NULL) ptr = limit;
        *ptr = 0;
        if (vptr != NULL) {
            *vptr++ = 0;
        }
        kvArray = (char **) realloc(kvArray, sizeof(char *) * (kvCount + 2));
        if (kvArray == NULL) {
            fprintf(stderr, "Unknown flag: couldn't parse flag name\n");
            goto ___parseFlags_exit_unclean;
        }
        kvArray[kvCount++] = sptr;
        kvArray[kvCount++] = vptr;
        sptr = ptr + 1;
    }
    if (flagName == NULL) {
        fprintf(stderr, "Unknown flag: couldn't parse flag name\n");
        goto ___parseFlags_exit_unclean;
    }

    /* parse flag */
    if (strcasecmp(flagName, "ro") == 0) {
        flag.type = VOLMAP_FLAG_READONLY;
    } else if (strcasecmp(flagName, "rec") == 0) {
        flag.type = VOLMAP_FLAG_RECURSIVE;
    } else if (strcasecmp(flagName, "perNodeCache") == 0) {
        size_t kvIdx = 0;
        flag.type = VOLMAP_FLAG_PERNODECACHE;
        VolMapPerNodeCacheConfig *cache = (VolMapPerNodeCacheConfig *) malloc(sizeof(VolMapPerNodeCacheConfig));
        memset(cache, 0, sizeof(VolMapPerNodeCacheConfig));

        /* TODO move these configs into udiRoot.conf */
        cache->cacheSize = 0;
        cache->blockSize = 1024 * 1024; /* default 1MB */
        cache->fstype = strdup("ext4");
        cache->method = strdup("loop");
        flag.value = cache;

        for (kvIdx = 0; kvIdx < kvCount; kvCount += 2) {
            char *key = kvArray[kvIdx];
            char *value = kvArray[kvIdx + 1];

            if (key == NULL) {
                fprintf(stderr, "Failed to parse volmap flag value\n");
                goto ___parseFlags_exit_unclean;
            }
            if (strcasecmp(key, "size") == 0) {
                cache->cacheSize = __parseBytes(value);
                if (cache->cacheSize <= 0) {
                    fprintf(stderr, "Invalid size for perNodeCache: %s\n", value);
                    goto ___parseFlags_exit_unclean;
                }
            } else if (strcasecmp(key, "fs") == 0) {
                if (cache->fstype != NULL) {
                    free(cache->fstype);
                    cache->fstype = NULL;
                }
                if (value != NULL) {
                    if (strcasecmp(value, "ext4") == 0) {
                        cache->fstype = strdup("ext4");
                    }
                }
                if (cache->fstype == NULL) {
                    fprintf(stderr, "Invalid fstype for perNodeCache: %s\n", value);
                    goto ___parseFlags_exit_unclean;
                }
            } else if (strcasecmp(key, "bs") == 0) {
                cache->blockSize = __parseBytes(value);
                if (cache->blockSize <= 0) {
                    fprintf(stderr, "Invalid blocksize for perNodeCache: %s\n", value);
                    goto ___parseFlags_exit_unclean;
                }
            } else if (strcasecmp(key, "method") == 0) {
                if (cache->method != NULL) {
                    free(cache->method);
                    cache->method = NULL;
                }
                if (value != NULL) {
                    if (strcasecmp(value, "loop") == 0) {
                        cache->method = strdup("loop");
                    }
                }
                if (cache->method == NULL) {
                    fprintf(stderr, "Invalid method for perNodeCache: %s\n", value);
                    goto ___parseFlags_exit_unclean;
                }
            }
        }
    } else {
        fprintf(stderr, "Unknown flag: %s\n", sptr);
        goto ___parseFlags_exit_unclean;
    }

    if (flagIdx >= *flagCapacity) {
        VolumeMapFlag *tmp = (VolumeMapFlag *) realloc(flags, sizeof(VolumeMapFlag) * *flagCapacity + 2);
        if (tmp == NULL) {
            fprintf(stderr, "Failed to allocate memory!\n");
            goto ___parseFlags_exit_unclean;
        }
        *flags = tmp;
        *flagCapacity += 1;
    }
    memcpy(&(*flags[flagIdx++]), &flag, sizeof(VolumeMapFlag));
    memset(&(*flags[flagIdx]), 0, sizeof(VolumeMapFlag));

    return 0;
___parseFlags_exit_unclean:
    return 1;
}

int __parseVolumeMap(
        const char *input,
        VolumeMap *volMap,
        int (*_validate_fp)(const char *, const char *, VolumeMapFlag *),
        short requireTo
) {
    if (input == NULL || volMap == NULL) return 1;
    char **rawPtr = volMap->raw + volMap->n;
    char **toPtr = volMap->to + volMap->n;
    char **fromPtr = volMap->from + volMap->n;

    const char *ptr = input;
    const char *eptr = NULL;
    char *tmp = NULL;
    size_t len = strlen(input);
    int ret = 0;
    char *to = NULL;
    char *from = NULL;
    VolumeMapFlag *flags = NULL;
    size_t flagsCapacity = 0;
    char *raw = NULL;
    char **tokens = NULL;
    char **tk_ptr = NULL;
    size_t tokenIdx = 0;
    size_t rawLen = 0;

    while (ptr < input + len) {
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

        for (tokenIdx = 2; tokenIdx < ntokens; tokenIdx++) {
            if (__parseFlag(tokens[tokenIdx], &flags, &flagsCapacity) != 0) {
                fprintf(stderr, "Invalid mount flags specified: %s\n", tokens[tokenIdx]);
                goto _parseVolumeMap_unclean;
            }
        }

        /* if we only got a from and to is not required 
           assume we are binding a path from outside the container
           and to can be set to from */
        if (from && ntokens == 1 && !requireTo) {
            to = strdup(from);
        }

        /* ensure the user is asking for a legal mapping */
        if (_validate_fp(from, to, flags) != 0) {
            fprintf(stderr, "Invalid Volume Map: %.*s, aborting!\n",
                (int) (eptr - ptr),
                ptr
            );
            goto _parseVolumeMap_unclean;
        }

        if (to == NULL || from == NULL) {
            fprintf(stderr, "INVALID format for volume map %.*s\n", 
                (int) (eptr - ptr),
                ptr
            );
            goto _parseVolumeMap_unclean;
        }

        /* generate a new "raw" string from the filtered values */
        rawLen = 2 + strlen(from) + strlen(to);
#if 0
        if (flags != NULL) {
            rawLen += 1 + strlen(flags);
        }
#endif
        raw = (char *) malloc(sizeof(char) * rawLen);
        snprintf(raw, rawLen, "%s:%s", from, to);


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
        char *freeArray[] = {tmp, from, to, raw, NULL};
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
        VolumeMapFlag *flags,
        const char **toStartsWithDisallowed, 
        const char **toExactDisallowed,
        const char **fromStartsWithDisallowed,
        const char **fromExactDisallowed,
        size_t allowedFlags)
{
    const char **ptr = NULL;

    if (from == NULL || to == NULL) return 1;

    /* verify that the specified flags are acceptable */
#if 0
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
#endif

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
        VolumeMapFlag *flags = volMap->flags[count];
        if (from == NULL || to == NULL) continue;

        nBytes += fprintf(fp, "FROM: %s, TO: %s, FLAGS: ", from, to);
        if (flags == NULL) nBytes += fprintf(fp, "None");
        else {
        }
#if 0
        if (flags & VOLMAP_FLAG_READONLY) 
            nBytes += fprintf(fp, "%sread-ony", (flagDisplay++ > 0 ? ", " : ""));
        if (flags & VOLMAP_FLAG_RECURSIVE)
            nBytes += fprintf(fp, "%srecursive", (flagDisplay++ > 0 ? ", " : ""));
        if (flags & VOLMAP_FLAG_EXT4PERNODECACHE)
            nBytes += fprintf(fp, "%sext4PerNodeCache", (flagDisplay++ > 0 ? ", " : ""));
#endif
        nBytes += fprintf(fp, "\n");
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

    /* sort the volmaps to ensure different invocations of the same
       request are seen as identical */
    qsort(volMap->raw, volMap->n, sizeof(char *), _vstrcmp);

    /* sum strlens to get full summary string capacity
       add volMap->n characters to cover commas and final null byte */
    for (ptr = volMap->raw; *ptr != NULL; ptr++) {
        len += strlen(*ptr);
    }
    ret = (char *) malloc(sizeof(char) * (len + volMap->n));

    /* construct summary sig string */
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
    if (volMap->flags != NULL) {
        VolumeMapFlag **flagListPtr = volMap->flags;
        for ( ; flagListPtr && *flagListPtr; flagListPtr++) {
            VolumeMapFlag *flagArr = *flagListPtr;
            size_t idx = 0;
            for (idx = 0; ; idx++) {
                if (flagArr[idx].type == 0) break;
                if (flagArr[idx].type == VOLMAP_FLAG_PERNODECACHE
                        && flagArr[idx].value != NULL) {
                    VolMapPerNodeCacheConfig *cconfig = (VolMapPerNodeCacheConfig *) flagArr[idx].value;
                    free_VolMapPerNodeCacheConfig(cconfig);
                    flagArr[idx].value = NULL;
                }
            }
            free(*flagListPtr);
        }
        free(volMap->flags);
        volMap->flags = NULL;
    }
    if (freeStruct == 1) {
        free(volMap);
    }
}

void free_VolMapPerNodeCacheConfig(VolMapPerNodeCacheConfig *cacheConfig) {
    if (cacheConfig == NULL) return;
    if (cacheConfig->method != NULL) free(cacheConfig->method);
    if (cacheConfig->fstype != NULL) free(cacheConfig->fstype);
    free(cacheConfig);
}
