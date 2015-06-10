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

#define _GNU_SOURCE
#include <unistd.h>
#include <sched.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <grp.h>

#include "UdiRootConfig.h"
#include "shifter_core.h"
#include "ImageData.h"
#include "utility.h"

extern char **environ;

#define VOLUME_ALLOC_BLOCK 10

struct options {
    char *imageType;
    char *imageIdentifier;
    uid_t tgtUid;
    gid_t tgtGid;
    char **args;
    char **volumeMapFrom;
    char **volumeMapTo;
    char **volumeMapFlags;

    /* state variables only used at parse time */
    size_t volumeMap_capacity;
    char **volumeMapFrom_ptr;
    char **volumeMapTo_ptr;
    char **volumeMapFlags_ptr;
};

static void _usage(int);
static void _version(void);
char **copyenv(void);
int parse_options(int argc, char **argv, struct options *opts);
int fprint_options(FILE *, struct options *);
void free_options(struct options *);

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

    /* destroy this environment */
    clearenv();

    if (parse_UdiRootConfig(CONFIG_FILE) != 0) {
        fprintf(stderr, "FAILED to parse udiRoot configuration.\n");
        exit(1);
    }

    /* parse config file and command line options */
    if (parse_config(argc, argv, &opts) != 0) {
        fprintf(stderr, "FAILED to parse command line arguments.\n");
        exit(1);
    }

    snprintf(udiRoot, PATH_MAX, "%s%s", udiConfig.nodeContextPrefix, udiConfig.udiMountPoint);
    udiRoot[PATH_MAX-1];


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

    /* TODO: run setupRoot here */
    snprintf(exec, PATH_MAX, "%s%s/sbin/setupRoot", udiConfig.nodeContextPrefix, udiConfig.udiMountPoint);
    exec[PATH_MAX-1] = 0;
    {
        char *args[] = {exec, config.imageType, config.imageIdentifier, NULL};
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

    arg = opts.args;
    while (*arg != NULL) {
        printf("arg: %s\n", *arg);
        arg++;
    }
    execve(opts.args[0], opts.args, environ_copy);
    return 0;
}

int parse_SetupRootConfig(int argc, char **argv, SetupRootConfig *config) {
    int opt = 0;
    static struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"volume", 1, 0, 'V'},
        {"verbose", 0, 0, 'v'},
        {"user", 0, 0, 0},
        {"group", 0, 0, 0},
        {0, 0, 0, 0}
    };


    optind = 1;
    for ( ; ; ) {
        int longopt_index = 0;
        opt = getopt_long(argc, argv, "hvV:", long_options, &longopt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0:
                {
                    if (strcmp(long_options[longopt_index].name, "user") == 0) {
                        if (optarg == NULL) {
                            fprintf(stderr, "Must specify user with --user flag.\n");
                            _usage(1);
                        }
                    } else if (strcmp(long_options[longopt_index].name, "group") == 0) {
                        if (optarg == NULL) {
                            fprintf(stderr, "Must specify group with --group flag.\n");
                            _usage(1);
                        }
                    }
                }
                break;
            case 'v': config->verbose = 1; break;
            case 'V':
                {
                    char *from  = strtok(optarg, ":");
                    char *to    = strtok(NULL,   ":");
                    char *flags = strtok(NULL,   ":");
                    size_t cnt = config->volumeMapFrom_ptr - config->volumeMapFrom;

                    if (from == NULL || to == NULL) {
                        fprintf(stderr, "ERROR: invalid format for volume map!");
                        _usage(1);
                    }

                    if (config->volumeMapFrom == NULL || (cnt + 2) >= config->volumeMap_capacity) {
                        char **fromPtr = realloc(config->volumeMapFrom, config->volumeMap_capacity + VOLUME_ALLOC_BLOCK);
                        char **toPtr = realloc(config->volumeMapTo, config->volumeMap_capacity + VOLUME_ALLOC_BLOCK);
                        char **flagsPtr = realloc(config->volumeMapFlags, config->volumeMap_capacity + VOLUME_ALLOC_BLOCK);
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
                }
                break;
            case '?':
                fprintf(stderr, "Missing an argument!\n");
                _usage(1);
                break;
            default:
                break;
        }
    }

    int remaining = argc - optind;
    if (remaining != 2) {
        fprintf(stderr, "Must specify image type and image identifier\n");
        _usage(1);
    }
    config->imageType = _filterString(argv[optind++]);
    config->imageIdentifier = _filterString(argv[optind++]);
    return 0;
}

static void _usage(int status) {
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
    for (ptr = environ, wptr = outenv; *ptr != NULL; ++ptr, ++wptr) {
        *wptr = strdup(*ptr);
    }
    *wptr = NULL;
    return outenv;
}
