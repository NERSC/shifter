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
 * See LICENSE for full text.
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
#include <signal.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include "UdiRootConfig.h"
#include "shifter_core.h"
#include "shifter_mem.h"
#include "ImageData.h"
#include "utility.h"
#include "VolumeMap.h"
#include "config.h"

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
    char *entrypoint;
    uid_t tgtUid;
    gid_t tgtGid;
    char *username;
    char *workdir;
    char *selectedModulesStr;
    char **args;
    char **env;
    VolumeMap volumeMap;
    int verbose;
    int useEntryPoint;
};

static void _usage(int);
int parse_options(int argc, char **argv, struct options *opts, UdiRootConfig *);
int parse_environment(struct options *opts, UdiRootConfig *);
int fprint_options(FILE *, struct options *);
void free_options(struct options *, int freeStruct);
int isImageLoaded(ImageData *, struct options *, UdiRootConfig *);
int loadImage(ImageData *, struct options *, UdiRootConfig *);
int adoptPATH(char **environ);

#ifndef _TESTHARNESS_SHIFTER

int main(int argc, char **argv) {
    sighandler_t sighupHndlr = signal(SIGHUP, SIG_IGN);
    sighandler_t sigintHndlr = signal(SIGINT, SIG_IGN);
    sighandler_t sigstopHndlr = signal(SIGSTOP, SIG_IGN);
    sighandler_t sigtermHndlr = signal(SIGTERM, SIG_IGN);

    /* save a copy of the environment for the exec */
    char **environ_copy = shifter_copyenv();

    /* declare needed variables */
    char *wd = _malloc(sizeof(char) * PATH_MAX);
    char *udiRoot = _malloc(sizeof(char) * PATH_MAX);
    char **run_args = NULL;
    uid_t actualUid = 0;
    uid_t actualGid = 0;
    uid_t eUid = 0;
    gid_t eGid = 0;
    int idx = 0;
    struct options *opts = _malloc(sizeof(struct options));
    UdiRootConfig *udiConfig = _malloc(sizeof(UdiRootConfig));
    ImageData *imageData = _malloc(sizeof(ImageData));
    memset(opts, 0, sizeof(struct options));
    memset(udiConfig, 0, sizeof(UdiRootConfig));
    memset(imageData, 0, sizeof(ImageData));

    if (parse_UdiRootConfig(CONFIG_FILE, udiConfig, UDIROOT_VAL_ALL) != 0) {
        fprintf(stderr, "FAILED to parse udiRoot configuration.\n");
        exit(1);
    }
    if (parse_environment(opts, udiConfig) != 0) {
        fprintf(stderr, "FAILED to parse environment\n");
        exit(1);
    }
    /* destroy this environment */
    clearenv();

    /* parse config file and command line options */
    if (parse_options(argc, argv, opts, udiConfig) != 0) {
        fprintf(stderr, "FAILED to parse command line arguments.\n");
        exit(1);
    }

    /* discover information about this image */
    if (parse_ImageData(opts->imageType, opts->imageIdentifier, udiConfig, imageData) != 0) {
        fprintf(stderr, "FAILED to find requested image.\n");
        exit(1);
    }

    run_args = calculate_args(opts->useEntryPoint, opts->args, opts->entrypoint,
                              imageData);
    if (run_args == NULL || run_args == NULL ) {
        fprintf(stderr, "Error calculating run arguements\n");
        exit(1);
    }
    if (imageData->workdir != NULL && opts->workdir == NULL) {
        opts->workdir = _strdup(imageData->workdir);
    }

    snprintf(udiRoot, PATH_MAX, "%s", udiConfig->udiMountPoint);
    udiRoot[PATH_MAX - 1] = 0;

    /* figure out who we are and who we want to be */
    eUid = geteuid();
    eGid = getegid();
    actualUid = getuid();
    actualGid = getgid();
    udiConfig->auxiliary_gids = shifter_getgrouplist(opts->username, opts->tgtGid, &(udiConfig->nauxiliary_gids));

    if (eUid != 0 && eGid != 0) {
        fprintf(stderr, "%s\n", "Not running with root privileges, will fail.");
        exit(1);
    }
    if (opts->tgtUid == 0 || opts->tgtGid == 0 || opts->username == NULL) {
        fprintf(stderr, "%s\n", "Failed to lookup username or attempted to run as root.\n");
        exit(1);
    }
    if (opts->tgtUid != actualUid || opts->tgtGid != actualGid) {
        fprintf(stderr, "Failed to correctly identify uid/gid, exiting.\n");
        exit(1);
    }
    if (!check_image_permissions(opts->tgtUid, opts->tgtGid,
                                udiConfig->auxiliary_gids,
                                udiConfig->nauxiliary_gids,
                                imageData))
    {
        fprintf(stderr,"FAILED permission denied to image\n");
        exit(1);
    }

    /* keep cwd to switch back to it (if possible), after chroot */
    if (getcwd(wd, PATH_MAX) == NULL) {
        perror("Failed to determine current working directory: ");
        exit(1);
    }
    wd[PATH_MAX - 1] = 0;
    if (opts->workdir == NULL) {
        opts->workdir = _strdup(wd);
    }

    if (isImageLoaded(imageData, opts, udiConfig) == 0) {
        if (loadImage(imageData, opts, udiConfig) != 0) {
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
    if (chdir("/") != 0) {
        perror("Could not chdir to new root: ");
        abort();
    }

    /* attempt to prevent this process and its heirs from ever gaining any
     * privilege by any means */
    if (shifter_set_capability_boundingset_null() != 0) {
        fprintf(stderr, "Failed to restrict future capabilities\n");
        abort();
    }

    /* drop privileges */
    if (setgroups(udiConfig->nauxiliary_gids, udiConfig->auxiliary_gids) != 0) {
        fprintf(stderr, "Failed to setgroups\n");
        abort();
    }
    if (setresgid(opts->tgtGid, opts->tgtGid, opts->tgtGid) != 0) {
        fprintf(stderr, "Failed to setgid to %d\n", opts->tgtGid);
        abort();
    }
    if (setresuid(opts->tgtUid, opts->tgtUid, opts->tgtUid) != 0) {
        fprintf(stderr, "Failed to setuid to %d\n", opts->tgtUid);
        abort();
    }
#if HAVE_DECL_PR_SET_NO_NEW_PRIVS == 1
    /* ensure this process and its heirs cannot gain privilege in recent kernels */
    /* see https://www.kernel.org/doc/Documentation/prctl/no_new_privs.txt */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        fprintf(stderr, "Failed to fully drop privileges: %s",
                strerror(errno));
        abort();
    }
#endif

    /* chdir (within chroot) to where we belong again */
    if (chdir(opts->workdir) != 0) {
        fprintf(stderr, "Failed to switch to working dir: %s, staying in /\n", opts->workdir);
    }

    /* set the environment variables */
    if (shifter_setupenv(&environ_copy, imageData, udiConfig) != 0) {
        fprintf(stderr, "Failed to setup container environment variables\n");
    }

    /* run any user hooks */
    for (idx = 0; idx < udiConfig->n_active_modules; idx++) {
        if (udiConfig->active_modules[idx]->userhook == NULL)
            continue;

        char *args[] = { "/bin/sh", udiConfig->active_modules[idx]->userhook, NULL };
        int rc = forkAndExecv(args);
        if (rc != 0) {
            fprintf(stderr, "Failed to setup module %s\n", udiConfig->active_modules[idx]->name);
            exit(1);
        }
    }

    /* immediately set PATH to container PATH to get search right */
    adoptPATH(environ_copy);

    /* reset signal handlers */
    signal(SIGHUP, sighupHndlr);
    signal(SIGINT, sigintHndlr);
    signal(SIGSTOP, sigstopHndlr);
    signal(SIGTERM, sigtermHndlr);

    /* attempt to execute user-requested exectuable */
    execvpe(run_args[0], run_args, environ_copy);

    /* doh! how did we get here? return the error */
    fprintf(stderr, "%s: %s: %s\n", argv[0], run_args[0], strerror(errno));
    return 127;
}
#endif /* TESTHARNESS */


int parse_options(int argc, char **argv, struct options *config, UdiRootConfig *udiConfig) {
    int opt = 0;
    int volOptCount = 0;
    static struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"volume", 1, 0, 'V'},
        {"verbose", 0, 0, 'v'},
        {"image", 1, 0, 'i'},
        {"entrypoint", 2, 0, 0},
        {"workdir", 1, 0, 'w'},
        {"module", 1, 0, 'm'},
        {"env", 0, 0, 'e'},
        {0, 0, 0, 0}
    };
    if (config == NULL) {
        return 1;
    }

    /* set some defaults */
    config->tgtUid = getuid();
    config->tgtGid = getgid();

    /* ensure that getopt processing stops at first non-option */
    setenv("POSIXLY_CORRECT", "1", 1);

    optind = 1;
    for ( ; ; ) {
        int longopt_index = 0;
        opt = getopt_long(argc, argv, "hnvV:i:e:w:m:", long_options, &longopt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0:
                {
                    if (strcmp(long_options[longopt_index].name, "entrypoint") == 0) {
                        config->useEntryPoint = 1;
                        if (optarg != NULL) {
                            config->entrypoint = _strdup(optarg);
                        }
                    }
                }
                break;
            case 'w':
                if (optarg != NULL) {
                    config->workdir = _strdup(optarg);
                }
                break;
            case 'v':
                config->verbose = 1;
                break;
            case 'V':
                {
                    if (optarg == NULL) break;
                    size_t raw_capacity = 0;
                    size_t new_capacity = strlen(optarg);

                    /* if the user is specifying command-line volumes, want to
                     * get rid of anything coming from the environment
                     */
                    if (volOptCount == 0 && config->rawVolumes != NULL) {
                        free(config->rawVolumes);
                        config->rawVolumes = NULL;
                    }

                    if (config->rawVolumes != NULL) {
                        raw_capacity = strlen(config->rawVolumes);
                    }
                    config->rawVolumes = (char *) _realloc(config->rawVolumes, sizeof(char) * (raw_capacity + new_capacity + 2));
                    char *ptr = config->rawVolumes + raw_capacity;
                    snprintf(ptr, new_capacity + 2, ";%s", optarg);

                    volOptCount++;
                    break;
                }
            case 'i':
                {
                    char *type = NULL;
                    char *tag = NULL;

                    if (parse_ImageDescriptor(optarg, &type, &tag, udiConfig)
                        != 0) {

                        fprintf(stderr, "Incorrect format for image "
                                "identifier: need \"image_type:image_desc\", "
                                "e.g., docker:ubuntu:14.04\n");
                        _usage(1);
                        break;
                    }
                    config->imageType = type;
                    config->imageTag = tag;
                }
                break;
            case 'e':
                /* TODO - add support for explicitly overriding environment
                 * variables
                 */
                break;
            case 'm':
                if (config->selectedModulesStr) {
                    free(config->selectedModulesStr);
                }
                config->selectedModulesStr = _strdup(optarg);
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
        int curr_euid = geteuid();
        if (seteuid(config->tgtUid) != 0) {
            fprintf(stderr, "FAILED to change permissions to uid %d\n", config->tgtUid);
            abort();
        }
        config->imageIdentifier = lookup_ImageIdentifier(config->imageType,
                                      config->imageTag, config->verbose,
                                      udiConfig);

        if (seteuid(curr_euid) != 0) {
            fprintf(stderr, "FAILED to change permissions back to uid %d\n", curr_euid);
            abort();
        }
    }
    if (config->imageIdentifier == NULL) {
        fprintf(stderr, "FAILED to lookup %s image %s\n", config->imageType, config->imageTag);
        _usage(1);
    }

    if (config->rawVolumes != NULL) {
        /* remove trailing comma */
        size_t len = strlen(config->rawVolumes);
        if (config->rawVolumes[len - 1] == ';') {
            config->rawVolumes[len - 1] = 0;
        }
    }

    int remaining = argc - optind;
    if (config->useEntryPoint && remaining == 0) {
        config->args = (char **) _malloc(sizeof(char *) );
        config->args[0] = NULL;
    } else if (remaining > 0) {
        /* interpret all remaining arguments as the intended command */
        char **argsPtr = NULL;
        config->args = (char **) _malloc(sizeof(char *) * (remaining + 1));
        for (argsPtr = config->args; optind < argc; optind++) {
            *argsPtr++ = _strdup(argv[optind]);
        }
        *argsPtr = NULL;
    }
    /* validate and organize any user-requested bind-mounts */
    if (config->rawVolumes != NULL) {
        if (parseVolumeMap(config->rawVolumes, &(config->volumeMap)) != 0) {
            fprintf(stderr, "Failed to parse volume map options\n");
            _usage(1);
        }
    }

    if (config->username == NULL) {
        struct passwd *pwd = shifter_getpwuid(config->tgtUid, udiConfig);
        if (pwd != NULL) {
            config->username = _strdup(pwd->pw_name);
        }
    }

    if (config->selectedModulesStr != NULL) {
        if (parse_selected_ShifterModule(config->selectedModulesStr, udiConfig) != 0) {
            fprintf(stderr, "Invalid shifter module selection: %s\n",
                    config->selectedModulesStr);
            _usage(1);
        }
    }

    udiConfig->target_uid = config->tgtUid;
    udiConfig->target_gid = config->tgtGid;
    return 0;
}

