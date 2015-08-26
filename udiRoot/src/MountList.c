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
        ptr = strtok(NULL, " ");
        if (ptr == NULL) continue;
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
        char **found = (char **) bsearch(&mountPoint, mounts->mountPointList, mounts->count, sizeof(char**), compareFxn);
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

#ifdef _TESTHARNESS_MOUNTLIST
#include <CppUTest/CommandLineTestRunner.h>

TEST_GROUP(MountListTestGroup) {
};

TEST(MountListTestGroup, BasicTests) {
    /* can't easily create synthetic cases, since this is reading from /proc
     * but, will count lines and ensure we get the same number of mounts here
     */

    MountList m;
    int rc = 0;
    char **ptr = NULL;
    pid_t pid = getpid();
    int count = 0;

    memset(&m, 0, sizeof(MountList));

    rc = parse_MountList(&m);
    CHECK(rc == 0)
    CHECK(m.count > 0);

    count = m.count;

    /* expect / to be mounted */
    ptr = find_MountList(&m, "/");
    CHECK(ptr != NULL);
    CHECK(*ptr != NULL);
    CHECK(strcmp(*ptr, "/") == 0);


    /* sort the list forward and check for sorting */
    setSort_MountList(&m, MOUNT_SORT_FORWARD);
    char *last = NULL;
    for (ptr = m.mountPointList; ptr && *ptr; ptr++) {
        if (last != NULL) {
            CHECK(strcmp(last, *ptr) <= 0)
        }
        last = *ptr;
    }
    CHECK(ptr - m.mountPointList == m.count)

    /* sort the list reverse and check for sorting */
    setSort_MountList(&m, MOUNT_SORT_REVERSE);
    last = NULL;
    for (ptr = m.mountPointList; ptr && *ptr; ptr++) {
        if (last != NULL) {
            CHECK(strcmp(last, *ptr) >= 0)
        }
        last = *ptr;
    }
    CHECK(ptr - m.mountPointList == m.count)


    setSort_MountList(&m, MOUNT_SORT_FORWARD);
    /* validate sort order maintained */
    last = NULL;
    for (ptr = m.mountPointList; ptr && *ptr; ptr++) {
        if (last != NULL) {
            CHECK(strcmp(last, *ptr) <= 0)
        }
        last = *ptr;
    }
    CHECK(ptr - m.mountPointList == m.count)

    /* add a new item to list */
    insert_MountList(&m, "/a/b/c");
    CHECK(count + 1 == m.count)

    insert_MountList(&m, "/c/b/a");
    CHECK(count + 2 == m.count)

    insert_MountList(&m, "/b/c/a");
    CHECK(count + 3 == m.count)

    /* validate sort was maintained */
    last = NULL;
    for (ptr = m.mountPointList; ptr && *ptr; ptr++) {
        if (last != NULL) {
            CHECK(strcmp(last, *ptr) <= 0)
        }
        last = *ptr;
    }
    CHECK(ptr - m.mountPointList == m.count)

    /* can we find an item */
    ptr = find_MountList(&m, "/b/c/a");
    CHECK(ptr != NULL)
    CHECK(ptr >= m.mountPointList);
    CHECK(ptr <= m.mountPointList + m.count)
    CHECK(strcmp(*ptr, "/b/c/a") == 0)

    /* try removing an item */
    rc = remove_MountList(&m, "/b/c/a");
    CHECK(rc == 0)
    CHECK(count + 2 == m.count);
    ptr = find_MountList(&m, "/b/c/a");
    CHECK(ptr == NULL)

    /* ensure sort is maintained and that there isn't a NULL "hole" present */
    last = NULL;
    for (ptr = m.mountPointList; ptr && *ptr; ptr++) {
        if (last != NULL) {
            CHECK(strcmp(last, *ptr) <= 0)
        }
        last = *ptr;
    }
    CHECK(ptr - m.mountPointList == m.count)

    free_MountList(&m, 0);
}

TEST(MountListTestGroup, insertBoundary) {
    MountList m;
    memset(&m, 0, sizeof(MountList));

    insert_MountList(&m, "abcd");
    CHECK(m.count == 1);

    insert_MountList(&m, "Abcd");
    CHECK(m.count == 2);

    free_MountList(&m, 0);
}

TEST(MountListTestGroup, removeBoundary) {
    MountList m;
    memset(&m, 0, sizeof(MountList));

    insert_MountList(&m, "abcd");
    insert_MountList(&m, "Abcd");

    CHECK(m.count == 2);

    remove_MountList(&m, "Abcd");
    CHECK(m.count == 1);
    remove_MountList(&m, "Abcd");
    CHECK(m.count == 1);
    CHECK(strcmp(*(m.mountPointList), "abcd") == 0);

    remove_MountList(&m, "abcd");
    CHECK(m.count == 0);
    CHECK(*(m.mountPointList) == NULL);

    remove_MountList(&m, "abcd");
    CHECK(m.count == 0);

    free_MountList(&m, 0);
}

TEST(MountListTestGroup, findBoundary) {
    MountList m;
    memset(&m, 0, sizeof(MountList));

    insert_MountList(&m, "abcd");
    insert_MountList(&m, "abcd");
    CHECK(m.count == 1);

    char **ptr = find_MountList(&m, "abcd");
    CHECK(ptr != NULL);
    CHECK(*ptr != NULL);
    CHECK(strcmp(*ptr, "abcd") == 0);

    ptr = find_MountList(&m, "dcba");
    CHECK(ptr == NULL);
    
    ptr = find_MountList(&m, NULL);
    CHECK(ptr == NULL);

    remove_MountList(&m, "abcd");
    CHECK(m.count == 0);

    ptr = find_MountList(&m, "abcd");
    CHECK(ptr == NULL);

    free_MountList(&m, 0);
}

TEST(MountListTestGroup, findstartswith) {
    MountList m;
    char **ptr = NULL;
    memset(&m, 0, sizeof(MountList));

    insert_MountList(&m, "abdec");
    insert_MountList(&m, "abcde");
    insert_MountList(&m, "abcd");
    insert_MountList(&m, "abctuv");
    insert_MountList(&m, "abbbcd");

    /* should be sorted in forward direction */
    ptr = findstartswith_MountList(&m, "abc");
    CHECK(ptr != NULL);
    CHECK(strcmp(*ptr, "abcd") == 0);

    ptr = findstartswith_MountList(&m, "ab");
    CHECK(ptr != NULL);
    CHECK(strcmp(*ptr, "abbbcd") == 0);

    /* reverse the sort and make sure we get the correct result */
    setSort_MountList(&m, MOUNT_SORT_REVERSE);
    ptr = findstartswith_MountList(&m, "ab");
    CHECK(ptr != NULL);
    CHECK(strcmp(*ptr, "abdec") == 0);

    /* see what happens when we search for something not in the list */
    CHECK(findstartswith_MountList(&m, "notInList") == NULL);

    /* check invalid input */
    CHECK(findstartswith_MountList(&m, "") == NULL);
    CHECK(findstartswith_MountList(&m, NULL) == NULL);
    CHECK(findstartswith_MountList(NULL, "key") == NULL);

    free_MountList(&m, 0);
}

int main(int argc, char** argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

#endif
