/**
 *  @file shifter.c
 *  @brief setuid utility to setup and interactively enter a shifter env
 * 
 * @author Douglas M. Jacobsen <dmjacobsen@lbl.gov>
 */

/* Shifter, Copyright (c) 2015, The Regents of the University of California,
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *  
 * You are under no obligation whatsoever to provide any bug fixes, patches, or
 * upgrades to the features, functionality or performance of the source code
 * ("Enhancements") to anyone; however, if you choose to make your Enhancements
 * available either publicly, or directly to Lawrence Berkeley National
 * Laboratory, without imposing a separate written license agreement for such
 * Enhancements, then you hereby grant the following license: a  non-exclusive,
 * royalty-free perpetual license to install, use, modify, prepare derivative
 * works, incorporate into other computer software, distribute, and sublicense
 * such enhancements or derivative works thereof, in binary and source code
 * form.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sched.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "UdiRootConfig.h"
#include "shifter_core.h"
#include "ImageData.h"
#include "utility.h"

extern char **environ;

#define VOLUME_ALLOC_BLOCK 10

#ifndef VERSION
#define VERSION "0Test0"
#endif


struct options {
    char *request;
    char *imageType;
    char *imageIdentifier;
    char *rawVolumes;
    uid_t tgtUid;
    gid_t tgtGid;
    char **args;
    char **volumeStrings;
    char **volumeMapFrom;
    char **volumeMapTo;
    char **volumeMapFlags;
    int verbose;
    int useEntryPoint;

    /* state variables only used at parse time */
    size_t volumeMap_capacity;
    char **volumeMapFrom_ptr;
    char **volumeMapTo_ptr;
    char **volumeMapFlags_ptr;
};


static void _usage(int);
static void _version(void);
static char *_filterString(const char *input);
static int _appendStringArray(const char *target, size_t nChars, char ***wptr,
        char ***array, size_t *capacity, size_t allocationBlock);
char **copyenv(void);
int parse_options(int argc, char **argv, struct options *opts);
int parse_environment(struct options *opts);
int parse_volume_mounts(struct options *opts);
int fprint_options(FILE *, struct options *);
void free_options(struct options *);
int appendVolumeMap(struct options *config, char *volumeDesc);
int local_putenv(char ***environ, const char *newVar);

