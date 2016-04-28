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

#ifndef __SHFTR_VOLMAP_INCLUDE
#define __SHFTR_VOLMAP_INCLUDE

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VOLUME_ALLOC_BLOCK 10

#define VOLMAP_FLAG_READONLY 1
#define VOLMAP_FLAG_RECURSIVE 2
#define VOLMAP_FLAG_PERNODECACHE 4
#define VOLMAP_FLAG_SLAVE 8
#define VOLMAP_FLAG_PRIVATE 16

typedef struct {
    int type;
    void *value;
} VolumeMapFlag;

typedef struct _VolumeMap {
    char **raw;
    char **to;
    char **from;
    VolumeMapFlag **flags;
    size_t n;

    size_t rawCapacity;
    size_t toCapacity;
    size_t fromCapacity;
    size_t flagsCapacity;
} VolumeMap;

typedef struct {
    ssize_t cacheSize;
    ssize_t blockSize;
    char *method;
    char *fstype;
} VolMapPerNodeCacheConfig;


int parseVolumeMap(const char *input, VolumeMap *volMap);
int parseVolumeMapSiteFs(const char *input, VolumeMap *volMap);
char *getVolMapSignature(VolumeMap *volMap);
size_t fprint_VolumeMap(FILE *fp, VolumeMap *volMap);
void free_VolumeMap(VolumeMap *volMap, int freeStruct);
int validateVolumeMap_userRequest(const char *from, const char *to, VolumeMapFlag *flags);
int validateVolumeMap_siteRequest(const char *from, const char *to, VolumeMapFlag *flags);

void free_VolumeMapFlag(VolumeMapFlag *flag, int freeStruct);
void free_VolMapPerNodeCacheConfig(VolMapPerNodeCacheConfig *cacheConfig);
int validate_VolMapPerNodeCacheConfig(VolMapPerNodeCacheConfig *cacheConfig);
ssize_t parseBytes(const char *input);

/* semi-private methods */
int _validateVolumeMap(
        const char *from,
        const char *to,
        VolumeMapFlag *flags,
        const char **toStartsWithDisallowed, 
        const char **toExactDisallowed,
        const char **fromStartsWithDisallowed,
        const char **fromExactDisallowed,
        size_t allowedFlags);

int _parseVolumeMap(const char *input, VolumeMap *volMap,
        int (*_validate_fp)(const char *, const char *, VolumeMapFlag *),
        short requireTo);
const char *_findEndVolumeMapString(const char *basePtr);
char **_tokenizeVolumeMapInput(char *input);
int _parseFlag(char *flagStr, VolumeMapFlag **flags, size_t *flagCapacity);

#ifdef __cplusplus
}
#endif

#endif
