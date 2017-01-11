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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "UdiRootConfig.h"
#include "utility.h"

#define SITEFS_ALLOC_BLOCK 16
#define SERVER_ALLOC_BLOCK 3
#define PNCALLOWEDFS_ALLOC_BLOCK 10

static int _assign(const char *key, const char *value, void *tUdiRootConfig);
static int _validateConfigFile(const char *);

int parse_UdiRootConfig(const char *configFile, UdiRootConfig *config, int validateFlags) {
    int ret = 0;

    ret = _validateConfigFile(configFile);
    if (ret != 0) {
        return ret;
    }

    if (shifter_parseConfig(configFile, '=', config, _assign) != 0) {
        return UDIROOT_VAL_PARSE;
    }

    ret = validate_UdiRootConfig(config, validateFlags);
    return ret;
}

void free_UdiRootConfig(UdiRootConfig *config, int freeStruct) {
    if (config == NULL) return;

    if (config->udiMountPoint != NULL) {
        free(config->udiMountPoint);
        config->udiMountPoint = NULL;
    }
    if (config->loopMountPoint != NULL) {
        free(config->loopMountPoint);
        config->loopMountPoint = NULL;
    }
    if (config->batchType != NULL) {
        free(config->batchType);
        config->batchType = NULL;
    }
    if (config->system != NULL) {
        free(config->system);
        config->system = NULL;
    }
    if (config->defaultImageType != NULL) {
        free(config->defaultImageType);
        config->defaultImageType = NULL;
    }
    if (config->imageBasePath != NULL) {
        free(config->imageBasePath);
        config->imageBasePath = NULL;
    }
    if (config->udiRootPath != NULL) {
        free(config->udiRootPath);
        config->udiRootPath = NULL;
    }
    if (config->perNodeCachePath != NULL) {
        free(config->perNodeCachePath);
        config->perNodeCachePath = NULL;
    }
    if (config->sitePreMountHook != NULL) {
        free(config->sitePreMountHook);
        config->sitePreMountHook = NULL;
    }
    if (config->sitePostMountHook != NULL) {
        free(config->sitePostMountHook);
        config->sitePostMountHook = NULL;
    }
    if (config->optUdiImage != NULL) {
        free(config->optUdiImage);
        config->optUdiImage = NULL;
    }
    if (config->etcPath != NULL) {
        free(config->etcPath);
        config->etcPath = NULL;
    }
    if (config->kmodBasePath != NULL) {
        free(config->kmodBasePath);
        config->kmodBasePath = NULL;
    }
    if (config->kmodPath != NULL) {
        free(config->kmodPath);
        config->kmodPath = NULL;
    }
    if (config->kmodCacheFile != NULL) {
        free(config->kmodCacheFile);
        config->kmodCacheFile = NULL;
    }
    if (config->modprobePath != NULL) {
        free(config->modprobePath);
        config->modprobePath = NULL;
    }
    if (config->insmodPath != NULL) {
        free(config->insmodPath);
        config->insmodPath = NULL;
    }
    if (config->cpPath != NULL) {
        free(config->cpPath);
        config->cpPath = NULL;
    }
    if (config->mvPath != NULL) {
        free(config->mvPath);
        config->mvPath = NULL;
    }
    if (config->chmodPath != NULL) {
        free(config->chmodPath);
        config->chmodPath = NULL;
        config->chmodPath = NULL;
    }
    if (config->ddPath != NULL) {
        free(config->ddPath);
        config->ddPath = NULL;
    }
    if (config->mkfsXfsPath != NULL) {
        free(config->mkfsXfsPath);
        config->mkfsXfsPath = NULL;
    }
    if (config->rootfsType != NULL) {
        free(config->rootfsType);
        config->rootfsType = NULL;
    }
    if (config->siteResources != NULL) {
        free(config->siteResources);
        config->siteResources = NULL;
    }
    if (config->siteFs != NULL) {
        free_VolumeMap(config->siteFs, 1);
        config->siteFs = NULL;
    }

    if (config->username != NULL) {
        free(config->username);
        config->username = NULL;
    }
    if (config->sshPubKey != NULL) {
        free(config->sshPubKey);
        config->sshPubKey = NULL;
    }
    if (config->nodeIdentifier != NULL) {
        free(config->nodeIdentifier);
        config->nodeIdentifier = NULL;
    }
    if (config->jobIdentifier != NULL) {
        free(config->jobIdentifier);
        config->jobIdentifier = NULL;
    }

    char **arrays[] = {
        config->perNodeCacheAllowedFsType,
        config->gwUrl,
        config->siteEnv,
        config->siteEnvAppend,
        config->siteEnvPrepend,
        config->siteEnvUnset,
        NULL
    };
    char ***arrayPtr = NULL;
    for (arrayPtr = arrays; *arrayPtr != NULL; arrayPtr++) {
        char **iPtr = NULL;
        for (iPtr = *arrayPtr; *iPtr != NULL; iPtr++) {
            free(*iPtr);
        }
        free(*arrayPtr);
        *arrayPtr = NULL;
    }
    if (freeStruct) {
        free(config);
    }
}

