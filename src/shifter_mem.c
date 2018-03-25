/** @file shifter_mem.h
 *  @brief Core include for heap operations
 *
 *  @author Douglas M. Jacobsen <dmjacobsen@lbl.gov>
 */

/* Shifter, Copyright (c) 2017, The Regents of the University of California,
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shifter_mem.h>

void *shifter_realloc(void *ptr, size_t alloc_size, const char *file, int line,
                        const char *function)
{
    void *ret = realloc(ptr, alloc_size);
    if (ret == NULL) {
        fprintf(stderr, "FAILED to allocate memory at %s: %d in %s\n",
                file, line, function);
        abort();
    }
    return ret;
}

void *shifter_malloc(size_t alloc_size, const char *file, int line,
                            const char *function)
{
    void *ret = malloc(alloc_size);
    if (ret == NULL) {
        fprintf(stderr, "FAILED to allocate memory at %s: %d in %s\n",
                file, line, function);
        abort();
    }
    return ret;
}

char *shifter_strdup(const char *input, const char *file, int line,
                        const char *function)
{
    char *ret = strdup(input);
    if (ret == NULL) {
        fprintf(stderr, "FAILED to duplicate string at %s: %d in %s\n",
                file, line, function);
        abort();
    }
    return ret;
}

char *shifter_strndup(const char *input, size_t len, const char *file, int line,
                        const char *function)
{
    char *ret = strndup(input, len);
    if (ret == NULL) {
        fprintf(stderr, "FAILED to duplicate string at %s: %d in %s\n",
                file, line, function);
        abort();
    }
    return ret;
}
