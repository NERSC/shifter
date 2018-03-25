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

#ifndef __SHFTR_MEM_INCLUDE
#define __SHFTR_MEM_INCLUDE

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define _realloc(__p, __sz) \
    shifter_realloc(__p, __sz, __FILE__, __LINE__, __func__)

#define _malloc(__sz) \
    shifter_malloc(__sz, __FILE__, __LINE__, __func__)

#define _strdup(__str) \
    shifter_strdup(__str, __FILE__, __LINE__, __func__)

#define _strndup(__str, __sz) \
    shifter_strndup(__str, __sz, __FILE__, __LINE__, __func__)

void *shifter_realloc(void *, size_t, const char *, int, const char *);
void *shifter_malloc(size_t, const char *, int, const char *);
char *shifter_strdup(const char *, const char *, int, const char *);
char *shifter_strndup(const char *, size_t, const char *, int, const char *);

#ifdef __cplusplus
}
#endif

#endif