#ifndef _TESTHARNESS_SHIFTER
int main(int argc, char **argv) {

    /* save a copy of the environment for the exec */
    char **environ_copy = copyenv();

    /* declare needed variables */
    char wd[PATH_MAX];
    char exec[PATH_MAX];
    char udiRoot[PATH_MAX];
    uid_t uid,eUid;
    gid_t gid,eGid;
    gid_t *gidList = NULL;
    int nGroups = 0;
    int idx = 0;
    struct options opts;
    UdiRootConfig udiConfig;
    ImageData imageData;
    memset(&opts, 0, sizeof(struct options));
    memset(&udiConfig, 0, sizeof(UdiRootConfig));
    memset(&imageData, 0, sizeof(ImageData));

    if (parse_environment(&opts) != 0) {
        fprintf(stderr, "FAILED to parse environment\n");
        exit(1);
    }

    /* destroy this environment */
    clearenv();

    if (parse_UdiRootConfig(CONFIG_FILE, &udiConfig, 0) != 0) {
        fprintf(stderr, "FAILED to parse udiRoot configuration.\n");
        exit(1);
    }

    /* parse config file and command line options */
    if (parse_options(argc, argv, &opts) != 0) {
        fprintf(stderr, "FAILED to parse command line arguments.\n");
        exit(1);
    }

    /* discover information about this image */
    if (parse_ImageData(opts.imageIdentifier, &udiConfig, &imageData) != 0) {
        fprintf(stderr, "FAILED to find requested image.\n");
        exit(1);
    }

    /* check if entrypoint is defined and desired */
    if (opts.useEntryPoint == 1) {
        if (imageData.entryPoint != NULL) {
            opts.args[0] = strdup(imageData.entryPoint);
        } else {
            fprintf(stderr, "Image does not have a defined entrypoint.\n");
        }
    }

    snprintf(udiRoot, PATH_MAX, "%s%s", udiConfig.nodeContextPrefix, udiConfig.udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;

    /* figure out who we are and who we want to be */
    uid = getuid();
    eUid = geteuid();
    gid = getgid();
    eGid = getegid();

    if (opts.tgtUid == 0) opts.tgtUid = uid;
    if (opts.tgtGid == 0) opts.tgtGid = gid;

    nGroups = getgroups(0, NULL);
    if (nGroups > 0) {
        gidList = (gid_t *) malloc(sizeof(gid_t) * nGroups);
        if (gidList == NULL) {
            fprintf(stderr, "Failed to allocate memory for group list\n");
            exit(1);
        }
        if (getgroups(nGroups, gidList) == -1) {
            fprintf(stderr, "Failed to get supplementary group list\n");
            exit(1);
        }
        for (idx = 0; idx < nGroups; ++idx) {
            if (gidList[idx] == 0) {
                gidList[idx] = opts.tgtGid;
            }
        }
    }

    if (eUid != 0 && eGid != 0) {
        fprintf(stderr, "%s\n", "Not running with root privileges, will fail.");
        exit(1);
    }
    if (opts.tgtUid == 0 || opts.tgtGid == 0) {
        fprintf(stderr, "%s\n", "Will not run as root.");
        exit(1);
    }

    if (unshare(CLONE_NEWNS) != 0) {
        perror("Failed to unshare the filesystem namespace.");
        exit(1);
    }
    if (setresuid(0, 0, 0) != 0) {
        fprintf(stderr, "Failed to setuid to %d\n", 0);
        exit(1);
    }

    /* run setupRoot here */
    snprintf(exec, PATH_MAX, "%s%s/sbin/setupRoot", udiConfig.nodeContextPrefix, udiConfig.udiRootPath);
    exec[PATH_MAX-1] = 0;
    {
        char *args[] = {exec, opts.imageType, opts.imageIdentifier, NULL};
        if (forkAndExecv(args) != 0) {
            fprintf(stderr, "FAILED to run setupRoot!\n");
            exit(1);
        }
    }


    /* keep cwd to switch back to it (if possible), after chroot */
    if (getcwd(wd, PATH_MAX) == NULL) {
        perror("Failed to determine current working directory: ");
        exit(1);
    }
    wd[PATH_MAX-1] = 0;

    /* switch to new / to prevent the chroot jail from being leaky */
    if (chdir(udiRoot) != 0) {
        perror("Failed to switch to root path: ");
        exit(1);
    }

    /* chroot into the jail */
    if (chroot(udiRoot) != 0) {
        perror("Could not chroot: ");
        exit(1);
    }

    /* drop privileges */
    if (setgroups(nGroups, gidList) != 0) {
        fprintf(stderr, "Failed to setgroups\n");
        exit(1);
    }
    if (setresgid(opts.tgtGid, opts.tgtGid, opts.tgtGid) != 0) {
        fprintf(stderr, "Failed to setgid to %d\n", opts.tgtGid);
        exit(1);
    }
    if (setresuid(opts.tgtUid, opts.tgtUid, opts.tgtUid) != 0) {
        fprintf(stderr, "Failed to setuid to %d\n", opts.tgtUid);
        exit(1);
    }

    /* chdir (within chroot) to where we belong again */
    if (chdir(wd) != 0) {
        fprintf(stderr, "Failed to switch to original cwd: %s\n", wd);
        exit(1);
    }

    /* source the environment variables from the image */
    char **envPtr = NULL;
    for (envPtr = imageData.env; *envPtr != NULL; envPtr++) {
        local_putenv(&environ_copy, *envPtr);
    }

    execve(opts.args[0], opts.args, environ_copy);
    return 0;
}
#endif

/* local_putenv
 * Provides similar functionality to linux putenv, but on a targetted
 * environment.  Expects all strings to be in "var=value" format.
 * Expects environment to be unsorted (linear search). The environ
 * may be reallocated by this code if it needs to add to the environment.
 * newVar will not be changed.
 *
 * environ: pointer to pointer to NULL-terminated array of points to key/value
 *          strings
 * newVar: key/value string to replace, add to environment
 */
int local_putenv(char ***environ, const char *newVar) {
    const char *ptr = NULL;
    size_t envSize = 0;
    int nameSize = 0;
    char **envPtr = NULL;

    if (environ == NULL || newVar == NULL || *environ == NULL) return 1;
    ptr = strchr(newVar, '=');
    if (ptr == NULL) {
        fprintf(stderr, "WARNING: cannot parse container environment variable: %s\n", newVar);
        return 1;
    }
    nameSize = ptr - newVar;

    for (envPtr = *environ; *envPtr != NULL; envPtr++) {
        if (strncmp(*envPtr, newVar, nameSize) == 0) {
            free(*envPtr);
            *envPtr = strdup(newVar);
            return 0;
        }
        envSize++;
    }

    /* did not find newVar in the environment, need to add it */
    char **tmp = (char **) realloc(*environ, sizeof(char *) * (envSize + 2));
    if (tmp == NULL) {
        fprintf(stderr, "WARNING: failed to add %*s to the environment, out of memory.\n", nameSize, newVar);
        return 1;
    }
    *environ = tmp;
    (*environ)[envSize++] = strdup(newVar);
    (*environ)[envSize++] = NULL;
    return 0;
}

int parse_options(int argc, char **argv, struct options *config) {
    int opt = 0;
    static struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"volume", 1, 0, 'V'},
        {"verbose", 0, 0, 'v'},
        {"user", 1, 0, 0},
        {"image", 1, 0, 'i'},
        {"entry", 0, 0, 'e'},
        {0, 0, 0, 0}
    };
    char *ptr = NULL;


    optind = 1;
    for ( ; ; ) {
        int longopt_index = 0;
        opt = getopt_long(argc, argv, "hvV:i:e", long_options, &longopt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0:
                {
                    if (strcmp(long_options[longopt_index].name, "user") == 0) {
                        struct passwd *pwd = NULL;
                        if (optarg == NULL) {
                            fprintf(stderr, "Must specify user with --user flag.\n");
                            _usage(1);
                        }
                        pwd = getpwnam(optarg);
                        if (pwd != NULL) {
                            config->tgtUid = pwd->pw_uid;
                            config->tgtGid = pwd->pw_gid;
                        } else {
                            uid_t uid = atoi(optarg);
                            if (uid != 0) {
                                pwd = getpwuid(uid);
                                config->tgtUid = pwd->pw_uid;
                                config->tgtGid = pwd->pw_gid;
                            } else {
                                fprintf(stderr, "Cannot run as root.\n");
                                _usage(1);
                            }
                        }
                    }
                }
                break;
            case 'v': config->verbose = 1; break;
            case 'V':
                {
                    if (optarg == NULL) break;
                    size_t raw_capacity = 0;
                    size_t new_capacity = strlen(optarg);
                    if (config->rawVolumes != NULL) {
                        raw_capacity = strlen(config->rawVolumes);
                    }
                    config->rawVolumes = (char *) realloc(config->rawVolumes, sizeof(char) * (raw_capacity + new_capacity + 2));
                    char *ptr = config->rawVolumes + raw_capacity;
                    snprintf(ptr, new_capacity + 2, "%s,", optarg);
                    break;
                }
            case 'i':
                ptr = strchr(optarg, ':');
                if (ptr == NULL) {
                    fprintf(stderr, "Incorrect format for image identifier:  need \"image_type:image_id\"\n");
                    _usage(1);
                    break;
                }
                *ptr = 0;
                config->imageType = _filterString(optarg);
                config->imageIdentifier = _filterString(ptr);
                break;
            case 'e':
                config->useEntryPoint = 1;
                break;
            case '?':
                fprintf(stderr, "Missing an argument!\n");
                _usage(1);
                break;
            default:
                break;
        }
    }

    if (config->rawVolumes != NULL) {
        /* remove trailing comma */
        size_t len = strlen(config->rawVolumes);
        if (config->rawVolumes[len - 1] == ',') {
            config->rawVolumes[len - 1] = 0;
        }
    }

    int remaining = argc - optind;
    if (config->useEntryPoint == 1) {
        char **argsPtr = NULL;
        config->args = (char **) malloc(sizeof(char *) * (remaining + 2));
        argsPtr = config->args;
        *argsPtr++ = (char *) 0x1; /* leave space for entry point */
        for ( ; optind < argc; optind++) {
            *argsPtr++ = strdup(argv[optind]);
        }
        *argsPtr = NULL;
    } else if (remaining > 0) {
        /* interpret all remaining arguments as the intended command */
        char **argsPtr = NULL;
        config->args = (char **) malloc(sizeof(char *) * (remaining + 1));
        for (argsPtr = config->args; optind < argc; optind++) {
            *argsPtr++ = strdup(argv[optind]);
        }
        *argsPtr = NULL;
    } else if (getenv("SHELL") != NULL) {
        /* use the current shell */
        config->args = (char **) malloc(sizeof(char *) * 2);
        config->args[0] = strdup(getenv("SHELL"));
        config->args[1] = NULL;
    } else {
        /* use /bin/sh */
        config->args = (char **) malloc(sizeof(char*) * 2);
        config->args[0] = strdup("/bin/sh");
        config->args[1] = NULL;
    }

    /* validate and organize any user-requested bind-mounts */
    if (parse_volume_mounts(config) != 0) {
        _usage(1);
    }

    return 0;
}