int parse_environment(struct options *opts, UdiRootConfig *udiConfig) {
    char *envPtr = NULL;
    char *type = NULL;
    char *tag = NULL;
    char *module = NULL;

    /* read type and tag from the environment */
    if ((envPtr = getenv("SHIFTER_IMAGETYPE")) != NULL) {
        type = imageDesc_filterString(envPtr, NULL);
    } else if ((envPtr = getenv("SLURM_SPANK_SHIFTER_IMAGETYPE")) != NULL) {
        type = imageDesc_filterString(envPtr, NULL);
    }
    if ((envPtr = getenv("SHIFTER_IMAGE")) != NULL) {
        tag = imageDesc_filterString(envPtr, type);
    } else if ((envPtr = getenv("SLURM_SPANK_SHIFTER_IMAGE")) != NULL) {
        tag = imageDesc_filterString(envPtr, type);
    }
    if ((envPtr = getenv("SHIFTER_MODULE")) != NULL) {
        module = _strdup(envPtr);
    } else if ((envPtr = getenv("SLURM_SPANK_SHIFTER_MODULE")) != NULL) {
        module = _strdup(envPtr);
    }

    if (module) {
        opts->selectedModulesStr = module;
    }

    /* validate type and tag */
    if ((type && !tag) || (!type && tag)) {
        if (type) {
            free(type);
            type = NULL;
        }
        if (tag) {
            free(tag);
            tag = NULL;
        }
    }
    if (type && tag && strlen(type) > 0 && strlen(tag) > 0) {
        opts->imageType = type;
        opts->imageTag = tag;
    }

    if ((envPtr = getenv("SHIFTER")) != NULL) {
        opts->request = _strdup(envPtr);
    } else if ((envPtr = getenv("SLURM_SPANK_SHIFTER")) != NULL) {
        opts->request = _strdup(envPtr);
    }
    if (opts->request != NULL) {
        /* if the the imageType and Tag weren't specified earlier, parse from here */
        if (opts->imageType == NULL && opts->imageTag == NULL) {
            if (parse_ImageDescriptor(opts->request, &(opts->imageType),
                    &(opts->imageTag), udiConfig) != 0) {

                if (opts->imageType != NULL) {
                    free(opts->imageType);
                    opts->imageType = NULL;
                }
                if (opts->imageTag != NULL) {
                    free(opts->imageTag);
                    opts->imageTag = NULL;
                }
            }
        }
    }
    if ((envPtr = getenv("SHIFTER_VOLUME")) != NULL) {
        opts->rawVolumes = _strdup(envPtr);
    } else if ((envPtr = getenv("SLURM_SPANK_SHIFTER_VOLUME")) != NULL) {
        opts->rawVolumes = _strdup(envPtr);
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
    if (opts->entrypoint) {
        free(opts->entrypoint);
        opts->entrypoint = NULL;
    }
    if (opts->args != NULL) {
        for (ptr = opts->args; *ptr != NULL; ptr++) {
            if (*ptr != (char *) 0x1) free(*ptr);
        }
        free(opts->args);
        opts->args = NULL;
    }
    if (opts->env != NULL) {
        for (ptr = opts->env; *ptr != NULL; ptr++) {
            free(*ptr);
        }
        free(opts->env);
        opts->env = NULL;
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
    snprintf(chrootPath, PATH_MAX, "%s", udiConfig->udiMountPoint);
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

    /* all of the mounts visible to us are now in our private namespace; since
     * we'll be changing things, need to ensure that nothing is mounted with
     * MS_SHARED, which would cause our changes to propagate out to the outside
     * system.  If we used MS_PRIVATE it would prevent us from receiving
     * external events even if downstream bindmounts made by the container
     * specify MS_SLAVE.  Thus setting MS_SLAVE forces the one-way propagation
     * of mount/umounts that are desirable here
     */
    if (mount(NULL, "/", NULL, MS_SLAVE|MS_REC, NULL) != 0) {
        perror("Failed to remount \"/\" non-shared.");
        goto _loadImage_error;
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
    if (mountImageVFS(image, opts->username, opts->verbose, NULL, udiConfig) != 0) {
        fprintf(stderr, "FAILED to mount image into UDI\n");
        goto _loadImage_error;
    }

    if (setupUserMounts(&(opts->volumeMap), udiConfig) != 0) {
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

int adoptPATH(char **environ) {
    char **ptr = environ;
    for ( ; ptr && *ptr; ptr++) {
        if (strncmp(*ptr, "PATH=", 5) == 0) {
            char *path = *ptr + 5;
            setenv("PATH", path, 1);
            return 0;
        }
    }
    return 1;
}
