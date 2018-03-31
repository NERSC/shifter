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
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "UdiRootConfig.h"
#include "utility.h"
#include "shifter_mem.h"

#define SITEFS_ALLOC_BLOCK 16
#define SERVER_ALLOC_BLOCK 3
#define PNCALLOWEDFS_ALLOC_BLOCK 10

static int _assign(const char *key, const char *value, void *tUdiRootConfig);
static int _validateConfigFile(const char *);

void free_ShifterModule(ShifterModule *module, int free_struct) {
    char **ptr = NULL;

    if (module == NULL) return;
    if (module->name != NULL) {
        free(module->name);
        module->name = NULL;
    }
    if (module->userhook != NULL) {
        free(module->userhook);
        module->userhook = NULL;
    }
    if (module->siteEnv != NULL) {
        for (ptr = module->siteEnv; ptr && *ptr; ptr++) {
            free(*ptr);
        }
        free(module->siteEnv);
        module->siteEnv = NULL;
    }
    if (module->siteEnvPrepend != NULL) {
        for (ptr = module->siteEnvPrepend; ptr && *ptr; ptr++) {
            free(*ptr);
        }
        free(module->siteEnvPrepend);
        module->siteEnvPrepend = NULL;
    }
    if (module->siteEnvAppend != NULL) {
        for (ptr = module->siteEnvAppend; ptr && *ptr; ptr++) {
            free(*ptr);
        }
        free(module->siteEnvAppend);
        module->siteEnvAppend = NULL;
    }
    if (module->siteEnvUnset != NULL) {
        for (ptr = module->siteEnvUnset; ptr && *ptr; ptr++) {
            free(*ptr);
        }
        free(module->siteEnvUnset);
        module->siteEnvUnset = NULL;
    }
    if (module->siteFs != NULL) {
        free_VolumeMap(module->siteFs, 1);
        module->siteFs = NULL;
    }
    if (module->copyPath != NULL) {
        free(module->copyPath);
        module->copyPath = NULL;
    }
    if (module->conflict_str != NULL) {
        for (ptr = module->conflict_str; ptr && *ptr; ptr++) {
            free(*ptr);
        }
        free(module->conflict_str);
        module->conflict_str = NULL;
    }
    if (free_struct) {
        free(module);
    }
}

/* this function _requires_ that _all_ of the modules shifter will use for
 * this invocataion are already parsed, since it is storing pointers into
 * the array of modules */
int parse_selected_ShifterModule(const char *selected, UdiRootConfig *config) {
    char *selected_tmp = NULL;
    char *search = NULL;
    char *svPtr = NULL;
    char *ptr = NULL;
    int rc = 0;

    if (!selected || !config) {
        return -1;
    }
    if (config->selectedModulesStr) {
        free(config->selectedModulesStr);
        config->selectedModulesStr = NULL;
    }
    config->selectedModulesStr = _strdup(selected);
    selected_tmp = _strdup(selected);
    search = shifter_trim(selected_tmp);

    /* even if there are default modules active, disable them, user override */
    if (config->active_modules) {
        free(config->active_modules);
        config->active_modules = NULL;
    }
    config->n_active_modules = 0;

    /* if user specified none, then end */
    if (strcasecmp(search, "none") == 0) {
        rc = 0;
        goto finish;
    }

    while ((ptr = strtok_r(search, ",", &svPtr)) != NULL) {
        int idx = 0;
        int jdx = 0;
        int kdx = 0;
        bool found = false;
        search = NULL;

        for (idx = 0; idx < config->n_modules && !found; idx++) {
            if (strcmp(ptr, config->modules[idx].name) == 0) {
                if (config->modules[idx].enabled == 0) {
                    fprintf(stderr, "WARNING: module %s is not enabled for use.\n", config->modules[idx].name);
                    continue;
                }
                for (jdx = 0; jdx < config->modules[idx].n_conflict; jdx++) {
                    for (kdx = 0; kdx < config->n_active_modules; kdx++) {
                        if (config->active_modules[kdx] == config->modules[idx].conflict[jdx]) {
                            fprintf(stderr, "ERROR: cannot load conflicting modules %s and %s\n", config->modules[idx].name, config->active_modules[kdx]->name);
                            rc = -1;
                            goto finish;
                        }
                    }
                }
                for (jdx = 0; jdx < config->n_active_modules; jdx++) {
                    for (kdx = 0; kdx < config->active_modules[jdx]->n_conflict; kdx++) {
                        if (config->active_modules[jdx]->conflict[kdx] == &config->modules[idx]) {
                            fprintf(stderr, "ERROR: cannot load conflicting modules %s and %s\n", config->modules[idx].name, config->active_modules[jdx]->name);
                            rc = -1;
                            goto finish;
                        }
                    }
                }

                config->active_modules =
                    _realloc(config->active_modules,
                             sizeof(ShifterModule *) *
                             (config->n_active_modules + 2)
                    );

                config->active_modules[config->n_active_modules] =
                    &config->modules[idx];

                config->n_active_modules++;
                config->active_modules[config->n_active_modules] = NULL;
                found = true;
            }
        }
        if (!found) {
            fprintf(stderr, "Unknown shifter module: %s\n", ptr);
            rc = -1;
            goto finish;
        }
    }
    rc = 0;

finish:
    if (selected_tmp)
        free(selected_tmp);
    return rc;
}