size_t fprint_UdiRootConfig(FILE *fp, UdiRootConfig *config) {
    size_t idx = 0;
    size_t written = 0;

    if (config == NULL || fp == NULL) return 0;

    written += fprintf(fp, "***** UdiRootConfig *****\n");
    written += fprintf(fp, "udiMountPoint = %s\n", 
        (config->udiMountPoint != NULL ? config->udiMountPoint : ""));
    written += fprintf(fp, "loopMountPoint = %s\n", 
        (config->loopMountPoint != NULL ? config->loopMountPoint : ""));
    written += fprintf(fp, "batchType = %s\n", 
        (config->batchType != NULL ? config->batchType : ""));
    written += fprintf(fp, "system = %s\n", 
        (config->system != NULL ? config->system : ""));
    written += fprintf(fp, "defaultImageType = %s\n",
        (config->defaultImageType != NULL ? config->defaultImageType : ""));
    written += fprintf(fp, "imageBasePath = %s\n", 
        (config->imageBasePath != NULL ? config->imageBasePath : ""));
    written += fprintf(fp, "udiRootPath = %s\n", 
        (config->udiRootPath != NULL ? config->udiRootPath : ""));
    written += fprintf(fp, "perNodeCachePath = %s\n",
        (config->perNodeCachePath != NULL ? config->perNodeCachePath : ""));
    written += fprintf(fp, "perNodeCacheSizeLimit = %lu\n",
        config->perNodeCacheSizeLimit);
    written += fprintf(fp, "perNodeCacheAllowedFsType =");
    for (idx = 0; idx < config->perNodeCacheAllowedFsType_size; idx++) {
        char *ptr = config->perNodeCacheAllowedFsType[idx];
        written += fprintf(fp, " %s", ptr);
    }
    fprintf(fp, "\n");
    written += fprintf(fp, "sitePreMountHook = %s\n",
        (config->sitePreMountHook != NULL ? config->sitePreMountHook : ""));
    written += fprintf(fp, "sitePostMountHook = %s\n",
        (config->sitePostMountHook != NULL ? config->sitePostMountHook : ""));
    written += fprintf(fp, "optUdiImage = %s\n", 
        (config->optUdiImage != NULL ? config->optUdiImage : ""));
    written += fprintf(fp, "etcPath = %s\n", 
        (config->etcPath != NULL ? config->etcPath : ""));
    written += fprintf(fp, "allowLocalChroot = %d\n",
            config->allowLocalChroot);
    written += fprintf(fp, "allowLibcPwdCalls = %d\n",
            config->allowLibcPwdCalls);
    written += fprintf(fp, "populateEtcDynamically = %d\n",
            config->populateEtcDynamically);
    written += fprintf(fp, "optionalSshdAsRoot = %d\n",
            config->optionalSshdAsRoot);
    written += fprintf(fp, "autoLoadKernelModule = %d\n",
        config->autoLoadKernelModule);
    written += fprintf(fp, "mountPropagationStyle = %s\n",
        (config->mountPropagationStyle == VOLMAP_FLAG_SLAVE ?
         "slave" : "private"));
    written += fprintf(fp, "kmodBasePath = %s\n", 
        (config->kmodBasePath != NULL ? config->kmodBasePath : ""));
    written += fprintf(fp, "kmodPath = %s\n", 
        (config->kmodPath != NULL ? config->kmodPath : ""));
    written += fprintf(fp, "kmodCacheFile = %s\n", 
        (config->kmodCacheFile != NULL ? config->kmodCacheFile : ""));
    written += fprintf(fp, "rootfsType = %s\n",
        (config->rootfsType != NULL ? config->rootfsType : ""));
    written += fprintf(fp, "modprobePath = %s\n",
        (config->modprobePath != NULL ? config->modprobePath : ""));
    written += fprintf(fp, "insmodPath = %s\n",
        (config->insmodPath != NULL ? config->insmodPath : ""));
    written += fprintf(fp, "cpPath = %s\n",
        (config->cpPath != NULL ? config->cpPath : ""));
    written += fprintf(fp, "mvPath = %s\n",
        (config->mvPath != NULL ? config->mvPath : ""));
    written += fprintf(fp, "chmodPath = %s\n",
        (config->chmodPath != NULL ? config->chmodPath : ""));
    written += fprintf(fp, "ddPath = %s\n",
        (config->ddPath != NULL ? config->ddPath : ""));
    written += fprintf(fp, "mkfsXfsPath = %s\n",
        (config->mkfsXfsPath != NULL ? config->mkfsXfsPath : ""));
    written += fprintf(fp, "siteResources = %s\n",
        (config->siteResources != NULL ? config->siteResources : ""));
    written += fprintf(fp, "Image Gateway Servers = %lu servers\n", config->gwUrl_size);
    for (idx = 0; idx < config->gwUrl_size; idx++) {
        char *gwUrl = config->gwUrl[idx];
        written += fprintf(fp, "    %s\n", gwUrl);
    }
    if (config->siteFs != NULL) {
        written += fprintf(fp, "Site FS Bind-mounts = %lu fs\n", config->siteFs->n);
        written += fprint_VolumeMap(fp, config->siteFs);
    }
    written += fprintf(fp, "\nRUNTIME Options:\n");
    written += fprintf(fp, "username: %s\n",
        (config->username != NULL ? config->username : ""));
    written += fprintf(fp, "target_uid: %d\n", config->target_uid);
    written += fprintf(fp, "target_gid: %d\n", config->target_gid);
    written += fprintf(fp, "sshPubKey: %s\n",
        (config->sshPubKey != NULL ? config->sshPubKey : ""));
    written += fprintf(fp, "nodeIdentifier: %s\n",
        (config->nodeIdentifier != NULL ? config->nodeIdentifier : ""));
    written += fprintf(fp, "jobIdentifier: %s\n",
        (config->jobIdentifier != NULL ? config->jobIdentifier : ""));

    written += fprintf(fp, "***** END UdiRootConfig *****\n");
    return written;
}

