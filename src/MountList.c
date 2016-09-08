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
#include <limits.h>
#include "MountList.h"
#include "utility.h"

/**
 * _sortMountForward
 * Utility function use for comparisons in support of MOUNT_SORT_FORWARD
 */
static int _sortMountForward(const void *ta, const void *tb) {
    const char **a = (const char **) ta;
    const char **b = (const char **) tb;

    return strcmp(*a, *b);
}

/**
 * _sortMountReverse
 * Utility function used for comparisons in support of MOUNT_SORT_REVERSE
 */
static int _sortMountReverse(const void *ta, const void *tb) {
    const char **a = (const char **) ta;
    const char **b = (const char **) tb;

    return -1 * strcmp(*a, *b);
}

/**
 * parse_MountList
 * Parses /proc/<pid>/mounts to populate a MountList; should be empty at start
 * Generates de-duplicated list of discrete mount points
 *
 * Parameters:
 * mounts: pointer to existing MountList structure
 *
 * Returns:
 * 0 on success
 * 1 on failure
 */
int parse_MountList(MountList *mounts) {
    pid_t pid = getpid();
    char fname_buffer[PATH_MAX];
    FILE *fp = NULL;
    char *lineBuffer = NULL;
    size_t lineBuffer_size = 0;
    ssize_t nRead = 0;

    if (mounts == NULL) {
        return 1;
    }

    snprintf(fname_buffer, PATH_MAX, "/proc/%d/mounts", pid);
    fp = fopen(fname_buffer, "r");
    if (fp == NULL) {
        fprintf(stderr, "FAILED to open %s\n", fname_buffer);
        return 1;
    }

    /* each line represents a valid mount point in this namespace, insert each
     * into the list */
    while (!feof(fp) && !ferror(fp)) {
        char *ptr = NULL;
        nRead = getline(&lineBuffer, &lineBuffer_size, fp);
        if (nRead == 0 || feof(fp) || ferror(fp)) {
            break;
        }
        /* want second space-seperated column */
        /* TODO switch to strtok_r */
        ptr = strtok(lineBuffer, " ");
        if (ptr == NULL) {
            goto _parseMountList_error;
        }
        ptr = strtok(NULL, " ");
        if (ptr == NULL) {
            continue;
        }
        if (insert_MountList(mounts, ptr) == 2) {
            goto _parseMountList_error;
        }
    }

    /* clean up */
    fclose(fp);
    fp = NULL;
    if (lineBuffer != NULL) {
        free(lineBuffer);
        lineBuffer = NULL;
    }

    return 0;
_parseMountList_error:
    if (fp != NULL) {
        fclose(fp);
    }
    if (lineBuffer != NULL) {
        free(lineBuffer);
    }
    return 1;
}

/**
 * setSort_MountList
 * sets a sorting order for the MountList and sorts it, can efficiently
 * reverse the list when switching sort direction of an already-sorted list.
 *
 * Parameters:
 * mounts: pointer to MountList
 * sorting: flag to determine type of sorting, either
 *          MOUNT_SORT_FORWARD or MOUNT_SORT_REVERSE
 *
 * Returns:
 * Nothing
 */
void setSort_MountList(MountList *mounts, MountListSortOrder sorting) {
    if (mounts == NULL) return;
    if (sorting != MOUNT_SORT_FORWARD && sorting != MOUNT_SORT_REVERSE)
        return;

    if (mounts->sorted == sorting) return;
    if (mounts->sorted == MOUNT_SORT_UNSORTED) {
        qsort(mounts->mountPointList, mounts->count, sizeof(char **), sorting == MOUNT_SORT_FORWARD ? _sortMountForward : _sortMountReverse);
    } else {
        /* need to reverse the list */
        char **left = mounts->mountPointList;
        char **right = mounts->mountPointList + mounts->count - 1;
        while (left < right) {
            char *t = *left;
            *left++ = *right;
            *right-- = t;
        }
    }
    mounts->sorted = sorting;
}

/**
 * insert_MountList
 * Add copy of key to MountList, maintaining sorted status.  If unsorted, 
 * is make Forward order sorted afterwards
 *
 * Parameters:
 * mounts: pointer to MountList structure
 * mountPoint: key to copy and insert into MountList
 *
 * Returns
 * 0 if successful
 * 1 if key already present
 * 2 if error
 */
int insert_MountList(MountList *mounts, const char *mountPoint) {
    char **mPtr = NULL;
    int (*compareFxn)(const void*,const void*) = NULL;


    if (mounts == NULL || mountPoint == NULL) return 0;

    if (mounts->count == 0 && mounts->sorted == MOUNT_SORT_UNSORTED) {
        mounts->sorted = MOUNT_SORT_FORWARD;
    }

    /* prevent duplicates */
    mPtr = find_MountList(mounts, mountPoint);
    if (mPtr != NULL) return 1;

    /* append value to end of array */
    mPtr = mounts->mountPointList + mounts->count;
    if (strncpy_StringArray(mountPoint, strlen(mountPoint), &mPtr, &(mounts->mountPointList), &(mounts->capacity), MOUNT_ALLOC_BLOCK) != 0) {
        return 2;
    }
    mounts->count++;
    if (mounts->count <= 1) return 0;

    /* work out sort (if any) */
    if (mounts->sorted == MOUNT_SORT_FORWARD) {
        compareFxn = _sortMountForward;
    } else if (mounts->sorted == MOUNT_SORT_FORWARD) {
        compareFxn = _sortMountReverse;
    } else {
        setSort_MountList(mounts, MOUNT_SORT_FORWARD);
        return 0;
    }

    mPtr--; /* reverse back to the item just inserted */

    /* position the one item that is out of order by truncated insertion sort */
    while (mPtr > mounts->mountPointList) {
        char **cPtr = mPtr--;
        int cmpVal = compareFxn(mPtr, cPtr);
        if (cmpVal > 0) {
            char *tmp = *cPtr;
            *cPtr = *mPtr;
            *mPtr = tmp;
        } else {
            break;
        }
    }
    return 0;
}