int parse_environment(struct options *opts) {
    char *envPtr = NULL;

    if ((envPtr = getenv("SHIFTER_IMAGETYPE")) != NULL) {
        opts->imageType = strdup(envPtr);
    }
    if ((envPtr = getenv("SHIFTER_IMAGE")) != NULL) {
        opts->imageIdentifier = strdup(envPtr);
    }
    if ((envPtr = getenv("SHIFTER")) != NULL) {
        opts->request = strdup(envPtr);
    }
    if ((envPtr = getenv("SHIFTER_VOLUME")) != NULL) {
        opts->rawVolumes = strdup(envPtr);
    }

    return 0;
}

int parse_volume_mounts(struct options *config) {
    int n_bindMounts = 0;
    size_t rawVolumesLen = 0;
    char *ptr = NULL;
    char *eptr = NULL;
    char **bindMountStrings = NULL;
    char **bindMountPtr = NULL;
    size_t bindMountStrings_capacity = 0;
    int ret = 0;

    if (config == NULL) return -1;
    if (config->rawVolumes == NULL) return 0; /* no error if none */

    rawVolumesLen = strlen(config->rawVolumes);
    if (rawVolumesLen == 0) return 0; /* no error if none */

    /* count how many mounts we see */
    n_bindMounts = 0;
    ptr = config->rawVolumes;
    while (ptr < config->rawVolumes + rawVolumesLen) {

        /* find end of current substring */
        eptr = strchr(ptr, ',');
        if (eptr == NULL) {
            eptr = config->rawVolumes + rawVolumesLen;
        }

        /* copy substring into array */
        ret = _appendStringArray(
                ptr, eptr - ptr, &bindMountPtr, &bindMountStrings,
                &bindMountStrings_capacity, VOLUME_ALLOC_BLOCK
        );
        if (ret != 0) {
            ret *= -1;
            goto _parseVolumeMounts_end;
        }

        /* increment to next substring */
        ptr = eptr + 1;
        n_bindMounts++;
    }

    /* sort array - important for comparing mounts */
    qsort(bindMountStrings, bindMountPtr - bindMountStrings, sizeof(char *), strcmp);

    for (bindMountPtr = bindMountStrings; *bindMountPtr != NULL; bindMountPtr++) {

    }


    ret = n_bindMounts;
_parseVolumeMounts_end:
    if (bindMountStrings != NULL) {
        for (bindMountPtr = bindMountStrings; *bindMountPtr != NULL; bindMountPtr++) {
            free(*bindMountPtr);
        }
        free(bindMountStrings);
        bindMountStrings = NULL;
    }
    return ret;
}

