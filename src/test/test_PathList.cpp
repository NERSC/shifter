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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include "PathList.h"
#include <CppUTest/CommandLineTestRunner.h>

TEST_GROUP(PathListTestGroup) {
};

TEST(PathListTestGroup, BasicTests) {
    const char *cases[] = {
        "/this/is/a/test",
        "///////this/is//a///test",
        "/./this/././././is/a/test",
        "/./this/././././is/not/../a/test",
        "/../../../this/././././is/not/../a/test",
        "///////this/is//a///test",
        NULL
    };
    const char *relroots[] = {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "/this/is",
        NULL
    };
    const char **input = NULL;
    const char **relroot = NULL;

    for (input = cases, relroot = relroots; input && *input; input++, relroot++) {
        fprintf(stderr, "Checking \"%s\"%s%s\n", *input,
                (*relroot != NULL) ? " with relative root " : "",
                (*relroot != NULL) ? *relroot : "");
        PathList *path = pathList_init(*input);
        PathComponent *cmp = NULL;
        int count = 0;
        CHECK(path != NULL);
        CHECK(path->absolute == 1);

        if (*relroot != NULL) {
            CHECK(pathList_setRoot(path, *relroot) == 0);
        }

        for (cmp = path->path; cmp; cmp = cmp->child, count++) {
            switch(count) {
                case 0: CHECK(strcmp(cmp->item, "this") == 0); break;
                case 1: CHECK(strcmp(cmp->item, "is") == 0); break;
                case 2: CHECK(strcmp(cmp->item, "a") == 0); break;
                case 3: CHECK(strcmp(cmp->item, "test") == 0); break;
                default: CHECK(1 == 0);
            }
        }
        char *out = pathList_string(path);

        CHECK(count == 4);
        CHECK(strcmp(out, "/this/is/a/test") == 0);

        free(out);
        pathList_free(path);
    }
}

TEST(PathListTestGroup, appendPathTest) {
    const char *base = "/var/udiRoot";
    PathList *basePath = pathList_init(base);
    CHECK(basePath != NULL);
    CHECK(basePath->absolute == 1);

    char *strPath = NULL;
    CHECK(pathList_append(basePath, "global/homes") == 0);
    strPath = pathList_string(basePath);
    CHECK(strPath != NULL);
    CHECK(strcmp(strPath, "/var/udiRoot/global/homes") == 0);
    free(strPath);
    strPath = NULL;

    pathList_free(basePath);
}

TEST(PathListTestGroup, resolveSymLink) {
    const char *baseRoot = "/var/udiRoot";
    PathList *basePath = pathList_init(baseRoot);
    CHECK(basePath != NULL);
    CHECK(basePath->absolute == 1);

    CHECK(pathList_setRoot(basePath, baseRoot) == 0);
    CHECK(basePath->terminal != NULL);
    CHECK(basePath->terminal->item != NULL);
    CHECK(strcmp(basePath->terminal->item, "udiRoot") == 0);

    PathList *tmp = pathList_duplicate(basePath);
    CHECK(tmp != NULL);
    CHECK(pathList_append(tmp, "global") == 0);
    char *strPath = pathList_string(tmp);
    CHECK(strPath != NULL);
    fprintf(stderr, "got str: %s\n", strPath);
    CHECK(strcmp(strPath, "/var/udiRoot/global") == 0);
    free(strPath);
    strPath = NULL;

    PathList *newpath = pathList_symlinkResolve(tmp, "/global/u1");
    CHECK(newpath != NULL);
    strPath = pathList_string(tmp);
    CHECK(strPath != NULL);
    CHECK(strcmp(strPath, "/var/udiRoot/global") == 0);
    free(strPath);
    strPath = pathList_string(newpath);
    CHECK(strPath != NULL);
    CHECK(strcmp(strPath, "/var/udiRoot/global/u1") == 0);
    free(strPath);
    strPath = NULL;

    pathList_free(newpath);
    pathList_free(tmp);
    pathList_free(basePath);
}

TEST(PathListTestGroup, substituteSymLink_typical) {

    PathList *path = pathList_init("/var/udiMount/global/user/dmj/asdf/1234");
    pathList_setRoot(path, "/var/udiMount");
    PathComponent *search = NULL;
    for (search = path->path; search; search = search->child) {
        if (strcmp(search->item, "user") == 0) {
            break;
        }
    }
    CHECK(search != NULL);

    search = pathList_symlinkSubstitute(path, search, "/global/u1");
    CHECK(search != NULL);
    CHECK(strcmp(search->item, "u1") == 0);
    char *str = pathList_string(path);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/var/udiMount/global/u1/dmj/asdf/1234") == 0);
    free(str);
    pathList_free(path);
}

