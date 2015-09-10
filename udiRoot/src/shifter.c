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
#include "VolumeMap.h"

extern char **environ;

#define VOLUME_ALLOC_BLOCK 10

#ifndef VERSION
#define VERSION "0Test0"
#endif


struct options {
    char *request;
    char *imageType;
    char *imageTag;
    char *imageIdentifier;
    char *rawVolumes;
    uid_t tgtUid;
    gid_t tgtGid;
    char *username;
    char *workdir;
    char **args;
    VolumeMap volumeMap;
    int verbose;
    int useEntryPoint;
};


static void _usage(int);
static char *_filterString(const char *input, int allowSlash);
char **copyenv(void);
int parse_options(int argc, char **argv, struct options *opts, UdiRootConfig *);
int parse_environment(struct options *opts);
int fprint_options(FILE *, struct options *);
void free_options(struct options *, int freeStruct);
int local_putenv(char ***environ, const char *newVar);
int isImageLoaded(ImageData *, struct options *, UdiRootConfig *);
int loadImage(ImageData *, struct options *, UdiRootConfig *);

#ifndef _TESTHARNESS_SHIFTER
int main(int argc, char **argv) {

    /* save a copy of the environment for the exec */
    char **environ_copy = copyenv();

    /* declare needed variables */
    char wd[PATH_MAX];
    char udiRoot[PATH_MAX];
    uid_t eUid = 0;
    gid_t eGid = 0;
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

    if (parse_UdiRootConfig(CONFIG_FILE, &udiConfig, UDIROOT_VAL_ALL) != 0) {
        fprintf(stderr, "FAILED to parse udiRoot configuration.\n");
        exit(1);
    }

    /* parse config file and command line options */
    if (parse_options(argc, argv, &opts, &udiConfig) != 0) {
        fprintf(stderr, "FAILED to parse command line arguments.\n");
        exit(1);
    }

    /* discover information about this image */
    if (parse_ImageData(opts.imageType, opts.imageIdentifier, &udiConfig, &imageData) != 0) {
        fprintf(stderr, "FAILED to find requested image.\n");
        exit(1);
    }

    /* check if entrypoint is defined and desired */
    if (opts.useEntryPoint == 1) {
        if (imageData.entryPoint != NULL) {
            opts.args[0] = strdup(imageData.entryPoint);
            if (imageData.workdir != NULL) {
                opts.workdir = strdup(imageData.workdir);
            }
        } else {
            fprintf(stderr, "Image does not have a defined entrypoint.\n");
        }
    }

    snprintf(udiRoot, PATH_MAX, "%s%s", udiConfig.nodeContextPrefix, udiConfig.udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;

    /* figure out who we are and who we want to be */
    eUid = geteuid();
    eGid = getegid();


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
    if (opts.tgtUid == 0 || opts.tgtGid == 0 || opts.username == NULL) {
        fprintf(stderr, "%s\n", "Will not run as root.");
        fprintf(stderr, "%d %d, %s\n", opts.tgtUid, opts.tgtGid, opts.username);
        exit(1);
    }

    /* keep cwd to switch back to it (if possible), after chroot */
    if (getcwd(wd, PATH_MAX) == NULL) {
        perror("Failed to determine current working directory: ");
        exit(1);
    }
    wd[PATH_MAX-1] = 0;
    if (opts.workdir == NULL) {
        opts.workdir = strdup(wd);
    }

    if (isImageLoaded(&imageData, &opts, &udiConfig) == 0) {
        if (loadImage(&imageData, &opts, &udiConfig) != 0) {
            fprintf(stderr, "FAILED to setup image.\n");
            exit(1);
        }
    }

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
    if (chdir(opts.workdir) != 0) {
        fprintf(stderr, "Failed to switch to working dir: %s\n", opts.workdir);
        exit(1);
    }

    /* source the environment variables from the image */
    char **envPtr = NULL;
    for (envPtr = imageData.env; envPtr && *envPtr; envPtr++) {
        local_putenv(&environ_copy, *envPtr);
    }
    char *shifterRuntime = strdup("SHIFTER_RUNTIME=1");
    local_putenv(&environ_copy, shifterRuntime);

    execvpe(opts.args[0], opts.args, environ_copy);
    return 127;
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

int parse_options(int argc, char **argv, struct options *config, UdiRootConfig *udiConfig) {
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

    if (config == NULL) {
        return 1;
    }

    /* set some defaults */
    config->tgtUid = getuid();
    config->tgtGid = getgid();

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
                        printf("lookup user: %s\n", optarg);
                        pwd = getpwnam(optarg);
                        if (pwd != NULL) {
                            config->tgtUid = pwd->pw_uid;
                            config->tgtGid = pwd->pw_gid;
                            config->username = strdup(pwd->pw_name);
                            printf("got user: %s, %d\n", config->username, config->tgtUid);
                        } else {
                            uid_t uid = atoi(optarg);
                            if (uid != 0) {
                                pwd = getpwuid(uid);
                                config->tgtUid = pwd->pw_uid;
                                config->tgtGid = pwd->pw_gid;
                                config->username = strdup(pwd->pw_name);
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
                {
                    int isLocalOrDocker = 0;
                    char *tmp = NULL;
                    ptr = strchr(optarg, ':');
                    if (ptr == NULL) {
                        fprintf(stderr, "Incorrect format for image identifier:  need \"image_type:image_id\"\n");
                        _usage(1);
                        break;
                    }
                    *ptr++ = 0;
                    tmp = _filterString(optarg, 0);
                    if (config->imageType != NULL) free(config->imageType);
                    config->imageType = tmp;
                    if (strcmp(config->imageType, "local") == 0 || strcmp(config->imageType, "docker") == 0) {
                        isLocalOrDocker = 1;
                    }
                    tmp = _filterString(ptr, isLocalOrDocker);
                    if (config->imageTag != NULL) free(config->imageTag);
                    config->imageTag = tmp;
                    if (config->imageIdentifier != NULL) {
                        free(config->imageIdentifier);
                        config->imageIdentifier = NULL;
                    }
                }
                break;
            case 'e':
                config->useEntryPoint = 1;
                break;
            case 'h':
                _usage(0);
                break;
            case '?':
                fprintf(stderr, "Missing an argument!\n");
                _usage(1);
                break;
            default:
                break;
        }
    }

    if (config->imageType == NULL || config->imageTag == NULL) {
        fprintf(stderr, "No image specified, or specified incorrectly!\n");
        _usage(1);
    }
    if (config->imageIdentifier == NULL) {
        config->imageIdentifier = lookup_ImageIdentifier(config->imageType, config->imageTag, config->verbose, udiConfig);
    }
    if (config->imageIdentifier == NULL) {
        fprintf(stderr, "FAILED to lookup %s image %s\n", config->imageType, config->imageTag);
        _usage(1);
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
    if (config->rawVolumes != NULL) {
        if (parseVolumeMap(config->rawVolumes, &(config->volumeMap)) != 0) {
            fprintf(stderr, "Failed to parse volume map options\n");
            _usage(1);
        }
    }

    if (config->username == NULL) {
        struct passwd *pwd = getpwuid(config->tgtUid);
        if (pwd != NULL) {
            config->username = strdup(pwd->pw_name);
        }
    }

    return 0;
}

int parse_environment(struct options *opts) {
    char *envPtr = NULL;

    if ((envPtr = getenv("SHIFTER_IMAGETYPE")) != NULL) {
        opts->imageType = strdup(envPtr);
    } else if ((envPtr = getenv("SLURM_SPANK_SHIFTER_IMAGETYPE")) != NULL) {
        opts->imageType = strdup(envPtr);
    }
    if ((envPtr = getenv("SHIFTER_IMAGE")) != NULL) {
        opts->imageIdentifier = strdup(envPtr);
        opts->imageTag = strdup(envPtr);
    } else if ((envPtr = getenv("SLURM_SPANK_SHIFTER_IMAGE")) != NULL) {
        opts->imageIdentifier = strdup(envPtr);
        opts->imageTag = strdup(envPtr);
    }

    if ((envPtr = getenv("SHIFTER")) != NULL) {
        opts->request = strdup(envPtr);
    } else if ((envPtr = getenv("SLURM_SPANK_SHIFTER")) != NULL) {
        opts->request = strdup(envPtr);
    }
    if (opts->request != NULL) {
        char *ptr = NULL;
        /* if the the imageType and Tag weren't specified earlier, parse from here */
        if (opts->imageType == NULL && opts->imageTag == NULL) {
            ptr = strchr(opts->request, ':');
            if (ptr != NULL) {
                *ptr++ = 0;
                opts->imageType = strdup(opts->request); /* temporarily truncated at former ';' */
                opts->imageTag = strdup(ptr);
                /* put things back the way they were */
                ptr--;
                *ptr = ':';
            }
        }
    }
    if ((envPtr = getenv("SHIFTER_VOLUME")) != NULL) {
        opts->rawVolumes = strdup(envPtr);
    } else if ((envPtr = getenv("SLURM_SPANK_SHIFTER_VOLUME")) != NULL) {
        opts->rawVolumes = strdup(envPtr);
    }

    return 0;
}


static void _usage(int status) {
    printf("\n"
        "Usage:\n"
        "shifter [-h|--help] [-v|--verbose] [--image=<imageType>:<imageTag>]\n"
        "    [--entry] [-V|--volume=/path/to/bind:/mnt/in/image[:<flags>][,...]]\n"
        "    [-- /command/to/exec/in/shifter [args...]]\n"
        );
    printf("\n");
    printf(
"Image Selection:  Images can be selected in any of three ways, explicit\n"
"specification as an argument, e.g.:\n"
"    shifter --image=docker:ubuntu/15.04\n"
"Or an image can be specified in the environment by passing either:\n"
"    export SHIFTER=docker:ubuntu/15.04\n"
"                    or\n"
"    export SHIFTER_IMAGETYPE=docker\n"
"    export SHIFTER_IMAGE=ubuntu/15.04\n"
"Or if an image is already loaded in the global namespace owned by the\n"
"running user, and none of the above options are set, then the image loaded\n" 
"in the global namespace will be used.\n"
"\n"
"Command Selection: If a command is supplied on the command line Shifter will\n"
"attempt to exec that command within the image.  Otherwise, if \"--entry\" is\n"
"specified and an entry point is defined for the image, then the entrypoint\n"
"will be executed.  Finally if nothing else is specified /bin/sh will be\n"
"attempted.\n"
"\n"
"Environmental Transfer: All the environment variables defined in the\n"
"calling processes's environment will be transferred into the container,\n"
"however, any environment variables defined in the container desription,\n"
"e.g., Docker ENV-defined variables, will be sourced and override those.\n"
"\n"
"Volume Mapping:  You can request any path available in the current\n"
"environment to be mapped to some other path within the container.\n"
"e.g.,\n"
"    shifter --volume=/scratch/path/to/my/data:/data\n"
"Would cause /data within the container to be bound to\n"
"/scratch/path/to/my/data.  The mount-point (e.g., /data) must already exist\n"
"in the provided image.  You can not bind a path into any path including or\n"
"under /dev, /etc, /opt/udiImage, /proc, or /var; or overwrite any bind-\n"
"requested by the system configuration.\n"
"\n"
        );
    exit(status);
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

static char *_filterString(const char *input, int allowSlash) {
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
        if (allowSlash && *rptr == '/') {
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
    if (opts->imageTag != NULL) {
        free(opts->imageTag);
        opts->imageTag = NULL;
    }
    if (opts->workdir != NULL) {
        free(opts->workdir);
        opts->workdir = NULL;
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
    free_VolumeMap(&(opts->volumeMap), 0);
    if (freeStruct != 0) {
        free(opts);
    }
}

/**
 * isImageLoaded - Determine if an image with the exact same options is already
 * loaded on the system.  Calls shifter_core library function with image 
 * identifier and volume mount options to read the state of the system to 
 * determine if it is a match.
 * */
int isImageLoaded(ImageData *image, struct options *options, UdiRootConfig *udiConfig) {
    int cmpVal = compareShifterConfig(options->username,
                image,
                &(options->volumeMap),
                udiConfig);
    if (cmpVal == 0) return 1;
    return 0;
}

/**
 * Loads the needed image
 */
int loadImage(ImageData *image, struct options *opts, UdiRootConfig *udiConfig) {
    int retryCnt = 0;
    char chrootPath[PATH_MAX];
    snprintf(chrootPath, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    chrootPath[PATH_MAX - 1] = 0;
    gid_t gidZero = 0;

    /* must achieve full root privileges to perform mounts */
    if (setgroups(1, &gidZero) != 0) {
        fprintf(stderr, "Failed to setgroups to %d\n", gidZero);
        goto _loadImage_error;
    }
    if (setresgid(0, 0, 0) != 0) {
        fprintf(stderr, "Failed to setgid to %d\n", 0);
        goto _loadImage_error;
    }
    if (setresuid(0, 0, 0) != 0) {
        fprintf(stderr, "Failed to setuid to %d\n", 0);
        goto _loadImage_error;
    }
    if (unshare(CLONE_NEWNS) != 0) {
        perror("Failed to unshare the filesystem namespace.");
        goto _loadImage_error;
    }

    if (isSharedMount("/") == 1) {
        if (mount(NULL, "/", NULL, MS_PRIVATE|MS_REC, NULL) != 0) {
            perror("Failed to remount \"/\" non-shared.");
            goto _loadImage_error;
        }
    }

    /* remove access to any preexisting mounts in the global namespace to this area */
    destructUDI(udiConfig, 0);
    for (retryCnt = 0; retryCnt < 10; retryCnt++) {
        if (validateUnmounted(chrootPath, 1) == 0) break;
        usleep(300000); /* sleep for 0.3s */
    }
    if (retryCnt == 10) {
        fprintf(stderr, "FAILED to unmount old image in this namespace, cannot conintue.\n");
        goto _loadImage_error;
    }

    if (image->useLoopMount) {
        if (mountImageLoop(image, udiConfig) != 0) {
            fprintf(stderr, "FAILED to mount image on loop device.\n");
            goto _loadImage_error;
        }
    }
    if (mountImageVFS(image, opts->username, NULL, udiConfig) != 0) {
        fprintf(stderr, "FAILED to mount image into UDI\n");
        goto _loadImage_error;
    }

    if (setupUserMounts(image, &(opts->volumeMap), udiConfig) != 0) {
        fprintf(stderr, "FAILED to setup user-requested mounts.\n");
        goto _loadImage_error;
    }

    if (saveShifterConfig(opts->username, image, &(opts->volumeMap), udiConfig) != 0) {
        fprintf(stderr, "FAILED to writeout shifter configuration file\n");
        goto _loadImage_error;
    }

    if (!udiConfig->mountUdiRootWritable) {
        if (remountUdiRootReadonly(udiConfig) != 0) {
            fprintf(stderr, "FAILED to remount udiRoot readonly, fail!\n");
            goto _loadImage_error;
        }
    }

    return 0;
_loadImage_error:
    return 1;
}

#ifdef _TESTHARNESS_SHIFTER
#include <CppUTest/CommandLineTestRunner.h>

TEST_GROUP(ShifterTestGroup) {
};

TEST(ShifterTestGroup, FilterString_basic) {
    CHECK(_filterString(NULL, 0) == NULL);
    char *output = _filterString("echo test; rm -rf thing1", 0);
    CHECK(strcmp(output, "echotestrm-rfthing1") == 0);
    free(output);
    output = _filterString("V4l1d-str1ng.input", 0);
    CHECK(strcmp(output, "V4l1d-str1ng.input") == 0);
    free(output);
    output = _filterString("", 0);
    CHECK(output != NULL);
    CHECK(strlen(output) == 0);
    free(output);

    output = _filterString("/this/is/not/allowed", 0);
    CHECK(strcmp(output, "thisisnotallowed") == 0);
    free(output);
    output = _filterString("/this/is/allowed", 1);
    CHECK(strcmp(output, "/this/is/allowed") == 0);
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
        /* not free'ing *ptr, since *ptr is becoming part of the environment
           it is owned by environ now */
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


int main(int argc, char **argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
#endif