int appendVolumeMap(struct options *config, char *volumeDesc) {
    char *tokloc = NULL;
    char *from  = strtok_r(volumeDesc, ":", &tokloc);
    char *to    = strtok_r(NULL,   ":", &tokloc);
    char *flags = strtok_r(NULL,   ":", &tokloc);
    size_t cnt = config->volumeMapFrom_ptr - config->volumeMapFrom;

    if (from == NULL || to == NULL) {
        fprintf(stderr, "ERROR: invalid format for volume map!");
        _usage(1);
    }

    if (config->volumeMapFrom == NULL || (cnt + 2) >= config->volumeMap_capacity) {
        char **fromPtr = (char **) realloc(config->volumeMapFrom, config->volumeMap_capacity + VOLUME_ALLOC_BLOCK);
        char **toPtr = (char **) realloc(config->volumeMapTo, config->volumeMap_capacity + VOLUME_ALLOC_BLOCK);
        char **flagsPtr = (char **) realloc(config->volumeMapFlags, config->volumeMap_capacity + VOLUME_ALLOC_BLOCK);
        if (fromPtr == NULL || toPtr == NULL || flagsPtr == NULL) {
            fprintf(stderr, "ERROR: unable to allocate memory for volume map!\n");
            _usage(1);
        }
        config->volumeMapFrom = fromPtr;
        config->volumeMapTo = toPtr;
        config->volumeMapFlags = flagsPtr;
        config->volumeMapFrom_ptr = fromPtr + cnt;
        config->volumeMapTo_ptr = toPtr + cnt;
        config->volumeMapFlags_ptr = flagsPtr + cnt;
    }
    *(config->volumeMapFrom_ptr) = strdup(from);
    *(config->volumeMapTo_ptr) = strdup(to);
    *(config->volumeMapFlags_ptr) = (flags ? strdup(flags) : NULL);
    config->volumeMapFrom_ptr++;
    config->volumeMapTo_ptr++;
    config->volumeMapFlags_ptr++;
    *(config->volumeMapFrom_ptr) = NULL;
    *(config->volumeMapTo_ptr) = NULL;
    *(config->volumeMapFlags_ptr) = NULL;
    return 0;
}