int parse_UdiRootConfig(const char *configFile, UdiRootConfig *config, int validateFlags) {
    int ret = 0;

    ret = _validateConfigFile(configFile);
    if (ret != 0) {
        return ret;
    }

    if (shifter_parseConfig(configFile, '=', config, _assign) != 0) {
        return UDIROOT_VAL_PARSE;
    }

    if (ShifterModule_postprocessing(config) != 0) {
        return UDIROOT_VAL_PARSE;
    }

    ret = validate_UdiRootConfig(config, validateFlags);
    return ret;
}

int ShifterModule_postprocessing(UdiRootConfig *config) {
    int i = 0;
    int j = 0;
    char **ptr = NULL;
    for (i = 0; i < config->n_modules; i++) {
        int found_n_conflicts = 0;
        size_t alloc_size = 0;
        if (config->modules[i].n_conflict == 0) {
            continue;
        }
        alloc_size = sizeof(ShifterModule *) *
                           (config->modules[i].n_conflict + 1);
        config->modules[i].conflict = _malloc(alloc_size);
        memset(config->modules[i].conflict, 0, alloc_size);
        for (ptr = config->modules[i].conflict_str; ptr && *ptr; ptr++) {
            int found = 0;
            for (j = 0; j < config->n_modules; j++) {
                if (strcmp(*ptr, config->modules[j].name) == 0) {
                    config->modules[i].conflict[found_n_conflicts] =
                            &(config->modules[j]);
                    found_n_conflicts++;
                    found++;
                    break;
                }
            }
            if (!found) {
                fprintf(stderr, "FAILED to find %s conflict for module %s\n",
                        *ptr, config->modules[i].name);
                return 1;
            }
        }
        if (found_n_conflicts != config->modules[i].n_conflict) {
            fprintf(stderr, "FAILED To find all conflicts for module %s\n",
                    config->modules[i].name);
            return 1;
        }
    }
    if (config->defaultModulesStr) {
        return parse_selected_ShifterModule(config->defaultModulesStr, config);
    }
    return 0;
}

void free_UdiRootConfig(UdiRootConfig *config, int freeStruct) {
    int iidx = 0;
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
    for (iidx = 0; iidx < config->n_modules; iidx++) {
        free_ShifterModule(&(config->modules[iidx]), 0);
    }
    if (config->modules) {
        free(config->modules);
        config->modules = NULL;
    }
    if (config->defaultModulesStr) {
        free(config->defaultModulesStr);
        config->defaultModulesStr = NULL;
    }
    if (config->selectedModulesStr) {
        free(config->selectedModulesStr);
        config->selectedModulesStr = NULL;
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
    int iidx = 0;
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
    written += fprintf(fp, "mountPropagationStyle = %s\n",
        (config->mountPropagationStyle == VOLMAP_FLAG_SLAVE ?
         "slave" : "private"));
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
    written += fprintf(fp, "Image Gateway Servers = %lu servers\n", config->gwUrl_size);
    for (idx = 0; idx < config->gwUrl_size; idx++) {
        char *gwUrl = config->gwUrl[idx];
        written += fprintf(fp, "    %s\n", gwUrl);
    }
    if (config->siteFs != NULL) {
        written += fprintf(fp, "Site FS Bind-mounts = %lu fs\n", config->siteFs->n);
        written += fprint_VolumeMap(fp, config->siteFs);
    }
    for (iidx = 0; iidx < config->n_modules; iidx++) {
        written += fprint_ShifterModule(fp, &(config->modules[iidx]));
    }
    written += fprintf(fp, "defaultModules: %s\n", config->defaultModulesStr);
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
    written += fprintf(fp, "selectedModules: %s\n", config->selectedModulesStr);
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
    }
    return 0;
}

