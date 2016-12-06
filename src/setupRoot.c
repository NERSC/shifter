/** @file setupRoot.c
 *  @brief Prepare a shifter environment based on image in the filesystem.
 *
 *  The setupRoot program prepares a shifter environment, including performing
 *  site-required modifications and user-requested bind mounts.  This is 
 *  intended to be run by a WLM prologue prior to batch script execution.
 *
 *  @author Douglas M. Jacobsen <dmjacobsen@lbl.gov>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>

#include "ImageData.h"
#include "UdiRootConfig.h"
#include "shifter_core.h"
#include "VolumeMap.h"

#include "config.h"

#define VOLUME_ALLOC_BLOCK 10

typedef struct _SetupRootConfig {
    char *sshPubKey;
    char *user;
    char *imageType;
    char *imageIdentifier;
    uid_t uid;
    gid_t gid;
    char *minNodeSpec;
    VolumeMap volumeMap;

    int verbose;
} SetupRootConfig;

static void _usage(int);
int parse_SetupRootConfig(int argc, char **argv, SetupRootConfig *config);
void free_SetupRootConfig(SetupRootConfig *config);
void fprint_SetupRootConfig(FILE *, SetupRootConfig *config);
int getImage(ImageData *, SetupRootConfig *, UdiRootConfig *);

int main(int argc, char **argv) {
    UdiRootConfig udiConfig;
    SetupRootConfig config;

    ImageData image;

    memset(&udiConfig, 0, sizeof(UdiRootConfig));
    memset(&config, 0, sizeof(SetupRootConfig));
    memset(&image, 0, sizeof(ImageData));

    clearenv();
    setenv("PATH", "/usr/bin:/usr/sbin:/bin:/sbin", 1);

    if (parse_SetupRootConfig(argc, argv, &config) != 0) {
        fprintf(stderr, "FAILED to parse command line arguments. Exiting.\n");
        _usage(1);
    }
    if (parse_UdiRootConfig(CONFIG_FILE, &udiConfig, UDIROOT_VAL_ALL) != 0) {
        fprintf(stderr, "FAILED to parse udiRoot configuration. Exiting.\n");
        exit(1);
    }
    udiConfig.target_uid = config.uid;
    udiConfig.target_gid = config.gid;
    udiConfig.auxiliary_gids = shifter_getgrouplist(config.user, udiConfig.target_gid, &(udiConfig.nauxiliary_gids));

    if (udiConfig.auxiliary_gids == NULL || udiConfig.nauxiliary_gids == 0) {
        fprintf(stderr, "FAILED to lookup auxiliary gids. Exiting.\n");
        exit(1);
    }

    if (config.verbose) {
        fprint_SetupRootConfig(stdout, &config);
        fprint_UdiRootConfig(stdout, &udiConfig);
    }

    if (getImage(&image, &config, &udiConfig) != 0) {
        fprintf(stderr, "FAILED to get image %s of type %s\n", config.imageIdentifier, config.imageType);
        exit(1);
    }
    if (config.verbose) {
        fprint_ImageData(stdout, &image);
    }
    if (image.useLoopMount) {
        if (mountImageLoop(&image, &udiConfig) != 0) {
            fprintf(stderr, "FAILED to mount image on loop device.\n");
            exit(1);
        }
    }
    if (mountImageVFS(&image, config.user, NULL, 0, 0, config.minNodeSpec, &udiConfig) != 0) {
        fprintf(stderr, "FAILED to mount image into UDI\n");
        exit(1);
    }

    if (config.sshPubKey != NULL && strlen(config.sshPubKey) > 0
            && config.user != NULL && strlen(config.user) > 0
            && config.uid != 0) {
        if (setupImageSsh(config.sshPubKey, config.user, config.uid, config.gid, &udiConfig) != 0) {
            fprintf(stderr, "FAILED to setup ssh configuration\n");
            exit(1);
        }
        if (startSshd(config.user, &udiConfig) != 0) {
            fprintf(stderr, "FAILED to start sshd\n");
            exit(1);
        }
    }

    if (setupUserMounts(&(config.volumeMap), &udiConfig) != 0) {
        fprintf(stderr, "FAILED to setup user-requested mounts.\n");
        exit(1);
    }

    if (saveShifterConfig(config.user, &image, &(config.volumeMap), &udiConfig) != 0) {
        fprintf(stderr, "FAILED to writeout shifter configuration file\n");
        exit(1);
    }

    if (!udiConfig.mountUdiRootWritable) {
        if (remountUdiRootReadonly(&udiConfig) != 0) {
            fprintf(stderr, "FAILED to remount udiRoot readonly, fail!\n");
            exit(1);
        }
    }

    return 0;
}

static void _usage(int exitStatus) {
    exit(exitStatus);
}

int parse_SetupRootConfig(int argc, char **argv, SetupRootConfig *config) {
    int opt = 0;
    optind = 1;

    while ((opt = getopt(argc, argv, "v:s:u:U:G:N:V")) != -1) {
        switch (opt) {
            case 'V': config->verbose = 1; break;
            case 'v':
                if (parseVolumeMap(optarg, &(config->volumeMap)) != 0) {
                    fprintf(stderr, "Failed to parse volume map request: %s\n", optarg);
                    _usage(1);
                }

                break;
            case 's':
                config->sshPubKey = strdup(optarg);
                break;
            case 'u':
                config->user = strdup(optarg);
                break;
            case 'U':
                config->uid = strtoul(optarg, NULL, 10);
                break;
            case 'G':
                config->gid = strtoul(optarg, NULL, 10);
                break;
            case 'N':
                config->minNodeSpec = strdup(optarg);
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
    config->imageType = imageDesc_filterString(argv[optind++], NULL);
    config->imageIdentifier = imageDesc_filterString(argv[optind++], config->imageType);
    return 0;
}

void free_SetupRootConfig(SetupRootConfig *config) {
    if (config->sshPubKey != NULL) {
        free(config->sshPubKey);
    }
    if (config->user != NULL) {
        free(config->user);
    }
    if (config->imageType != NULL) {
        free(config->imageType);
    }
    if (config->imageIdentifier != NULL) {
        free(config->imageIdentifier);
    }
    if (config->minNodeSpec != NULL) {
        free(config->minNodeSpec);
    }
    free_VolumeMap(&(config->volumeMap), 0);
    free(config);
}

void fprint_SetupRootConfig(FILE *fp, SetupRootConfig *config) {
    if (config == NULL || fp == NULL) return;
    fprintf(fp, "***** SetupRootConfig *****\n");
    fprintf(fp, "imageType: %s\n", (config->imageType ? config->imageType : ""));
    fprintf(fp, "imageIdentifier: %s\n", (config->imageIdentifier ? config->imageIdentifier : ""));
    fprintf(fp, "sshPubKey: %s\n", (config->sshPubKey ? config->sshPubKey : ""));
    fprintf(fp, "user: %s\n", (config->user ? config->user : ""));
    fprintf(fp, "uid: %d\n", config->uid);
    fprintf(fp, "minNodeSpec: %s\n", (config->minNodeSpec ? config->minNodeSpec : ""));
    fprintf(fp, "volumeMap: %lu maps\n", config->volumeMap.n);
    fprint_VolumeMap(fp, &(config->volumeMap));
    fprintf(fp, "***** END SetupRootConfig *****\n");
}

int getImage(ImageData *imageData, SetupRootConfig *config, UdiRootConfig *udiConfig) {
    int ret = parse_ImageData(config->imageType, config->imageIdentifier, udiConfig, imageData);
    return ret;
}
