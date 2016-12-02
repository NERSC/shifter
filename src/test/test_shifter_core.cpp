/** @file test_shifter_core.cpp
 *  @brief Tests for library for setting up and tearing down user-defined images
 *
 *  @author Douglas M. Jacobsen <dmjacobsen@lbl.gov>
 */

/* Shifter, Copyright (c) 2016, The Regents of the University of California,
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

#include <CppUTest/CommandLineTestRunner.h>
#include <exception>
#include <algorithm>
#include <vector>
#include <string>
#include <iostream>

#include <grp.h>
#include <stdlib.h>

#include "ImageData.h"
#include "UdiRootConfig.h"
#include "shifter_core.h"
#include "utility.h"
#include "VolumeMap.h"
#include "MountList.h"
#include <fcntl.h>

extern "C" {
int _shifterCore_bindMount(UdiRootConfig *config, MountList *mounts, const char *from, const char *to, int ro, int overwrite);
int _shifterCore_copyFile(const char *cpPath, const char *source, const char *dest, int keepLink, uid_t owner, gid_t group, mode_t mode);
}

extern char** environ;

#ifdef NOTROOT
#define ISROOT 0
#else
#define ISROOT 1
#endif
#ifndef DANGEROUSTESTS
#define DANGEROUSTESTS 0
#endif

using namespace std;

int setupLocalRootVFSConfig(UdiRootConfig **config, ImageData **image, const char *tmpDir, const char *cwd) {
    *config = (UdiRootConfig *) malloc(sizeof(UdiRootConfig));
    *image = (ImageData *) malloc(sizeof(ImageData));

    const char *basePath = getenv("srcdir");
    if (basePath == NULL) {
        basePath = cwd;
    }

    memset(*config, 0, sizeof(UdiRootConfig));
    memset(*image, 0, sizeof(ImageData));

    (*image)->type = strdup("local");
    parse_ImageData((*image)->type, strdup("/"), *config, *image);
    (*config)->udiMountPoint = strdup(tmpDir);
    (*config)->udiRootPath = alloc_strgenf("/usr", basePath);
    (*config)->rootfsType = strdup(ROOTFS_TYPE);
    (*config)->etcPath = alloc_strgenf("%s/%s", basePath, "etc");
    (*config)->cpPath = strdup("/bin/cp");
    (*config)->mvPath = strdup("/bin/mv");
    (*config)->ddPath = strdup("/bin/dd");
    (*config)->chmodPath = strdup("/bin/chmod");
    (*config)->perNodeCachePath = strdup("/tmp");
    (*config)->allowLocalChroot = 1;
    (*config)->target_uid = 1000;
    (*config)->target_gid = 1000;
    (*config)->mountPropagationStyle = VOLMAP_FLAG_PRIVATE;
    return 0;
}


TEST_GROUP(ShifterCoreTestGroup) {
    bool isRoot;
    char *tmpDir;
    char cwd[PATH_MAX];
    vector<string> tmpFiles;
    vector<string> tmpDirs;

    void setup() {
        bool isRoot = getuid() == 0;
        bool macroIsRoot = ISROOT == 1;
        getcwd(cwd, PATH_MAX);
        tmpDir = strdup("/tmp/shifter.XXXXXX");
        if (isRoot && !macroIsRoot) {
            fprintf(stderr, "WARNING: the bulk of the functional tests are"
                    " disabled because the test suite is compiled with "
                    "-DNOTROOT, but could have run since you have root "
                    "privileges.");
        } else if (!isRoot && macroIsRoot) {
            fprintf(stderr, "WARNING: the test suite is built to run root-"
                    "privileged tests, but you don't have those privileges."
                    " Several tests will fail.");
        }
        if (mkdtemp(tmpDir) == NULL) {
            fprintf(stderr, "WARNING mkdtemp failed, some tests will crash.\n");
        }
    }

    void teardown() {
        MountList mounts;
        memset(&mounts, 0, sizeof(MountList));
        parse_MountList(&mounts);
        for (size_t idx = 0; idx < tmpFiles.size(); idx++) {
            unlink(tmpFiles[idx].c_str());
        }
        tmpFiles.clear();
        for (size_t idx = 0; idx < tmpDirs.size(); idx++) {
            rmdir(tmpDirs[idx].c_str());
        }
        tmpDirs.clear();
        if (find_MountList(&mounts, tmpDir) != NULL) {
            unmountTree(&mounts, tmpDir);
        }
        chdir(cwd);
        rmdir(tmpDir);
        free(tmpDir);
        free_MountList(&mounts, 0);
    }

};

TEST(ShifterCoreTestGroup, check_find_process_by_cmdline) {
    const char *basepath = getenv("srcdir");
    if (basepath == NULL) {
        basepath = ".";
    }
    char *cmd = alloc_strgenf("%s/shifter_sleep_test", basepath);
    char *args[] = {cmd, cmd, NULL};

    pid_t pid = fork();
    if (pid == 0) {
        execv(args[0], args);
        exit(127);
    }
    CHECK(pid > 0);
    usleep(1000000);

    pid_t discovered = shifter_find_process_by_cmdline(cmd);
    printf("pid: %d, discovered: %d, %s\n", pid, discovered, cmd);
    CHECK(pid == discovered);

    CHECK(kill(pid, SIGTERM) == 0);

    CHECK(shifter_find_process_by_cmdline(NULL) == -1);

    free(cmd);
};

#ifdef NOTROOT
IGNORE_TEST(ShifterCoreTestGroup, CopyFile_basic) {
#else
TEST(ShifterCoreTestGroup, CopyFile_basic) {
#endif
    char *toFile = NULL;
    char *ptr = NULL;
    int ret = 0;
    struct stat statData;

    toFile = alloc_strgenf("%s/passwd", tmpDir);

    /* check invalid input */
    ret = _shifterCore_copyFile("/bin/cp", NULL, toFile, 0, INVALID_USER, INVALID_GROUP, 0644);
    CHECK(ret != 0);
    ret = _shifterCore_copyFile("/bin/cp", "/etc/passwd", NULL, 0, INVALID_USER, INVALID_GROUP, 0644);
    CHECK(ret != 0);

    /* should succeed */
    ret = _shifterCore_copyFile("/bin/cp", "/etc/passwd", toFile, 0, INVALID_USER, INVALID_GROUP, 0644);
    tmpFiles.push_back(toFile);
    CHECK(ret == 0);

    ret = lstat(toFile, &statData);
    CHECK(ret == 0);
    CHECK((statData.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == 0644)

    ret = unlink(toFile);
    CHECK(ret == 0);

    ret = _shifterCore_copyFile("/bin/cp", "/etc/passwd", toFile, 0, INVALID_USER, INVALID_GROUP, 0755);
    CHECK(ret == 0);

    ret = lstat(toFile, &statData);
    CHECK(ret == 0);
    CHECK((statData.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == 0755)

    ret = unlink(toFile);
    CHECK(ret == 0);
    free(toFile);
}

int jailbreak() {
    chdir("/");
    int fd = open("/", O_DIRECTORY);
    mkdir("break", 0755);
    chdir("break");
    chroot(".");
    fchdir(fd);

    for ( ; ; ) {
        struct stat dot;
        struct stat dotdot;

        if (stat(".", &dot) != 0) break;
        if (stat("..", &dotdot) != 0) break;

        if (dot.st_ino == dotdot.st_ino) {
            return chroot(".");
        }
        chdir("..");
    }
    return 1;
}

#define SETUP_CHROOT(path) \
    CHECK(chdir(path) == 0);\
    CHECK(chroot(".") == 0);\
    {

#define CHECK_CHROOT(statement, returndir) \
    CHECK(statement);\
    }\
    CHECK(jailbreak() == 0);\
    printf("returning to %s\n", returndir);\
    CHECK(chdir(returndir) == 0);

#define END_CHROOT(returndir) \
    }\
    CHECK(jailbreak() == 0);\
    printf("returning to %s\n", returndir);\
    CHECK(chdir(returndir) == 0);


