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

#include "MountList.h"
#include "utility.h"
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