/**
 * remove_MountList
 * Find, then remove a key from a MountList.  Frees memory of object in
 * MountList.
 *
 * Parameters:
 * mounts: pointer to MountList structure
 * mountPoint: key to find, then remove
 *
 * Returns:
 * 0
 */
int remove_MountList(MountList *mounts, const char *mountPoint) {
    char **it = find_MountList(mounts, mountPoint);
    char **limit = NULL;
    if (it == NULL) return 0;
    limit = mounts->mountPointList + mounts->count;

    free(*it);
    for ( ; it < limit; it++) {
        *it = *(it + 1);
    }
    mounts->count--;
    return 0;
}

/**
 * find_MountList
 * Search mountlist for a particular key and return a pointer to it.  If the
 * MountList is sorted, then use a binary search, otherwise, use a linear scan.
 *
 * Parameters:
 * mounts:  point to the MountList structure
 * mountPoint: key to search for
 *
 * Returns:
 * Pointer to key in MountPoints, if discovered
 * NULL if not found
 */
char **find_MountList(MountList *mounts, const char *mountPoint) {
    char **ptr = NULL;
    int (*compareFxn)(const void *, const void *) = NULL;
    if (mounts == NULL || mountPoint == NULL) return NULL;
    if (mounts->sorted == MOUNT_SORT_FORWARD) {
        compareFxn = _sortMountForward;
    } else if (mounts->sorted == MOUNT_SORT_REVERSE) {
        compareFxn = _sortMountReverse;
    }

    if (compareFxn != NULL) {
        char **found = (char **) bsearch(&mountPoint, mounts->mountPointList, mounts->count, sizeof(char *), compareFxn);
        if (found != NULL) return found;
        return NULL;
    }

    /* fall back to linear search if content not sorted */
    for (ptr = mounts->mountPointList; ptr && *ptr; ptr++) {
        if (strcmp(*ptr, mountPoint) == 0) return ptr;
    }
    return NULL;
}

/**
 * findstartswith_MountList
 * Return pointer to the first (lowest memory address) mount which starts with
 * key.
 * \param mounts pointer to the MountList structure
 * \param key string to search for
 *
 * Returns lowest matching key or NULL if there are no matches
 */
char **findstartswith_MountList(MountList *mounts, const char *key) {
    char **ptr = NULL;
    size_t len = 0;

    /* TODO implement modified bsearch to allow partial key matching and
     * stably return the first matching key.
     * For now, just do the linear search since the list of mounts should
     * be relatively short*/

    if (mounts == NULL || key == NULL) return NULL;
    len = strlen(key);
    if (len == 0) return NULL;

    for (ptr = mounts->mountPointList; ptr && *ptr; ptr++) {
        if (strncmp(*ptr, key, len) == 0) return ptr;
    }
    return NULL;
}

/**
 * findpartial_MountList
 * Return pointer to the first (lowest memory address) mount which partially
 * matches target ignoring acceptablePrefix in both strings.
 * \param mounts pointer to the MountList structure
 * \param target string to partially match
 * \param acceptablePrefix string that can be present at beginning without
 *                         being considered a partial match
 *
 * Returns lowest matching key or NULL if there are no matches
 */
char **findpartial_MountList(MountList *mounts, const char *target, const char *acceptablePrefix) {
    char **ptr = NULL;
    size_t matchMin = 0;
    size_t matchMax = 0;
    if (mounts == NULL || target == NULL) return NULL;

    matchMax = strlen(target);
    if (acceptablePrefix != NULL) {
        matchMin = strlen(acceptablePrefix);
        if (strncmp(acceptablePrefix, target, matchMin) != 0) {
            /* acceptablePrefix must appear at beginning of target */
            return NULL;
        }
    }

    for (ptr = mounts->mountPointList; ptr && *ptr; ptr++) {
        size_t len = strlen(*ptr);
        if (len <= matchMin) continue;
        if (len > matchMax) continue;
        if (strncmp(*ptr, target, len) == 0) {
            const char *sub = target + len;
            if (strchr(sub, '/') != NULL) {
                return ptr;
            }
        }
    }
    return NULL;
}

/**
 * free_MountList
 * Fully destruct all components of the MountList, optionally, destruct the
 * MountList itself
 *
 * Parameters:
 * mounts:  pointer to the MountList structure
 * freeStruct:  1 to free the MountList itself, 0 otherwise
 *
 * Returns:
 * Nothing
 */
void free_MountList(MountList *mounts, int freeStruct) {
    char **ptr = NULL;
    if (mounts == NULL) return;
    if (mounts->mountPointList != NULL) {
        for (ptr = mounts->mountPointList; *ptr; ptr++) {
            free(*ptr);
        }
        free(mounts->mountPointList);
    }
    memset(mounts, 0, sizeof(MountList));
    if (freeStruct) {
        free(mounts);
    }
}