static void _usage(int status) {
    exit(status);
}

static void _version(void) {
    printf("interactive shifter version %s\n", VERSION);
}

char **copyenv(void) {
    char **outenv = NULL;
    char **ptr = NULL;
    char **wptr = NULL;

    if (environ == NULL) {
        return NULL;
    }

    for (ptr = environ; *ptr != NULL; ++ptr) {
    }
    outenv = (char **) malloc(sizeof(char*) * ((ptr - environ) + 1));
    ptr = environ;
    wptr = outenv;
    for ( ; *ptr != NULL; ptr++) {
        *wptr++ = strdup(*ptr);
    }
    *wptr = NULL;
    return outenv;
}

static char *_filterString(const char *input) {
    ssize_t len = 0;
    char *ret = NULL;
    const char *rptr = NULL;
    char *wptr = NULL;
    if (input == NULL) return NULL;

    len = strlen(input) + 1;
    ret = (char *) malloc(sizeof(char) * len);
    if (ret == NULL) return NULL;

    rptr = input;
    wptr = ret;
    while (wptr - ret < len && *rptr != 0) {
        if (isalnum(*rptr) || *rptr == '_' || *rptr == ':' || *rptr == '.' || *rptr == '+' || *rptr == '-') {
            *wptr++ = *rptr;
        }
        rptr++;
    }
    *wptr = 0;
    return ret;
}

void free_options(struct options *opts, int freeStruct) {
    char **ptr = NULL;
    if (opts == NULL) return;
    if (opts->request != NULL) {
        free(opts->request);
        opts->request = NULL;
    }
    if (opts->imageType != NULL) {
        free(opts->imageType);
        opts->imageType = NULL;
    }
    if (opts->imageIdentifier != NULL) {
        free(opts->imageIdentifier);
        opts->imageIdentifier = NULL;
    }
    if (opts->rawVolumes) {
        free(opts->rawVolumes);
        opts->rawVolumes = NULL;
    }
    if (opts->args != NULL) {
        for (ptr = opts->args; *ptr != NULL; ptr++) {
            if (*ptr != (char *) 0x1) free(*ptr);
        }
        free(opts->args);
        opts->args = NULL;
    }
    if (opts->volumeStrings != NULL) {
        for (ptr = opts->volumeStrings; *ptr != NULL; ptr++) {
            free(*ptr);
        }
        free(opts->volumeStrings);
        opts->volumeStrings = NULL;
    }
    if (opts->volumeMapFrom != NULL) {
        for (ptr = opts->volumeMapFrom; *ptr != NULL; ptr++) {
            free(*ptr);
        }
        free(opts->volumeMapFrom);
        opts->volumeMapFrom = NULL;
    }
    if (opts->volumeMapTo != NULL) {
        for (ptr = opts->volumeMapTo; *ptr != NULL; ptr++) {
            free(*ptr);
        }
        free(opts->volumeMapTo);
        opts->volumeMapTo = NULL;
    }
    if (opts->volumeMapFlags != NULL) {
        for (ptr = opts->volumeMapFlags; *ptr != NULL; ptr++) {
            free(*ptr);
        }
        free(opts->volumeMapFlags);
        opts->volumeMapFlags = NULL;
    }
    if (freeStruct != 0) {
        free(opts);
    }
}