int validate_UdiRootConfig(UdiRootConfig *config, int validateFlags) {
    if (config == NULL) return -1;

#define VAL_ERROR(message, code) { \
    fprintf(stderr, "%s\n", message); \
    return code; \
}

    if (validateFlags & UDIROOT_VAL_PARSE) {
        if (config->udiMountPoint == NULL || strlen(config->udiMountPoint) == 0) {
            VAL_ERROR("Base mount point \"udiMount\" is not defined", UDIROOT_VAL_PARSE);
        }
        if (config->loopMountPoint == NULL || strlen(config->loopMountPoint) == 0) {
            VAL_ERROR("Loop mount mount \"loopMount\" is not defined", UDIROOT_VAL_PARSE);
        }
        if (config->imageBasePath == NULL || strlen(config->imageBasePath) == 0) {
            VAL_ERROR("\"imagePath\" is not defined", UDIROOT_VAL_PARSE);
        }
        if (config->udiRootPath == NULL || strlen(config->udiRootPath) == 0) {
            VAL_ERROR("\"udiRootPath\" is not defined", UDIROOT_VAL_PARSE);
        }
        if (config->etcPath == NULL || strlen(config->etcPath) == 0) {
            VAL_ERROR("\"etcPath\" is not defined", UDIROOT_VAL_PARSE);
        }
        if (config->modprobePath == NULL || strlen(config->modprobePath) == 0) {
            VAL_ERROR("\"modprobePath\" is not defined", UDIROOT_VAL_PARSE);
        }
        if (config->insmodPath == NULL || strlen(config->insmodPath) == 0) {
            VAL_ERROR("\"insmodPath\" is not defined", UDIROOT_VAL_PARSE);
        }
        if (config->cpPath == NULL || strlen(config->cpPath) == 0) {
            VAL_ERROR("\"cpPath\" is not defined", UDIROOT_VAL_PARSE);
        }
        if (config->mvPath == NULL || strlen(config->mvPath) == 0) {
            VAL_ERROR("\"mvPath\" is not defined", UDIROOT_VAL_PARSE);
        }
        if (config->chmodPath == NULL || strlen(config->chmodPath) == 0) {
            VAL_ERROR("\"chmodPath\" is not defined", UDIROOT_VAL_PARSE);
        }
        if (config->ddPath == NULL || strlen(config->ddPath) == 0) {
            VAL_ERROR("\"ddPath\" is not defined", UDIROOT_VAL_PARSE);
        }
        if (config->rootfsType == NULL || strlen(config->rootfsType) == 0) {
            VAL_ERROR("\"rootfsType\" is not defined", UDIROOT_VAL_PARSE);
        }
        if (config->siteResources == NULL || strlen(config->siteResources) == 0) {
            VAL_ERROR("\"siteResources\" is not defined", UDIROOT_VAL_PARSE);
        }
    }
    if (validateFlags & UDIROOT_VAL_FILEVAL) {
        struct stat statData;
        if (stat(config->modprobePath, &statData) != 0) {
            VAL_ERROR("Specified \"modprobePath\" doesn't appear to exist.", UDIROOT_VAL_FILEVAL);
        } else if (!(statData.st_mode & S_IXUSR)) {
            VAL_ERROR("Specified \"modprobePath\" is not executable.", UDIROOT_VAL_FILEVAL);
        }
        if (stat(config->insmodPath, &statData) != 0) {
            VAL_ERROR("Specified \"insmodPath\" doesn't appear to exist.", UDIROOT_VAL_FILEVAL);
        } else if (!(statData.st_mode & S_IXUSR)) {
            VAL_ERROR("Specified \"insmodPath\" is not executable.", UDIROOT_VAL_FILEVAL);
        }
        if (stat(config->cpPath, &statData) != 0) {
            VAL_ERROR("Specified \"cpPath\" doesn't appear to exist.", UDIROOT_VAL_FILEVAL);
        } else if (!(statData.st_mode & S_IXUSR)) {
            VAL_ERROR("Specified \"cpPath\" is not executable.", UDIROOT_VAL_FILEVAL);
        }
        if (stat(config->mvPath, &statData) != 0) {
            VAL_ERROR("Specified \"mvPath\" doesn't appear to exist.", UDIROOT_VAL_FILEVAL);
        } else if (!(statData.st_mode & S_IXUSR)) {
            VAL_ERROR("Specified \"mvPath\" is not executable.", UDIROOT_VAL_FILEVAL);
        }
        if (stat(config->chmodPath, &statData) != 0) {
            VAL_ERROR("Specified \"chmodPath\" doesn't appear to exist.", UDIROOT_VAL_FILEVAL);
        } else if (!(statData.st_mode & S_IXUSR)) {
            VAL_ERROR("Specified \"chmodPath\" is not executable.", UDIROOT_VAL_FILEVAL);
        }
        if (stat(config->ddPath, &statData) != 0) {
            VAL_ERROR("Specified \"ddPath\" doesn't appear to exist.", UDIROOT_VAL_FILEVAL);
        } else if (!(statData.st_mode & S_IXUSR)) {
            VAL_ERROR("Specified \"ddPath\" is not executable.", UDIROOT_VAL_FILEVAL);
        }
        if (config->mkfsXfsPath) {
            if (stat(config->mkfsXfsPath, &statData) != 0) {
                VAL_ERROR("Specified \"mkfsXfsPath\" doesn't appear to exist.", UDIROOT_VAL_FILEVAL);
            } else if (!(statData.st_mode & S_IXUSR)) {
                VAL_ERROR("Specified \"mkfsXfsPath\" is not executable.", UDIROOT_VAL_FILEVAL);
            }
        }
        if (config->siteResources[0] != '/') {
            // note: we will find out later, through "mkdir", whether it is a valid path name or not
            VAL_ERROR("Specified \"siteResources\" is not an absolute path.", UDIROOT_VAL_FILEVAL);
        }
    }
    return 0;
}