static int _assign(const char *key, const char *value, void *t_config) {
    UdiRootConfig *config = (UdiRootConfig *)t_config;
    if (strcmp(key, "udiMount") == 0) {
        config->udiMountPoint = _strdup(value);
        if (config->udiMountPoint == NULL) return 1;
    } else if (strcmp(key, "loopMount") == 0) {
        config->loopMountPoint = _strdup(value);
        if (config->loopMountPoint == NULL) return 1;
    } else if (strcmp(key, "imagePath") == 0) {
        config->imageBasePath = _strdup(value);
        if (config->imageBasePath == NULL) return 1;
    } else if (strcmp(key, "udiRootPath") == 0) {
        config->udiRootPath = _strdup(value);
        if (config->udiRootPath == NULL) return 1;
    } else if (strcmp(key, "perNodeCachePath") == 0) {
        config->perNodeCachePath = _strdup(value);
    } else if (strcmp(key, "perNodeCacheSizeLimit") == 0) {
        config->perNodeCacheSizeLimit = parseBytes(value);
    } else if (strcmp(key, "perNodeCacheAllowedFsType") == 0) {
        char *valueDup = _strdup(value);
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
        config->sitePreMountHook = _strdup(value);
        if (config->sitePreMountHook == NULL) return 1;
    } else if (strcmp(key, "sitePostMountHook") == 0) {
        config->sitePostMountHook = _strdup(value);
        if (config->sitePostMountHook == NULL) return 1;
    } else if (strcmp(key, "optUdiImage") == 0) {
        config->optUdiImage = _strdup(value);
        if (config->optUdiImage == NULL) return 1;
    } else if (strcmp(key, "etcPath") == 0) {
        config->etcPath = _strdup(value);
        if (config->etcPath == NULL) return 1;
    } else if (strcmp(key, "allowLocalChroot") == 0) {
        config->allowLocalChroot = strtol(value, NULL, 10) != 0;
    } else if (strcmp(key, "allowLibcPwdCalls") == 0) {
        config->allowLibcPwdCalls = strtol(value, NULL, 10) != 0;
    } else if (strcmp(key, "optionalSshdAsRoot") == 0) {
        fprintf(stderr, "IGNORING parameter optionalSshdAsRoot, deprecated.\n");
    } else if (strcmp(key, "populateEtcDynamically") == 0) {
        config->populateEtcDynamically = strtol(value, NULL, 10) != 0;
    } else if (strcmp(key, "autoLoadKernelModule") == 0) {
        fprintf(stderr, "IGNORING parameter autoLoadKernelModule, deprecated.\n");
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
        config->modprobePath = _strdup(value);
    } else if (strcmp(key, "insmodPath") == 0) {
        config->insmodPath = _strdup(value);
    } else if (strcmp(key, "cpPath") == 0) {
        config->cpPath = _strdup(value);
    } else if (strcmp(key, "mvPath") == 0) {
        config->mvPath = _strdup(value);
    } else if (strcmp(key, "chmodPath") == 0) {
        config->chmodPath = _strdup(value);
    } else if (strcmp(key, "ddPath") == 0) {
        config->ddPath = _strdup(value);
    } else if (strcmp(key, "mkfsXfsPath") == 0) {
        config->mkfsXfsPath = _strdup(value);
    } else if (strcmp(key, "rootfsType") == 0) {
        config->rootfsType = _strdup(value);
    } else if (strcmp(key, "gatewayTimeout") == 0) {
        config->gatewayTimeout = strtoul(value, NULL, 10);
    } else if (strcmp(key, "kmodBasePath") == 0) {
        fprintf(stderr, "IGNORING parameter kmodBasePath, deprecated.\n");
    } else if (strcmp(key, "kmodCacheFile") == 0) {
        fprintf(stderr, "IGNORING parameter kmodCacheFile, deprecated.\n");
    } else if (strcmp(key, "siteFs") == 0) {
        if (config->siteFs == NULL) {
            config->siteFs = (VolumeMap *) _malloc(sizeof(VolumeMap));
            memset(config->siteFs, 0, sizeof(VolumeMap));
        }
        if (parseVolumeMapSiteFs(value, config->siteFs) != 0) {
            fprintf(stderr, "FAILED to parse siteFs volumeMap\n");
            return 1;
        }
    } else if (strcmp(key, "siteEnv") == 0) {
        char *valueDup = _strdup(value);
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
        char *valueDup = _strdup(value);
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
        char *valueDup = _strdup(value);
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
        char *valueDup = _strdup(value);
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
        char *valueDup = _strdup(value);
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
        config->batchType = _strdup(value);
        if (config->batchType == NULL) return 1;
    } else if (strcmp(key, "system") == 0) {
        config->system = _strdup(value);
        if (config->system == NULL) return 1;
    } else if (strcmp(key, "defaultImageType") == 0) {
        config->defaultImageType = _strdup(value);
    } else if (strcmp(key, "nodeContextPrefix") == 0) {
        /* do nothing, this key is defunct */
    } else if (strncmp(key, "module_", 7) == 0) {
        parse_ShifterModule_key(config, key, value);
    } else if (strcmp(key, "defaultModules") == 0) {
        config->defaultModulesStr = _strdup(value);
    } else {
        printf("Couldn't understand key: %s\n", key);
        return 2;
    }
    return 0;
}

