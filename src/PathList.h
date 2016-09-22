/* Shifter, Copyright (c) 2016, The Regents of the University of California,
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

#ifndef __SHFTR_PATHLIST_INCLUDE
#define __SHFTR_PATHLIST_INCLUDE

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _PathList;

typedef struct _PathComponent {
    char *item;
    struct _PathComponent *parent;
    struct _PathComponent *child;
    struct _PathList *list;
} PathComponent;

typedef struct _PathList {
    PathComponent *path;
    PathComponent *relroot;
    PathComponent *terminal;
    int absolute;
} PathList;

PathList *pathList_init(const char *path);
PathList *pathList_duplicate(PathList *src);
PathList *pathList_duplicatePartial(PathList *src, PathComponent *last);
void pathList_trimLast(PathList *src);
int pathList_setRoot(PathList *path, const char *relroot);
int pathList_append(PathList *base, const char *path);
PathComponent *pathList_matchPartial(PathList *fullpath, PathList *partial);

/** pathList_commonPath
 * calculates the overlapping common path between two different PathLists
 */
PathList *pathList_commonPath(PathList *a, PathList *b);

/** pathList_appendComponents
 * copy path components from a specified point in a source list to the end of a
 * destination list.  returns the dest copy of the specified component in src
 * any failure should result in no modification to dset
 */
PathComponent * pathList_appendComponents(PathList *dest, PathList *src, PathComponent *comp);

/** pathList_string
 * allocate a string representing the entire PathList
 */
char *pathList_string(PathList *path);

/** pathList_stringPartial
 * allocate a string representing a partion of the PathList up to and
 * including pos */
char *pathList_stringPartial(PathList *path, PathComponent *pos);

/** pathList_symlinkSubstitute
 * vital part of a realpath() implementation, replaces component link (and
 * any preceding components necessary) based on the provided value for it's
 * symlink (linkVal).
 *
 * This will modify the provided PathList *path, so you shouldn't hold any
 * pointers into it.
 *
 * This will return a pointer to the first modified component (i.e., in a
 * realpath() implementation the first that hasn't been considered, assuming
 * all components prior to the target symlink have been lstat()ed.)
 *
 * Example:
 * path:  /var/udiMount/global/user/dmj/test/1234
 *        where /var/udiMount is marked as the root
 * link:  component pointing to "user"
 * linkVal: "/global/u1" (i.e. user -> /global/u1)
 *
 * In this case "/global/u1" will be processed relative to the explict
 * root of /var/udiMount, so the link and all components before it will
 * be replaced with /var/udiMount/global/u1.  Since the original path
 * and new search path match up until u1, the component for u1 will be
 * returned.  Finally, the components for "dmj/test/1234" will be
 * re-attached following the link target (u1, in this case), so:
 *
 * return: the component pointing to u1
 * path:   updated to match "/var/udiMount/global/u1/dmj/test/1234"
 *
 * The search through realpath() should continue with u1.
 */
PathComponent *pathList_symlinkSubstitute(PathList *path,
        PathComponent *link, const char *linkVal);
PathList *pathList_symlinkResolve(PathList *base, const char *symlink);

void pathList_resolve(PathList *path);
void pathList_free(PathList *path);
void pathList_freeComponent(PathComponent *comp);
void pathList_freeComponents(PathComponent *comp);

#ifdef __cplusplus
}
#endif

#endif