TEST(PathListTestGroup, substituteSymLink_norelroot) {

    PathList *path = pathList_init("/var/udiMount/global/user/dmj/asdf/1234");
    PathComponent *search = NULL;
    for (search = path->path; search; search = search->child) {
        if (strcmp(search->item, "user") == 0) {
            break;
        }
    }
    CHECK(search != NULL);

    search = pathList_symlinkSubstitute(path, search, "/global/u1");
    CHECK(search != NULL);
    CHECK(strcmp(search->item, "global") == 0);
    char *str = pathList_string(path);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/global/u1/dmj/asdf/1234") == 0);
    free(str);
    pathList_free(path);
}

TEST(PathListTestGroup, substituteSymLink_terminal) {

    PathList *path = pathList_init("/var/udiMount/global/user/dmj/asdf/1234");
    PathComponent *search = NULL;
    for (search = path->path; search; search = search->child) {
        if (strcmp(search->item, "1234") == 0) {
            break;
        }
    }
    CHECK(search != NULL);

    search = pathList_symlinkSubstitute(path, search, "4567");
    CHECK(search != NULL);
    CHECK(strcmp(search->item, "4567") == 0);
    char *str = pathList_string(path);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/var/udiMount/global/user/dmj/asdf/4567") == 0);
    free(str);
    pathList_free(path);
}

TEST(PathListTestGroup, substituteSymLink_inRoot) {

    PathList *path = pathList_init("/var/udiMount/global/user/dmj/asdf/1234");
    PathComponent *search = NULL;
    for (search = path->path; search; search = search->child) {
        if (strcmp(search->item, "var") == 0) {
            break;
        }
    }
    CHECK(search != NULL);

    search = pathList_symlinkSubstitute(path, search, "4567");
    CHECK(search != NULL);
    CHECK(strcmp(search->item, "4567") == 0);
    char *str = pathList_string(path);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/4567/udiMount/global/user/dmj/asdf/1234") == 0);
    free(str);
    pathList_free(path);
}

TEST(PathListTestGroup, substituteSymLink_compremoval) {

    PathList *path = pathList_init("/var/udiMount/global/user/dmj/asdf/1234");
    pathList_setRoot(path, "/var/udiMount");
    PathComponent *search = NULL;
    for (search = path->path; search; search = search->child) {
        if (strcmp(search->item, "user") == 0) {
            break;
        }
    }
    CHECK(search != NULL);

    search = pathList_symlinkSubstitute(path, search, "../../../../../../../../..");
    CHECK(search != NULL);
    CHECK(strcmp(search->item, "dmj") == 0);
    char *str = pathList_string(path);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/var/udiMount/dmj/asdf/1234") == 0);
    free(str);
    pathList_free(path);
}

TEST(PathListTestGroup, substituteSymLink_compadd) {

    PathList *path = pathList_init("/var/udiMount/global/user/dmj/asdf/1234");
    pathList_setRoot(path, "/var/udiMount");
    PathComponent *search = NULL;
    for (search = path->path; search; search = search->child) {
        if (strcmp(search->item, "user") == 0) {
            break;
        }
    }
    CHECK(search != NULL);

    search = pathList_symlinkSubstitute(path, search, "real/home1");
    CHECK(search != NULL);
    CHECK(strcmp(search->item, "real") == 0);
    char *str = pathList_string(path);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/var/udiMount/global/real/home1/dmj/asdf/1234") == 0);
    free(str);
    pathList_free(path);
}

TEST(PathListTestGroup, appendcomp_test) {
    PathList *dest = pathList_init("/var/udiRoot");
    PathList *src = pathList_init("/var/udiRoot/test/append/path");
    PathComponent *find = NULL;

    CHECK(dest != NULL);
    CHECK(src != NULL);


    CHECK(pathList_setRoot(dest, "/var/udiRoot") == 0);
    CHECK(pathList_setRoot(src, "/var/udiRoot") == 0);

    for (find = src->path; find; find = find->child) {
        if (strcmp(find->item, "append") == 0) {
            break;
        }
    }
    CHECK(find != NULL);

    /* validate bad data checks work correctly */
    CHECK(pathList_appendComponents(NULL, src, find) == NULL);
    CHECK(pathList_appendComponents(dest, NULL, find) == NULL);
    CHECK(pathList_appendComponents(dest, src, NULL) == NULL);
    PathComponent *test = (PathComponent *) malloc(sizeof(PathComponent));
    memset(test, 0, sizeof(PathComponent));
    CHECK(pathList_appendComponents(dest, src, test) == NULL);
    free(test);

    PathComponent *tgt = pathList_appendComponents(dest, src, find);
    CHECK(tgt != NULL);
    CHECK(strcmp(tgt->item, "append") == 0);

    char *strrep = pathList_string(dest);
    CHECK(strrep != NULL);
    CHECK(strcmp(strrep, "/var/udiRoot/append/path") == 0);
    CHECK(strcmp(dest->relroot->item, "udiRoot") == 0);

    free(strrep);

    pathList_free(dest);
    pathList_free(src);
}