int parse_ShifterModule_key(UdiRootConfig *config, const char *key,
    const char *value)
{
    char *tmpkey = NULL;
    char *tmpvalue = NULL;
    char *name = NULL;
    char *subkey = NULL;
    int keycounter = 0;
    char *search = NULL;
    char *svPtr = NULL;
    char *ptr = NULL;
    ShifterModule *module = NULL;
    int rc = 0;
    int idx = 0;
    if (config == NULL || key == NULL || value == NULL) {
        rc = 1;
        goto cleanup;
    }
    tmpkey = _strdup(key);

    /* parse the key */
    search = tmpkey;
    svPtr = NULL;
    while ((ptr = strtok_r(search, "_", &svPtr)) != NULL) {
        if (keycounter == 0 && strcmp(ptr, "module") != 0) {
            fprintf(stderr, "FAILED to parse ShifterModule key %s\n", key);
            rc = 1;
            goto cleanup;
        } else if (keycounter == 1) {
            name = ptr;
        } else if (keycounter == 2) {
            subkey = ptr;
        }
        search = NULL; /* needed for strtok */
        keycounter++;
    }
    if (name == NULL || subkey == NULL) {
        fprintf(stderr, "FAILED to parse ShifterModule key %s\n", key);
        rc = 1;
        goto cleanup;
    }

    /* find the module or allocate a new one if it does not exist*/
    module = NULL;
    for (idx = 0; idx < config->n_modules; idx++) {
        if (strcmp(name, config->modules[idx].name) == 0) {
            module = &(config->modules[idx]);
        }
    }
    if (module == NULL) {
        size_t alloc_size = sizeof(ShifterModule) * (config->n_modules + 1);
        config->modules = _realloc(config->modules, alloc_size);
        memset(&(config->modules[config->n_modules]), 0, sizeof(ShifterModule));
        module = &(config->modules[config->n_modules]);
        config->n_modules += 1;
        module->name = _strdup(name);

        /* default module to enabled */
        module->enabled = 1;
    }

    /* populate module based on subkey and value */
    if (strcmp(subkey, "userhook") == 0) {
        module->userhook = _strdup(value);
    } else if (strcmp(subkey, "roothook") == 0) {
        module->roothook = _strdup(value);
    } else if (strcmp(subkey, "siteEnv") == 0 ||
                strcmp(subkey, "siteEnvPrepend") == 0 ||
                strcmp(subkey, "siteEnvAppend") == 0 ||
                strcmp(subkey, "siteEnvUnset") == 0 ||
                strcmp(subkey, "conflict") == 0)
    {
        tmpvalue = _strdup(value);
        search = tmpvalue;
        svPtr = NULL;
        char **ptrarray = NULL;
        char **end_ptrarray = NULL;
        size_t count = 0;
        size_t capacity = 0;
        while ((ptr = strtok_r(search, " ", &svPtr)) != NULL) {
            end_ptrarray = ptrarray + count;
            strncpy_StringArray(ptr, strlen(ptr), &end_ptrarray, &ptrarray,
                                &capacity, SITEFS_ALLOC_BLOCK);
            count++;
            search = NULL;
        }

        if (strcmp(subkey, "siteEnv") == 0) {
            module->siteEnv = ptrarray;
            module->n_siteEnv = count;
        } else if (strcmp(subkey, "siteEnvPrepend") == 0) {
            module->siteEnvPrepend = ptrarray;
            module->n_siteEnvPrepend = count;
        } else if (strcmp(subkey, "siteEnvAppend") == 0) {
            module->siteEnvAppend = ptrarray;
            module->n_siteEnvAppend = count;
        } else if (strcmp(subkey, "siteEnvUnset") == 0) {
            module->siteEnvUnset = ptrarray;
            module->n_siteEnvUnset = count;
        } else if (strcmp(subkey, "conflict") == 0) {
            module->conflict_str = ptrarray;
            module->n_conflict = count;
        }
    } else if (strcmp(subkey, "siteFs") == 0) {
        if (module->siteFs == NULL) {
            module->siteFs = (VolumeMap *) _malloc(sizeof(VolumeMap));
            memset(module->siteFs, 0, sizeof(VolumeMap));
        }
        if (parseVolumeMapSiteFs(value, module->siteFs) != 0) {
            fprintf(stderr, "FAILED to parse module siteFs volumeMap\n");
            rc = 1;
            goto cleanup;
        }
    } else if (strcmp(subkey, "copyPath") == 0) {
        module->copyPath = _strdup(value);
    } else if (strcmp(subkey, "enabled") == 0) {
        module->enabled = strtol(value, NULL, 10) != 0;
    }

cleanup:
    if (tmpkey != NULL) {
        free(tmpkey);
        tmpkey = NULL;
    }
    if (tmpvalue != NULL) {
        free(tmpvalue);
        tmpvalue = NULL;
    }
    return rc;
}

