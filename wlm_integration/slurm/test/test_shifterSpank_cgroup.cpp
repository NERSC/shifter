/* Shifter, Copyright (c) 2016, The Regents of the University of California,
   through Lawrence Berkeley National Laboratory (subject to receipt of any
   required approvals from the U.S. Dept. of Energy).  All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
  3. Neither the name of the University of California, Lawrence Berkeley
     National Laboratory, U.S. Dept. of Energy nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

 See LICENSE for full text.
*/

#include "shifterSpank.h"
#include "utility.h"

#include <CppUTest/CommandLineTestRunner.h>
#include <string>
#include <vector>

#include <unistd.h>
#include <limits.h>

using namespace std;

TEST_GROUP(shifterSpankCgroupGroup) {
    char *tmpDir;
    char cwd[PATH_MAX];
    vector<string> tmpDirs;
    vector<string> tmpFiles;

    void setup() {
        getcwd(cwd, PATH_MAX);
        tmpDir = strdup("/tmp/shifter.XXXXXX");
        if (mkdtemp(tmpDir) == NULL) {
            fprintf(stderr, "WARNING mkdtemp failed, some tests will crash.\n");
        }
    }

    void teardown() {
        for (size_t idx = 0; idx < tmpFiles.size(); idx++) {
            unlink(tmpFiles[idx].c_str());
        }
        tmpFiles.clear();
        for (size_t idx = 0; idx < tmpDirs.size(); idx++) {
            rmdir(tmpDirs[idx].c_str());
        }
        tmpDirs.clear();
        chdir(cwd);
        rmdir(tmpDir);
        free(tmpDir);
    }
};

int doNothingWithPath(shifterSpank_config *config, const char *path, void *data) {
    fprintf(stderr, "CGROUP PATH: %s\n", path);
    return 0;
}

TEST(shifterSpankCgroupGroup, generateCgroupPath) {
    shifterSpank_config *config = shifterSpank_init(NULL, 0, NULL, 0);

    char *path = setup_memory_cgroup(config, 10, 1000, doNothingWithPath, NULL);
    CHECK(path == NULL)
    shifterSpank_config_free(config);

    char *args[] = {
        alloc_strgenf("memory_cgroup=%s", tmpDir),
        NULL
    };

    config = shifterSpank_init(NULL, 1, args, 0);
    path = setup_memory_cgroup(config, 10, 1000, doNothingWithPath, NULL);
    CHECK(path != NULL);
    fprintf(stderr, "GOT PATH: %s\n", path);
}

int main(int argc, char** argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