#ifdef NOTROOT
IGNORE_TEST(ShifterCoreTestGroup, test_getgrouplist_basic) {
#else
TEST(ShifterCoreTestGroup, test_getgrouplist_basic) {
#endif
    gid_t *groups = NULL;
    int ngroups = 0;
    pid_t pid = 0;
    int ret = 0;

    /* set bad data to ensure it gets reset correctly */
    ngroups = 1;
    char *returndir = get_current_dir_name();

    /* make sure fails if user is NULL */
    groups = shifter_getgrouplist(NULL, 1000, &ngroups);
    CHECK(groups == NULL && ngroups == 0);

    /* make sure fails if user is root */
    ngroups = 1;
    groups = shifter_getgrouplist("root", 1000, &ngroups);
    CHECK(groups == NULL && ngroups == 0);

    /* make sure fails if group is 0 */
    ngroups = 1;
    groups = shifter_getgrouplist("test", 0, &ngroups);
    CHECK(groups == NULL && ngroups == 0);

    /* make sure fails if ngroups is NULL */
    groups = shifter_getgrouplist("test", 1000, NULL);
    CHECK(groups == NULL);

    /* in chroot1, user dmj is in groups 10, 990, and 1000 */
    ngroups = 0;

    SETUP_CHROOT("chroot1")
    groups = shifter_getgrouplist("dmj", 1000, &ngroups);
    fprintf(stderr, "got back %d groups\n", ngroups);
    int ok[] = {10, 990, 1000};
    int expcnt[] = {1, 1, 1};
    int gotcnt[] = {0, 0, 0};

    for (int idx = 0; idx < ngroups; idx++) {
        fprintf(stderr, "have gid: %d\n", groups[idx]);
        for (int grpidx = 0; grpidx < 3; grpidx++) {
            if (groups[idx] == ok[grpidx]) {
                gotcnt[grpidx]++;
            }
        }
    }
    for (int grpidx = 0; grpidx < 3; grpidx++) {
        if (expcnt[grpidx] != gotcnt[grpidx]) {
            fprintf(stderr, "%d != %d occurences for gid %d\n", expcnt[grpidx], gotcnt[grpidx], ok[grpidx]);
            exit(1);
        } 
    }
    CHECK(ret == 0);
    CHECK(ngroups == 3);
    END_CHROOT(returndir)

    printf("currdir: %s\n", get_current_dir_name());

    /* should get back the 3 correct groups plus a duplicate
     * 1000 replacing the evil 0 inserted into chroot2 */
    SETUP_CHROOT("chroot2")
    groups = shifter_getgrouplist("dmj", 1000, &ngroups);
    fprintf(stderr, "got back %d groups\n", ngroups);
    int ok[] = {10, 990, 1000};
    int expcnt[] = {1, 1, 2};
    int gotcnt[] = {0, 0, 0};

    for (int idx = 0; idx < ngroups; idx++) {
        fprintf(stderr, "have gid: %d\n", groups[idx]);
        for (int grpidx = 0; grpidx < 3; grpidx++) {
            if (groups[idx] == ok[grpidx]) {
                gotcnt[grpidx]++;
            }
        }
    }
    for (int grpidx = 0; grpidx < 3; grpidx++) {
        if (expcnt[grpidx] != gotcnt[grpidx]) {
            fprintf(stderr, "%d != %d occurences for gid %d\n", expcnt[grpidx], gotcnt[grpidx], ok[grpidx]);
            exit(1);
        } 
    }
    CHECK_CHROOT(ret == 0 && ngroups == 4, returndir)

    /* make sure the realloc works correctly */
    free(groups);
    groups = (gid_t *) malloc(sizeof(gid_t) * 1);
    ngroups = 1;
  
    /* after making buffer too small, re-run test from above */
    SETUP_CHROOT("chroot1")
    groups = shifter_getgrouplist("dmj", 1000, &ngroups);
    fprintf(stderr, "got back %d groups\n", ngroups);
    int ok[] = {10, 990, 1000};
    int expcnt[] = {1, 1, 1};
    int gotcnt[] = {0, 0, 0};

    for (int idx = 0; idx < ngroups; idx++) {
        fprintf(stderr, "have gid: %d\n", groups[idx]);
        for (int grpidx = 0; grpidx < 3; grpidx++) {
            if (groups[idx] == ok[grpidx]) {
                gotcnt[grpidx]++;
            }
        }
    }
    for (int grpidx = 0; grpidx < 3; grpidx++) {
        if (expcnt[grpidx] != gotcnt[grpidx]) {
            fprintf(stderr, "%d != %d occurences for gid %d\n", expcnt[grpidx], gotcnt[grpidx], ok[grpidx]);
            exit(1);
        } 
    }
    CHECK_CHROOT(ret == 0 && ngroups == 3, returndir)

    /* check case when NO group entries are present
     * should just get the provided gid back */
    SETUP_CHROOT("chroot3")
    groups = shifter_getgrouplist("dmj", 1000, &ngroups);
    fprintf(stderr, "got back %d groups\n", ngroups);
    int ok[] = {1000};
    int expcnt[] = {1};
    int gotcnt[] = {0};

    for (int idx = 0; idx < ngroups; idx++) {
        fprintf(stderr, "have gid: %d\n", groups[idx]);
        for (int grpidx = 0; grpidx < 1; grpidx++) {
            if (groups[idx] == ok[grpidx]) {
                gotcnt[grpidx]++;
            }
        }
    }
    for (int grpidx = 0; grpidx < 1; grpidx++) {
        if (expcnt[grpidx] != gotcnt[grpidx]) {
            fprintf(stderr, "%d != %d occurences for gid %d\n", expcnt[grpidx], gotcnt[grpidx], ok[grpidx]);
            exit(1);
        } 
    }
    CHECK_CHROOT(ret == 0 && ngroups == 1, returndir)
}

TEST(ShifterCoreTestGroup, setupPerNodeCacheFilename_tests) {
    int ret = 0;
    char buffer[PATH_MAX];
    VolMapPerNodeCacheConfig *cache = (VolMapPerNodeCacheConfig *) malloc(sizeof(VolMapPerNodeCacheConfig));
    UdiRootConfig *config = NULL;
    ImageData *image = NULL;

    CHECK(setupLocalRootVFSConfig(&config, &image, tmpDir, cwd) == 0);

    memset(cache, 0, sizeof(VolMapPerNodeCacheConfig));

    /* should fail because cache is NULL */
    ret = setupPerNodeCacheFilename(config, NULL, buffer, 10);
    CHECK(ret != 0);

    /* should fail because buffer is NULL */
    ret = setupPerNodeCacheFilename(config, cache, NULL, 10);
    CHECK(ret != 0);

    /* should fail because buffer len is 0 */
    ret = setupPerNodeCacheFilename(config, cache, buffer, 0);
    CHECK(ret != 0);


    /* should successfully work */
    char hostname[128];
    char result[1024];
    gethostname(hostname, 128);
    snprintf(result, 1024, "/tmp/file_%s.xfs", hostname);
    cache->fstype = strdup("xfs");
    snprintf(buffer, PATH_MAX, "/tmp/file");
    ret = setupPerNodeCacheFilename(config, cache, buffer, PATH_MAX);
    CHECK(ret >= 0);
    close(ret);

    /* should fail because fstype is NULL */
    free(cache->fstype);
    cache->fstype = NULL;
    ret = setupPerNodeCacheFilename(config, cache, buffer, PATH_MAX);
    CHECK(ret == -1);

    free_ImageData(image, 1);
    free_UdiRootConfig(config, 1);
    free_VolMapPerNodeCacheConfig(cache);
    cache = NULL;
}

#ifdef NOTROOT
TEST(ShifterCoreTestGroup, setupPerNodeCacheBackingStore_tests) {
#else
IGNORE_TEST(ShifterCoreTestGroup, setupPerNodeCacheBackingStore_tests) {
#endif
    int ret = 0;
    VolMapPerNodeCacheConfig *cache = (VolMapPerNodeCacheConfig *) malloc(sizeof(VolMapPerNodeCacheConfig));
    UdiRootConfig *config = NULL;
    ImageData *image = NULL;
    char backingStorePath[PATH_MAX];
    char *ptr = NULL;

    CHECK(setupLocalRootVFSConfig(&config, &image, tmpDir, cwd) == 0);
    memset(cache, 0, sizeof(VolMapPerNodeCacheConfig));

    cache->fstype = strdup("xfs");
    cache->cacheSize = 200 * 1024 * 1024; // 200mb

    if (access("/sbin/mkfs.xfs", X_OK) == 0) {
        config->target_uid = getuid();
        config->target_gid = getgid();
        config->mkfsXfsPath = strdup("/sbin/mkfs.xfs");

        snprintf(backingStorePath, PATH_MAX, "%s/testBackingStore.xfs", tmpDir);
        ret = setupPerNodeCacheBackingStore(cache, backingStorePath, config);
        CHECK(ret == 0);

        unlink(backingStorePath);
    }

    free(cache->fstype);
    cache->fstype = NULL;
    free_UdiRootConfig(config, 1);
    free_ImageData(image, 1);
    free(cache);
}

TEST(ShifterCoreTestGroup, CheckSupportedFilesystems) {
    char **fsTypes = getSupportedFilesystems();
    char **ptr = NULL;
    int haveCommonFsType = 0;
    CHECK(fsTypes != NULL);

    haveCommonFsType = supportsFilesystem(fsTypes, "ext4") == 0;
    haveCommonFsType |= supportsFilesystem(fsTypes, "xfs") == 0;
    CHECK(supportsFilesystem(fsTypes, "proc") == 0);
    CHECK(haveCommonFsType == 1);
    CHECK(supportsFilesystem(fsTypes, "blergityboo") != 0);

    for (ptr = fsTypes; ptr && *ptr; ptr++) {
        free(*ptr);
    }
    free(fsTypes);
}

#ifdef NOTROOT
IGNORE_TEST(ShifterCoreTestGroup, CopyFile_chown) {
#else
TEST(ShifterCoreTestGroup, CopyFile_chown) {
#endif
    char *toFile = NULL;
    char *ptr = NULL;
    int ret = 0;
    struct stat statData;

    toFile = alloc_strgenf("%s/passwd", tmpDir);
    CHECK(toFile != NULL);
    
    ret = _shifterCore_copyFile("/bin/cp", "/etc/passwd", toFile, 0, 2, 2, 0644);
    tmpFiles.push_back(toFile);
    CHECK(ret == 0);

    ret = lstat(toFile, &statData);
    CHECK(ret == 0);
    CHECK((statData.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == 0644);
    CHECK(statData.st_uid == 2);
    CHECK(statData.st_gid == 2);

    ret = unlink(toFile);
    CHECK(ret == 0);

    ret = _shifterCore_copyFile("/bin/cp", "/etc/passwd", toFile, 0, 2, 2, 0755);
    CHECK(ret == 0);

    ret = lstat(toFile, &statData);
    CHECK(ret == 0);
    CHECK((statData.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == 0755)
    CHECK(statData.st_uid == 2);
    CHECK(statData.st_gid == 2);

    ret = unlink(toFile);
    CHECK(ret == 0);
    free(toFile);
}

#ifdef NOTROOT
IGNORE_TEST(ShifterCoreTestGroup, isSharedMount_basic) {
#else
TEST(ShifterCoreTestGroup, isSharedMount_basic) {
#endif
    /* main() already generated a new namespace for this process */
    CHECK(mount(NULL, "/", "", MS_SHARED, NULL) == 0);

    CHECK(isSharedMount("/") == 1);

    CHECK(mount(NULL, "/", "", MS_PRIVATE, NULL) == 0);
    CHECK(isSharedMount("/") == 0);
}

#ifdef NOTROOT
IGNORE_TEST(ShifterCoreTestGroup, validatePrivateNamespace) {
#else
TEST(ShifterCoreTestGroup, validatePrivateNamespace) {
#endif
    struct stat statInfo;
    pid_t child = 0;

    CHECK(stat("/tmp", &statInfo) == 0);
    CHECK(stat("/tmp/test_shifter_core", &statInfo) != 0);
    
    child = fork();
    if (child == 0) {
        char currDir[PATH_MAX];
        struct stat localStatInfo;

        if (unshare(CLONE_NEWNS) != 0) exit(1);
        if (isSharedMount("/") == 1) {
            if (mount(NULL, "/", "", MS_PRIVATE|MS_REC, NULL) != 0) exit(1);
        }
        if (getcwd(currDir, PATH_MAX) == NULL) exit(1);
        currDir[PATH_MAX - 1] = 0;

        if (mount(currDir, "/tmp", "bind", MS_BIND, NULL) != 0) exit(1);
        if (stat("/tmp/test_shifter_core", &localStatInfo) == 0) {
            exit(0);
        }
        exit(1);
    } else if (child > 0) {
        int status = 0;
        waitpid(child, &status, 0);

        status = WEXITSTATUS(status);
        CHECK(status == 0);

        CHECK(stat("/tmp/test_shifter_core", &statInfo) != 0);
    }
}

TEST(ShifterCoreTestGroup, writeHostFile_basic) { 
   char tmpDirVar[] = "/tmp/shifter.XXXXXX/var";
   char hostsFilename[] = "/tmp/shifter.XXXXXX/var/hostsfile";
   FILE *fp = NULL;
   char *linePtr = NULL;
   size_t linePtr_size = 0;
   int count = 0;

   memcpy(tmpDirVar, tmpDir, sizeof(char) * strlen(tmpDir));
   memcpy(hostsFilename, tmpDir, sizeof(char) * strlen(tmpDir));
   tmpDirs.push_back(tmpDirVar);
   tmpFiles.push_back(hostsFilename);

   CHECK(mkdir(tmpDirVar, 0755) == 0);

   UdiRootConfig config;
   memset(&config, 0, sizeof(UdiRootConfig));

   config.udiMountPoint = tmpDir;

   int ret = writeHostFile("host1/4", &config);
   CHECK(ret == 0);

   fp = fopen(hostsFilename, "r");
   while (fp && !feof(fp) && !ferror(fp)) {
       size_t nread = getline(&linePtr, &linePtr_size, fp);
       if (nread == 0 || feof(fp) || ferror(fp)) break;

       if (count < 4) {
           CHECK(strcmp(linePtr, "host1\n") == 0);
           count++;
           continue;
       }
       FAIL("Got beyond where we should be");
   }
   fclose(fp);

   ret = writeHostFile(NULL, &config);
   CHECK(ret == 1);

   ret = writeHostFile("host1 host2", &config);
   CHECK(ret == 1);

   ret = writeHostFile("host1/24 host2/24 host3/24", &config);
   count = 0;
   fp = fopen(hostsFilename, "r");
   while (fp && !feof(fp) && !ferror(fp)) {
       size_t nread = getline(&linePtr, &linePtr_size, fp);
       if (nread == 0 || feof(fp) || ferror(fp)) break;

       if (count < 24) {
           CHECK(strcmp(linePtr, "host1\n") == 0);
           count++;
           continue;
       } else if (count < 48) {
           CHECK(strcmp(linePtr, "host2\n") == 0);
           count++;
           continue;
       } else if (count < 72) {
           CHECK(strcmp(linePtr, "host3\n") == 0);
           count++;
           continue;
       }
       FAIL("Got beyond where we should be");
   }
   fclose(fp);

   if (linePtr != NULL) free(linePtr);
}

#ifdef NOTROOT
IGNORE_TEST(ShifterCoreTestGroup, validateUnmounted_Basic) { 
#else
TEST(ShifterCoreTestGroup, validateUnmounted_Basic) { 
#endif
    int rc = 0;
    MountList mounts;
    UdiRootConfig config;
    memset(&config, 0, sizeof(UdiRootConfig));

    memset(&mounts, 0, sizeof(MountList));
    CHECK(parse_MountList(&mounts) == 0);

    rc = validateUnmounted(tmpDir, 0);
    CHECK(rc == 0);

    CHECK(_shifterCore_bindMount(&config, &mounts, "/", tmpDir, 1, 0) == 0);
    
    rc = validateUnmounted(tmpDir, 0);
    CHECK(rc == 1);

    CHECK(unmountTree(&mounts, tmpDir) == 0);

    rc = validateUnmounted(tmpDir, 0);
    CHECK(rc == 0);

    free_MountList(&mounts, 0);
}

#ifdef NOTROOT
IGNORE_TEST(ShifterCoreTestGroup, validateLocalTypeIsConfigurable) {
#else
TEST(ShifterCoreTestGroup, validateLocalTypeIsConfigurable) {
#endif
    UdiRootConfig *config = NULL;
    ImageData *image = NULL;
    MountList mounts;
    int rc = 0;
    memset(&mounts, 0, sizeof(MountList));
    CHECK(setupLocalRootVFSConfig(&config, &image, tmpDir, cwd) == 0);
    config->allowLocalChroot = 0;

    fprint_ImageData(stderr, image);

    rc = mountImageVFS(image, "dmj", NULL, 0, NULL, config);
    CHECK(rc == 1);
    CHECK(parse_MountList(&mounts) == 0);
    CHECK(find_MountList(&mounts, tmpDir) == NULL);

    rc = unmountTree(&mounts, config->udiMountPoint);
    CHECK(rc == 0);
    free_MountList(&mounts, 0);
    memset(&mounts, 0, sizeof(MountList));

    config->allowLocalChroot = 1;
    rc = mountImageVFS(image, "dmj", NULL, 0, NULL, config);
    CHECK(rc == 0);
    CHECK(parse_MountList(&mounts) == 0);
    CHECK(find_MountList(&mounts, tmpDir) != NULL);
    rc = unmountTree(&mounts, config->udiMountPoint);
    free_UdiRootConfig(config, 1);
    free_ImageData(image, 1);
    free_MountList(&mounts, 0);
}

#ifdef NOTROOT
IGNORE_TEST(ShifterCoreTestGroup, _bindMount_basic) {
#else
TEST(ShifterCoreTestGroup, _bindMount_basic) {
#endif
    MountList mounts;
    int rc = 0;
    struct stat statData;
    UdiRootConfig config;
    memset(&config, 0, sizeof(UdiRootConfig));
    memset(&mounts, 0, sizeof(MountList));
    memset(&statData, 0, sizeof(struct stat));

    CHECK(parse_MountList(&mounts) == 0);

    rc = _shifterCore_bindMount(&config, &mounts, "/", tmpDir, 0, 0);
    CHECK(rc == 0);

    char *usrPath = alloc_strgenf("%s/%s", tmpDir, "usr");
    char *test_shifter_corePath = alloc_strgenf("%s/%s", tmpDir, "test_shifter_core");
    CHECK(usrPath != NULL);
    CHECK(test_shifter_corePath != NULL);

    /* make sure we can see /usr in the bind-mount location */
    CHECK(stat(usrPath, &statData) == 0);
    CHECK(find_MountList(&mounts, tmpDir) != NULL);

    /* make sure that without overwrite set the mount is unchanged */
    CHECK(_shifterCore_bindMount(&config, &mounts, cwd, tmpDir, 0, 0) != 0);
    CHECK(stat(test_shifter_corePath, &statData) != 0);
    CHECK(stat(usrPath, &statData) == 0);
    CHECK(find_MountList(&mounts, tmpDir) != NULL);

    /* set overwrite and make sure that works */
    CHECK(_shifterCore_bindMount(&config, &mounts, cwd, tmpDir, 0, 1) == 0);
    CHECK(stat(test_shifter_corePath, &statData) == 0);
    CHECK(stat(usrPath, &statData) != 0);
    CHECK(find_MountList(&mounts, tmpDir) != NULL);

    /* make sure that the directory is writable */
    char *tmpFile = alloc_strgenf("%s/testFile.XXXXXX", tmpDir);
    int fd = mkstemp(tmpFile);
    CHECK(fd >= 0);
    CHECK(stat(tmpFile, &statData) == 0);
    close(fd);
    CHECK(unlink(tmpFile) == 0);
    free(tmpFile);

    /* remount with read-only set */
    CHECK(_shifterCore_bindMount(&config, &mounts, cwd, tmpDir, 1, 1) == 0);
    tmpFile = alloc_strgenf("%s/testFile.XXXXXX", tmpDir);
    fd = mkstemp(tmpFile);
    CHECK(fd < 0);
    CHECK(stat(tmpFile, &statData) != 0);
    free(tmpFile);

    /* clean up */
    CHECK(unmountTree(&mounts, tmpDir) == 0);
    free_MountList(&mounts, 0);
    free(usrPath);
    free(test_shifter_corePath);
}

#if ISROOT & DANGEROUSTESTS
TEST(ShifterCoreTestGroup, mountDangerousImage) {
#else
IGNORE_TEST(ShifterCoreTestGroup, mountDangerousImage) {
#endif

}

struct directory_entry
{
    directory_entry(const std::string& path, const std::string& realpath)
        : path(path)
        , realpath(realpath)
    {}

    bool operator==(const directory_entry& rhs) const
    {
        return path == rhs.path && realpath == rhs.realpath;
    }

    bool operator<(const directory_entry& rhs) const
    {
        return path < rhs.path;
    }

    std::string path;
    std::string realpath;   //this value is different than path when the entry
                            //is a symlink (it takes the value of the symlink's target)
};

std::vector<directory_entry> get_sorted_directory_entries(const std::string& dir_name)
{
    DIR* dir = opendir(dir_name.c_str());
    CHECK(dir != NULL);

    std::vector<directory_entry> entries;
    dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if( std::string(".") == entry->d_name
            || std::string("..") == entry->d_name
            || std::string("stdin") == entry->d_name
            || std::string("stdout") == entry->d_name
            || std::string("stderr") == entry->d_name)
            continue;

        std::string entry_abs_path = dir_name + "/" + entry->d_name;
        char entry_realpath[PATH_MAX];
        CHECK(realpath(entry_abs_path.c_str(), entry_realpath) != NULL);
        entries.push_back(directory_entry(entry_abs_path, entry_realpath));
    }
    std::sort(entries.begin(), entries.end());
    return entries;
}

#if ISROOT
TEST(ShifterCoreTestGroup, bindmount_dev_contents)
#else
IGNORE_TEST(ShifterCoreTestGroup, bindmount_dev_contents)
#endif
{
    MountList mounts = {};
    UdiRootConfig config = {};
    config.udiMountPoint = tmpDir;
    std::string container_dev_path = std::string(tmpDir) + "/dev";

    CHECK(mkdir(container_dev_path.c_str(), 0755) == 0);
    CHECK(parse_MountList(&mounts) == 0);
    CHECK(bindmount_dev_contents(&config, &mounts) == 0);

    // get entries in /dev (host) and bind mounted /dev (container)
    std::vector<directory_entry> host_entries = get_sorted_directory_entries("/dev");
    std::vector<directory_entry> container_entries = get_sorted_directory_entries(container_dev_path);

    // check value of entries
    CHECK( host_entries.size() > 0 );
    CHECK( host_entries.size() == container_entries.size() );

    typedef std::vector<directory_entry>::const_iterator dir_entry_it;
    for(dir_entry_it host_entry=host_entries.begin(), container_entry=container_entries.begin();
        host_entry != host_entries.end();
        ++host_entry, ++container_entry)
    {
        bool is_symlink = (host_entry->path != host_entry->realpath);
        if(is_symlink)
        {
            CHECK(tmpDir + host_entry->path == container_entry->path);
            CHECK(host_entry->realpath == container_entry->realpath);
        }
        else
        {
            CHECK(tmpDir + host_entry->path == container_entry->path);
            CHECK(tmpDir + host_entry->realpath == container_entry->realpath);
        }
    }

    // clean up
    CHECK(unmountTree(&mounts, tmpDir) == 0);
    free_MountList(&mounts, 0);
}

TEST(ShifterCoreTestGroup, copyenv_test) {
    char **copied_env = NULL;
    char **eptr = NULL;
    char **cptr = NULL;
    setenv("ABCD", "DCBA", 1);

    copied_env = shifter_copyenv(environ, 0);
    CHECK(copied_env != NULL);

    /* environment variables should be identical and in
       same order, but in different memory segments */
    for (eptr = environ, cptr = copied_env;
         eptr && *eptr && cptr && *cptr;
         eptr++, cptr++)
    {
        CHECK(strcmp(*eptr, *cptr) == 0);
        CHECK(*eptr != *cptr);
    } 
    CHECK(*eptr == NULL && *cptr == NULL);

    for (cptr = copied_env; cptr && *cptr; cptr++) {
        free(*cptr);
    }
    free(copied_env);
}

TEST(ShifterCoreTestGroup, setenv_test) {
    char **copied_env = NULL;
    char **cptr = NULL;
    char *tmpvar = strdup("FAKE_ENV_VAR_FOR_TEST=3");
    const char *pathenv = getenv("PATH");
    int pathok = 0;
    int ret = 0;
    int found = 0;
    size_t cnt = 0;
    size_t newcnt = 0;

    unsetenv("FAKE_ENV_VAR_FOR_TEST");
    copied_env = shifter_copyenv(environ, 0);
    CHECK(copied_env != NULL);
    for (cptr = copied_env; cptr && *cptr; cptr++) {
        cnt++;
    }

    ret = shifter_putenv(&copied_env, tmpvar);
    CHECK(ret == 0);
    
    /* make sure we cannot compare against original string */
    tmpvar[0] = 0;
    free(tmpvar);

    for (cptr = copied_env; cptr && *cptr; cptr++) {
        if (strcmp(*cptr, "FAKE_ENV_VAR_FOR_TEST=3") == 0) {
            found = 1;
        }
        if (strncmp(*cptr, "PATH=", 5) == 0) {
            char *ptr = *cptr + 5;
            if (strcmp(ptr, pathenv) == 0) {
                pathok = 1;
            }
        }
        newcnt++;
    }
    CHECK(found == 1);
    CHECK(newcnt == cnt + 1);
    CHECK(pathok == 1);


    for (cptr = copied_env; cptr && *cptr; cptr++) {
        free(*cptr);
    }
    free(copied_env);
}

TEST(ShifterCoreTestGroup, appendenv_test) {
    char **copied_env = NULL;
    char **cptr = NULL;
    char *tmpvar = strdup("FAKE_ENV_VAR_FOR_TEST=3");
    const char *pathenv = getenv("PATH");
    int pathok = 0;
    int ret = 0;
    int found = 0;
    size_t cnt = 0;
    size_t newcnt = 0;

    setenv("FAKE_ENV_VAR_FOR_TEST", "4:5", 1);
    copied_env = shifter_copyenv(environ, 0);
    CHECK(copied_env != NULL);
    for (cptr = copied_env; cptr && *cptr; cptr++) {
        cnt++;
    }

    ret = shifter_appendenv(&copied_env, tmpvar);
    CHECK(ret == 0);
    
    /* make sure we cannot compare against original string */
    tmpvar[0] = 0;
    free(tmpvar);

    for (cptr = copied_env; cptr && *cptr; cptr++) {
        if (strcmp(*cptr, "FAKE_ENV_VAR_FOR_TEST=4:5:3") == 0) {
            found = 1;
        }
        if (strncmp(*cptr, "PATH=", 5) == 0) {
            char *ptr = *cptr + 5;
            if (strcmp(ptr, pathenv) == 0) {
                pathok = 1;
            }
        }
        newcnt++;
    }
    CHECK(found == 1);
    CHECK(cnt == newcnt);
    CHECK(pathok == 1);


    for (cptr = copied_env; cptr && *cptr; cptr++) {
        free(*cptr);
    }
    free(copied_env);
    unsetenv("FAKE_ENV_VAR_FOR_TEST");
}

TEST(ShifterCoreTestGroup, prependenv_test) {
    char **copied_env = NULL;
    char **cptr = NULL;
    char *tmpvar = strdup("FAKE_ENV_VAR_FOR_TEST=3");
    const char *pathenv = getenv("PATH");
    int pathok = 0;
    int ret = 0;
    int found = 0;
    size_t cnt = 0;
    size_t newcnt = 0;

    setenv("FAKE_ENV_VAR_FOR_TEST", "4:5", 1);
    copied_env = shifter_copyenv(environ, 0);
    CHECK(copied_env != NULL);
    for (cptr = copied_env; cptr && *cptr; cptr++) {
        cnt++;
    }

    ret = shifter_prependenv(&copied_env, tmpvar);
    CHECK(ret == 0);
    
    /* make sure we cannot compare against original string */
    tmpvar[0] = 0;
    free(tmpvar);

    for (cptr = copied_env; cptr && *cptr; cptr++) {
        if (strcmp(*cptr, "FAKE_ENV_VAR_FOR_TEST=3:4:5") == 0) {
            found = 1;
        }
        if (strncmp(*cptr, "PATH=", 5) == 0) {
            char *ptr = *cptr + 5;
            if (strcmp(ptr, pathenv) == 0) {
                pathok = 1;
            }
        }
        newcnt++;
    }
    CHECK(found == 1);
    CHECK(cnt == newcnt);
    CHECK(pathok == 1);


    for (cptr = copied_env; cptr && *cptr; cptr++) {
        free(*cptr);
    }
    free(copied_env);
    unsetenv("FAKE_ENV_VAR_FOR_TEST");
}

TEST(ShifterCoreTestGroup, unsetenv_test) {
    char **copied_env = NULL;
    char **cptr = NULL;
    char *tmpvar = strdup("FAKE_ENV_VAR_FOR_TEST");
    const char *pathenv = getenv("PATH");
    int pathok = 0;
    int ret = 0;
    int found = 0;
    size_t cnt = 0;
    size_t newcnt = 0;

    setenv("FAKE_ENV_VAR_FOR_TEST", "4:5", 1);
    copied_env = shifter_copyenv(environ, 0);
    CHECK(copied_env != NULL);
    for (cptr = copied_env; cptr && *cptr; cptr++) {
        cnt++;
    }

    ret = shifter_unsetenv(&copied_env, tmpvar);
    CHECK(ret == 0);
    
    /* make sure we cannot compare against original string */
    tmpvar[0] = 0;
    free(tmpvar);

    for (cptr = copied_env; cptr && *cptr; cptr++) {
        if (strncmp(*cptr, "FAKE_ENV_VAR_FOR_TEST=", 22) == 0) {
            found = 1;
        }
        if (strncmp(*cptr, "PATH=", 5) == 0) {
            char *ptr = *cptr + 5;
            if (strcmp(ptr, pathenv) == 0) {
                pathok = 1;
            }
        }
        newcnt++;
    }
    CHECK(found == 0);
    CHECK(newcnt + 1 == cnt);
    CHECK(pathok == 1);


    for (cptr = copied_env; cptr && *cptr; cptr++) {
        free(*cptr);
    }
    free(copied_env);
    unsetenv("FAKE_ENV_VAR_FOR_TEST");
}

TEST(ShifterCoreTestGroup, setupenv_test) {
    UdiRootConfig *config = (UdiRootConfig *) malloc(sizeof(UdiRootConfig));
    ImageData *image = (ImageData *) malloc(sizeof(ImageData));
    char **ptr = NULL;
    char **local_env = NULL;

    memset(config, 0, sizeof(UdiRootConfig));
    memset(image, 0, sizeof(ImageData));
   
    /* initialize empty environment */
    local_env = (char **) malloc(sizeof(char *) * 2);
    local_env[0] = strdup("PATH=/incorrect");
    local_env[1] = NULL;

    /* copy arrays into config */
    config->siteEnv = (char **) malloc(sizeof(char *) * 3);
    config->siteEnv[0] = strdup("SHIFTER_RUNTIME=1");
    config->siteEnv[1] = strdup("NEW_VAR=abcd");
    config->siteEnv[2] = NULL;

    config->siteEnvAppend = (char **) malloc(sizeof(char *) * 2);
    config->siteEnvAppend[0] = strdup("PATH=/opt/udiImage/bin");
    config->siteEnvAppend[1] = NULL;

    config->siteEnvPrepend = (char **) malloc(sizeof(char *) * 2);
    config->siteEnvPrepend[0] = strdup("PATH=/sbin");
    config->siteEnvPrepend[1] = NULL;

    config->siteEnvUnset = (char **) malloc(sizeof(char *) * 2);
    config->siteEnvUnset[0] = strdup("NEW_VAR");
    config->siteEnvUnset[1] = NULL;

    /* setup image environment */
    image->env = (char **) malloc(sizeof(char *) * 2);
    image->env[0] = strdup("PATH=/usr/bin");
    image->env[1] = NULL;

    /* test target */
    int ret = shifter_setupenv(&local_env, image, config);

    CHECK(ret == 0);

    int found = 0;
    for (ptr = local_env ; ptr && *ptr; ptr++) {
        if (strcmp(*ptr, "PATH=/sbin:/usr/bin:/opt/udiImage/bin") == 0) {
            found++;
        }
        if (strcmp(*ptr, "SHIFTER_RUNTIME=1") == 0) {
            found++;
        }
    }
    CHECK(found == 2);
    CHECK(ptr - local_env == 2);

    free_ImageData(image, 1);
    free_UdiRootConfig(config, 1);
}

TEST(ShifterCoreTestGroup, setupenv_gpu_support_test) {
    UdiRootConfig *config = (UdiRootConfig *) malloc(sizeof(UdiRootConfig));
    char **local_env = NULL;

    memset(config, 0, sizeof(UdiRootConfig));

    /* initialize empty environment */
    local_env = (char **) malloc(sizeof(char *) * 2);
    local_env[0] = strdup("PATH=/usr/bin");
    local_env[1] = strdup("LD_LIBRARY_PATH=/usr/lib");
    local_env[2] = NULL;

    /* copy arrays into config */
    config->siteEnv = (char **) malloc(sizeof(char *) * 2);
    config->siteEnv[0] = NULL;

    config->siteResources = strdup("/site-resources");
    config->gpuBinPath = strdup("/site-resources/gpu/bin");
    config->gpuLibPath = strdup("/site-resources/gpu/lib");
    config->gpuLib64Path = strdup("/site-resources/gpu/lib64");


    const char* gpu_ids = "0";

    /* test target */
    int ret = shifter_setupenv_gpu_support(&local_env, config, gpu_ids);

    CHECK(ret == 0);

    int found = 0;
    char **ptr = NULL;
    for (ptr = local_env ; ptr && *ptr; ptr++) {
        if (strcmp(*ptr, "PATH=/site-resources/gpu/bin:/usr/bin") == 0) {
            found++;
        }
        if (strcmp(*ptr, "LD_LIBRARY_PATH=/site-resources/gpu/lib64:/site-resources/gpu/lib:/usr/lib") == 0) {
            found++;
        }
    }
    CHECK(found == 2);
    CHECK(ptr - local_env == 2);

    free_UdiRootConfig(config, 1);
}

TEST(ShifterCoreTestGroup, shifterRealpath_test) {
    UdiRootConfig *config = (UdiRootConfig *) malloc(sizeof(UdiRootConfig));
    memset(config, 0, sizeof(UdiRootConfig));
    char buffer[PATH_MAX];

    config->udiMountPoint = strdup(tmpDir);

    snprintf(buffer, PATH_MAX, "%s/test", tmpDir);
    mkdir(buffer, 0755);
    snprintf(buffer, PATH_MAX, "%s/test/path", tmpDir);
    mkdir(buffer, 0755);
    snprintf(buffer, PATH_MAX, "%s/test/path/rellink", tmpDir);
    symlink("../../../../../../../../test", buffer);
    snprintf(buffer, PATH_MAX, "%s/test/path/abslink", tmpDir);
    symlink("/test/path", buffer);

    char *result = shifter_realpath("test/path", config);
    CHECK(result != NULL);
    snprintf(buffer, PATH_MAX, "%s/test/path", tmpDir);
    CHECK(strcmp(result, buffer) == 0);
    free(result);

    result = shifter_realpath("test/path/rellink/path", config);
    CHECK(result != NULL);
    snprintf(buffer, PATH_MAX, "%s/test/path", tmpDir);
    CHECK(strcmp(result, buffer) == 0);
    free(result);

    result = shifter_realpath("test/path/abslink", config);
    CHECK(result != NULL);
    snprintf(buffer, PATH_MAX, "%s/test/path", tmpDir);
    CHECK(strcmp(result, buffer) == 0);

    free(result);
    free_UdiRootConfig(config, 1);
}

#if ISROOT
TEST(ShifterCoreTestGroup, destructUDI_test) {
#else
IGNORE_TEST(ShifterCoreTestGroup, destructUDI_test) {
#endif
    UdiRootConfig *config = NULL;
    ImageData *image = NULL;
    MountList mounts;
    struct stat statData;
    int rc = 0;
    memset(&mounts, 0, sizeof(MountList));

    CHECK(setupLocalRootVFSConfig(&config, &image, tmpDir, cwd) == 0);
    config->allowLocalChroot = 1;
    CHECK(mountImageVFS(image, "dmj", NULL, 0, NULL, config) == 0);

    CHECK(parse_MountList(&mounts) == 0);
    CHECK(find_MountList(&mounts, tmpDir) != NULL);
    
    free_MountList(&mounts, 0);
    memset(&mounts, 0, sizeof(MountList));

    CHECK(destructUDI(config, 0) == 0);
    CHECK(parse_MountList(&mounts) == 0);
    CHECK(find_MountList(&mounts, tmpDir) == NULL);
    free_MountList(&mounts, 0);

    /* i haven't found a way to reliably make destructUDI fail, so for now
     * that case will not be tested
     */


    free_ImageData(image, 1);
    free_UdiRootConfig(config, 1);
}

int main(int argc, char** argv) {
    if (getuid() == 0) {
#ifndef NOTROOT
        char buffer[PATH_MAX];
        CHECK(getcwd(buffer, PATH_MAX) != NULL);
        if (unshare(CLONE_NEWNS) != 0) {
            fprintf(stderr, "FAILED to unshare, test handler will exit in error.\n");
            exit(1);
        }
        CHECK(chdir(buffer) == 0);
#endif
    }
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