static int _assign(const char *key, const char *value, void *t_config) {
    UdiRootConfig *config = (UdiRootConfig *)t_config;
    if (strcmp(key, "udiMount") == 0) {
        config->udiMountPoint = strdup(value);
        if (config->udiMountPoint == NULL) return 1;
    } else if (strcmp(key, "loopMount") == 0) {
        config->loopMountPoint = strdup(value);
        if (config->loopMountPoint == NULL) return 1;
    } else if (strcmp(key, "imagePath") == 0) {
        config->imageBasePath = strdup(value);
        if (config->imageBasePath == NULL) return 1;
    } else if (strcmp(key, "udiRootPath") == 0) {
        config->udiRootPath = strdup(value);
        if (config->udiRootPath == NULL) return 1;
    } else if (strcmp(key, "perNodeCachePath") == 0) {
        config->perNodeCachePath = strdup(value);
    } else if (strcmp(key, "perNodeCacheSizeLimit") == 0) {
        config->perNodeCacheSizeLimit = parseBytes(value);
    } else if (strcmp(key, "perNodeCacheAllowedFsType") == 0) {
        char *valueDup = strdup(value);
        char *search = valueDup;
        char *svPtr = NULL;
        char *ptr = NULL;
        while ((ptr = strtok_r(search, " ", &svPtr)) != NULL) {
            char **pncPtr = config->perNodeCacheAllowedFsType +
                    config->perNodeCacheAllowedFsType_size;

            strncpy_StringArray(ptr, strlen(ptr), &pncPtr,
                    &(config->perNodeCacheAllowedFsType),
                    &(config->perNodeCacheAllowedFsType_capacity),
                    PNCALLOWEDFS_ALLOC_BLOCK);
            config->perNodeCacheAllowedFsType_size++;
            search = NULL;
        }
        free(valueDup);
    } else if (strcmp(key, "sitePreMountHook") == 0) {
        config->sitePreMountHook = strdup(value);
        if (config->sitePreMountHook == NULL) return 1;
    } else if (strcmp(key, "sitePostMountHook") == 0) {
        config->sitePostMountHook = strdup(value);
        if (config->sitePostMountHook == NULL) return 1;
    } else if (strcmp(key, "optUdiImage") == 0) {
        config->optUdiImage = strdup(value);
        if (config->optUdiImage == NULL) return 1;
    } else if (strcmp(key, "etcPath") == 0) {
        config->etcPath = strdup(value);
        if (config->etcPath == NULL) return 1;
    } else if (strcmp(key, "allowLocalChroot") == 0) {
        config->allowLocalChroot = strtol(value, NULL, 10) != 0;
    } else if (strcmp(key, "allowLibcPwdCalls") == 0) {
        config->allowLibcPwdCalls = strtol(value, NULL, 10) != 0;
    } else if (strcmp(key, "optionalSshdAsRoot") == 0) {
        config->optionalSshdAsRoot = strtol(value, NULL, 10) != 0;
    } else if (strcmp(key, "populateEtcDynamically") == 0) {
        config->populateEtcDynamically = strtol(value, NULL, 10) != 0;
    } else if (strcmp(key, "autoLoadKernelModule") == 0) {
        config->autoLoadKernelModule = strtol(value, NULL, 10);
    } else if (strcmp(key, "mountPropagationStyle") == 0) {
        if (strcmp(value, "private") == 0) {
            config->mountPropagationStyle = VOLMAP_FLAG_PRIVATE;
        } else if (strcmp(value, "slave") == 0) {
            config->mountPropagationStyle = VOLMAP_FLAG_SLAVE;
        } else {
            return 1;
        }
    } else if (strcmp(key, "mountUdiRootWritable") == 0) {
        config->mountUdiRootWritable = strtol(value, NULL, 10) != 0;
    } else if (strcmp(key, "maxGroupCount") == 0) {
        config->maxGroupCount = strtoul(value, NULL, 10);
    } else if (strcmp(key, "modprobePath") == 0) {
        config->modprobePath = strdup(value);
    } else if (strcmp(key, "insmodPath") == 0) {
        config->insmodPath = strdup(value);
    } else if (strcmp(key, "cpPath") == 0) {
        config->cpPath = strdup(value);
    } else if (strcmp(key, "mvPath") == 0) {
        config->mvPath = strdup(value);
    } else if (strcmp(key, "chmodPath") == 0) {
        config->chmodPath = strdup(value);
    } else if (strcmp(key, "ddPath") == 0) {
        config->ddPath = strdup(value);
    } else if (strcmp(key, "mkfsXfsPath") == 0) {
        config->mkfsXfsPath = strdup(value);
    } else if (strcmp(key, "rootfsType") == 0) {
        config->rootfsType = strdup(value);
    } else if (strcmp(key, "gatewayTimeout") == 0) {
        config->gatewayTimeout = strtoul(value, NULL, 10);
    } else if (strcmp(key, "kmodBasePath") == 0) {
        struct utsname uts;
        memset(&uts, 0, sizeof(struct utsname));
        if (uname(&uts) != 0) {
            fprintf(stderr, "FAILED to get uname data!\n");
            return 1;
        }
        config->kmodBasePath = strdup(value);
        if (config->kmodBasePath == NULL) return 1;
        config->kmodPath = alloc_strgenf("%s/%s", config->kmodBasePath, uts.release);
    } else if (strcmp(key, "kmodCacheFile") == 0) {
        config->kmodCacheFile = strdup(value);
        if (config->kmodCacheFile == NULL) return 1;
    } else if (strcmp(key, "siteFs") == 0) {
        if (config->siteFs == NULL) {
            config->siteFs = (VolumeMap *) malloc(sizeof(VolumeMap));
            memset(config->siteFs, 0, sizeof(VolumeMap));
        }
        if (parseVolumeMapSiteFs(value, config->siteFs) != 0) {
            fprintf(stderr, "FAILED to parse siteFs volumeMap\n");
            return 1;
        }
    } else if (strcmp(key, "siteEnv") == 0) {
        char *valueDup = strdup(value);
        char *search = valueDup;
        char *svPtr = NULL;
        char *ptr = NULL;
        while ((ptr = strtok_r(search, " ", &svPtr)) != NULL) {
            char **siteEnvPtr = config->siteEnv + config->siteEnv_size;
            strncpy_StringArray(ptr, strlen(ptr), &siteEnvPtr, &(config->siteEnv), &(config->siteEnv_capacity), SITEFS_ALLOC_BLOCK);
            config->siteEnv_size++;
            search = NULL;
        }
        free(valueDup);
    } else if (strcmp(key, "siteEnvAppend") == 0) {
        char *valueDup = strdup(value);
        char *search = valueDup;
        char *svPtr = NULL;
        char *ptr = NULL;
        while ((ptr = strtok_r(search, " ", &svPtr)) != NULL) {
            char **siteEnvAppendPtr = config->siteEnvAppend + config->siteEnvAppend_size;
            strncpy_StringArray(ptr, strlen(ptr), &siteEnvAppendPtr, &(config->siteEnvAppend), &(config->siteEnvAppend_capacity), SITEFS_ALLOC_BLOCK);
            config->siteEnvAppend_size++;
            search = NULL;
        }
        free(valueDup);
    } else if (strcmp(key, "siteEnvPrepend") == 0) {
        char *valueDup = strdup(value);
        char *search = valueDup;
        char *svPtr = NULL;
        char *ptr = NULL;
        while ((ptr = strtok_r(search, " ", &svPtr)) != NULL) {
            char **siteEnvPrependPtr = config->siteEnvPrepend + config->siteEnvPrepend_size;
            strncpy_StringArray(ptr, strlen(ptr), &siteEnvPrependPtr, &(config->siteEnvPrepend), &(config->siteEnvPrepend_capacity), SITEFS_ALLOC_BLOCK);
            config->siteEnvPrepend_size++;
            search = NULL;
        }
        free(valueDup);
    } else if (strcmp(key, "siteEnvUnset") == 0) {
        char *valueDup = strdup(value);
        char *search = valueDup;
        char *svPtr = NULL;
        char *ptr = NULL;
        while ((ptr = strtok_r(search, " ", &svPtr)) != NULL) {
            char **siteEnvUnsetPtr = config->siteEnvUnset + config->siteEnvUnset_size;
            strncpy_StringArray(ptr, strlen(ptr), &siteEnvUnsetPtr, &(config->siteEnvUnset), &(config->siteEnvUnset_capacity), SITEFS_ALLOC_BLOCK);
            config->siteEnvUnset_size++;
            search = NULL;
        }
        free(valueDup);
    } else if (strcmp(key, "imageGateway") == 0) {
        char *valueDup = strdup(value);
        char *search = valueDup;
        int ret = 0;
        char *svPtr = NULL;
        char *ptr = NULL;
        while ((ptr = strtok_r(search, " ", &svPtr)) != NULL) {
            char **gwUrlPtr = config->gwUrl + config->gwUrl_size;
            strncpy_StringArray(ptr, strlen(ptr), &(gwUrlPtr), &(config->gwUrl), &(config->gwUrl_capacity), SERVER_ALLOC_BLOCK);
            config->gwUrl_size++;
            search = NULL;
        }
        free(valueDup);
        return ret;
    } else if (strcmp(key, "batchType") == 0) {
        config->batchType = strdup(value);
        if (config->batchType == NULL) return 1;
    } else if (strcmp(key, "system") == 0) {
        config->system = strdup(value);
        if (config->system == NULL) return 1;
    } else if (strcmp(key, "defaultImageType") == 0) {
        config->defaultImageType = strdup(value);
    } else if (strcmp(key, "siteResources") == 0) {
        config->siteResources = strdup(value);
    } else if (strcmp(key, "nodeContextPrefix") == 0) {
        /* do nothing, this key is defunct */
    } else {
        printf("Couldn't understand key: %s\n", key);
        return 2;
    }
    return 0; 
}

static int _validateConfigFile(const char *configFile) {
    struct stat st;
    memset(&st, 0, sizeof(struct stat));

    if (stat(configFile, &st) != 0) {
        fprintf(stderr, "Cannot find %s\n", configFile);
        return 1;
    }
#ifndef NO_ROOT_OWN_CHECK
    if (st.st_uid != 0) {
        fprintf(stderr, "udiRoot.conf must be owned by user root!");
        return UDIROOT_VAL_CFGFILE;
    }
#endif
    if (st.st_mode & (S_IWOTH | S_IWGRP)) {
        fprintf(stderr, "udiRoot.conf must not be writable by non-root users!");
        return UDIROOT_VAL_CFGFILE;
    }
    return 0;
}