#ifdef _TESTHARNESS_SHIFTER
#include <CppUTest/CommandLineTestRunner.h>

TEST_GROUP(ShifterTestGroup) {
};

TEST(ShifterTestGroup, FilterString_basic) {
    CHECK(_filterString(NULL) == NULL);
    char *output = _filterString("echo test; rm -rf thing1");
    CHECK(strcmp(output, "echotestrm-rfthing1") == 0);
    free(output);
    output = _filterString("V4l1d-str1ng.input");
    CHECK(strcmp(output, "V4l1d-str1ng.input") == 0);
    free(output);
    output = _filterString("");
    CHECK(output != NULL);
    CHECK(strlen(output) == 0);
    free(output);
}

TEST(ShifterTestGroup, CopyEnv_basic) {
    MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
    setenv("TESTENV0", "gfedcba", 1);
    char **origEnv = copyenv();
    CHECK(origEnv != NULL);
    clearenv();
    setenv("TESTENV1", "abcdefg", 1);
    CHECK(getenv("TESTENV0") == NULL);
    CHECK(getenv("TESTENV1") != NULL);
    for (char **ptr = origEnv; *ptr != NULL; ptr++) {
        putenv(*ptr);
        // not free'ing *ptr, since *ptr is becoming part of the environment
        // it is owned by environ now
    }
    free(origEnv);
    CHECK(getenv("TESTENV0") != NULL);
    CHECK(strcmp(getenv("TESTENV0"), "gfedcba") == 0);
    MemoryLeakWarningPlugin::turnOnNewDeleteOverloads();
}

TEST(ShifterTestGroup, LocalPutEnv_basic) {
    setenv("TESTENV0", "qwerty123", 1);
    unsetenv("TESTENV2");
    char **altEnv = copyenv();
    CHECK(altEnv != NULL);
    char *testenv0Ptr = NULL;
    char *testenv2Ptr = NULL;
    char **ptr = NULL;
    int nEnvVar = 0;
    int nEnvVar2 = 0;
    for (ptr = altEnv; *ptr != NULL; ptr++) {
        if (strncmp(*ptr, "TESTENV0", 8) == 0) {
            testenv0Ptr = *ptr;
        }
        nEnvVar++;
    }
    CHECK(testenv0Ptr != NULL);
    CHECK(strcmp(testenv0Ptr, "TESTENV0=qwerty123") == 0);

    int ret = local_putenv(&altEnv, "TESTENV0=abcdefg321");
    CHECK(ret == 0);
    ret = local_putenv(&altEnv, "TESTENV2=asdfghjkl;");
    CHECK(ret == 0);
    ret = local_putenv(&altEnv, NULL);
    CHECK(ret != 0);
    ret = local_putenv(NULL, "TESTENV2=qwerty123");
    CHECK(ret != 0);

    for (ptr = altEnv; *ptr != NULL; ptr++) {
        if (strncmp(*ptr, "TESTENV0", 8) == 0) {
            testenv0Ptr = *ptr;
        } else if (strncmp(*ptr, "TESTENV2", 8) == 0) {
            testenv2Ptr = *ptr;
        } else {
            free(*ptr);
        }
        nEnvVar2++;
    }
    free(altEnv);
    CHECK(testenv0Ptr != NULL);
    CHECK(testenv2Ptr != NULL);
    CHECK(nEnvVar2 - nEnvVar == 1);
    CHECK(strcmp(testenv0Ptr, "TESTENV0=abcdefg321") == 0);
    CHECK(strcmp(testenv2Ptr, "TESTENV2=asdfghjkl;") == 0);

    free(testenv0Ptr);
    free(testenv2Ptr);
}

TEST(ShifterTestGroup, TestVolumeParse) {
    struct options opts;
    memset(&opts, 0, sizeof(struct options));
    int ret = parse_volume_mounts(&opts);
    CHECK(ret == 0);

    opts.rawVolumes = strdup("/test/path/1:/mnt/tp1");
    ret = parse_volume_mounts(&opts);
    CHECK(ret == 1);
    free_options(&opts, 0);

    opts.rawVolumes = strdup("/test/path/1:/mnt/tp1,/test/path/2:/mnt/tp2");
    ret = parse_volume_mounts(&opts);
    CHECK(ret == 2);
    free_options(&opts, 0);

}

int main(int argc, char **argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
#endif