TEST(PathListTestGroup, commonpath_test) {
    PathList *a = pathList_init("/var/udiRoot/global/u1/a/b/c/../d");
    PathList *b = pathList_init("/var/udiRoot/global/user/a/b/c/../d");

    CHECK(pathList_setRoot(a, "/var/udiRoot") == 0);
    CHECK(pathList_setRoot(b, "/var/udiRoot") == 0);

    PathList *common = pathList_commonPath(a, b);
    char *str = pathList_string(common);
    CHECK(common != NULL);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/var/udiRoot/global") == 0);
    CHECK(common->relroot != NULL);
    CHECK(strcmp(common->relroot->item, "udiRoot") == 0);

    free(str);
    pathList_free(common);
    pathList_free(a);
    pathList_free(b);

    a = pathList_init("/var/udiRoot/global/u1/a/b/c/../d");
    b = pathList_init("/var/udiRoot/global/u1/a/b/c/../d");
    common = pathList_commonPath(a, b);
    str = pathList_string(common);
    CHECK(common != NULL);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/var/udiRoot/global/u1/a/b/d") == 0);

    free(str);
    pathList_free(common);
    pathList_free(a);
    pathList_free(b);

    a = pathList_init("/a/b/c/d/asdf/fdsa");
    b = pathList_init("/asdf/fdsa/a/b/c/d");
    common = pathList_commonPath(a, b);
    CHECK(common != NULL);
    str = pathList_string(common);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/") == 0);
    CHECK(common->path == NULL);
    CHECK(common->terminal == NULL);
    CHECK(common->relroot == NULL);

    free(str);
    pathList_free(common);
    pathList_free(a);
    pathList_free(b);
}

TEST(PathListTestGroup, realpathlite_test) {
    PathList *userreq = pathList_init("/var/udiMount/global/user/dmj/test/path/1234");
    CHECK(pathList_setRoot(userreq, "/var/udiMount") == 0);
    PathComponent *compptr = NULL;

    for (compptr = userreq->path; compptr; compptr = compptr->child) {
        if (strcmp(compptr->item, "user") == 0) {
            break;
        }
    }
    CHECK(compptr != NULL);

    PathList *symlinkParent = pathList_duplicatePartial(userreq, compptr->parent);
    char *str = pathList_string(symlinkParent);
    CHECK(symlinkParent != NULL);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/var/udiMount/global") == 0);
    free(str);

    PathList *symlinkResolve = pathList_symlinkResolve(symlinkParent, "/global/u1");
    CHECK(symlinkResolve != NULL);
    CHECK(symlinkResolve->relroot != NULL);
    CHECK(strcmp(symlinkResolve->relroot->item, "udiMount") == 0);

    str = pathList_string(symlinkResolve);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/var/udiMount/global/u1") == 0);
    free(str);

    /* the "common" path are all the previously checked components */
    PathList *commonPath = pathList_commonPath(symlinkParent, symlinkResolve);
    CHECK(commonPath != NULL);
    str = pathList_string(commonPath);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/var/udiMount/global") == 0);
    free(str);

    /* get the first unchecked component */
    PathComponent *unchecked = pathList_matchPartial(symlinkResolve, commonPath);
    CHECK(unchecked != NULL);
    unchecked = unchecked->child;
    CHECK(unchecked != NULL);
    CHECK(strcmp(unchecked->item, "u1") == 0);

    /* construct new chain */
    unchecked = pathList_appendComponents(commonPath, symlinkResolve, unchecked);
    CHECK(unchecked != NULL);
    CHECK(strcmp(unchecked->item, "u1") == 0);
    str = pathList_string(commonPath);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/var/udiMount/global/u1") == 0);
    free(str);

    /* append rest of path */
    CHECK(pathList_appendComponents(commonPath, userreq, compptr->child) != NULL);
    str = pathList_string(commonPath);
    CHECK(str != NULL);
    CHECK(strcmp(str, "/var/udiMount/global/u1/dmj/test/path/1234") == 0);
    free(str);

    pathList_free(commonPath);
    pathList_free(symlinkParent);
    pathList_free(symlinkResolve);
    pathList_free(userreq);
}



int main(int argc, char** argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