size_t fprint_ShifterModule(FILE *fp, ShifterModule *module) {
    size_t written = 0;
    char **ptr = NULL;

    if (fp == NULL || module == NULL)
        return 0;

    written += fprintf(fp, "Shifter Module: %s\n", module->name);
    written += fprintf(fp, "====================================\n");
    written += fprintf(fp, "userhook: %s\n", module->userhook);
    written += fprintf(fp, "roothook: %s\n", module->roothook);
    written += fprintf(fp, "siteEnv:\n");
    for (ptr = module->siteEnv; ptr && *ptr; ptr++) {
        written += fprintf(fp, "        %s\n", *ptr);
    }
    written += fprintf(fp, "siteEnvPrepend:\n");
    for (ptr = module->siteEnvPrepend; ptr && *ptr; ptr++) {
        written += fprintf(fp, "        %s\n", *ptr);
    }
    written += fprintf(fp, "siteEnvAppend:\n");
    for (ptr = module->siteEnvAppend; ptr && *ptr; ptr++) {
        written += fprintf(fp, "        %s\n", *ptr);
    }
    written += fprintf(fp, "siteEnvUnset:\n");
    for (ptr = module->siteEnvUnset; ptr && *ptr; ptr++) {
        written += fprintf(fp, "        %s\n", *ptr);
    }
    written += fprintf(fp, "conflict:\n");
    for (ptr = module->conflict_str; ptr && *ptr; ptr++) {
        written += fprintf(fp, "        %s\n", *ptr);
    }
    written += fprintf(fp, "VolumeMap: ");
    written += fprint_VolumeMap(fp, module->siteFs);
    written += fprintf(fp, "\n");
    written += fprintf(fp, "copyPath: %s\n", module->copyPath);
    written += fprintf(fp, "enabled: %d\n", module->enabled);
    written += fprintf(fp, "====================================\n\n");
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
