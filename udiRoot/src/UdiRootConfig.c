/* Shifter, Copyright (c) 2015, The Regents of the University of California,
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

#define _GNU_SOURCE
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

static char *_trim(char *);
static int _assign(char *key, char *value, void *tUdiRootConfig);
static int _validateConfigFile();

int parse_UdiRootConfig(UdiRootConfig *config, int validateFlags) {

    if (_validateConfigFile() != 0) {
        return UDIROOT_VAL_CFGFILE;
    }

    if (shifter_parseConfig(UDIROOT_CONFIG, config, _assign) != 0) {
        return UDIROOT_VAL_PARSE;
    }

    return validate_UdiRootConfig(config, validateFlags);
}

void free_UdiRootConfig(UdiRootConfig *config) {
    if (config->nodeContextPrefix != NULL) {
        free(config->nodeContextPrefix);
    }
    if (config->udiMountPoint != NULL) {
        free(config->udiMountPoint);
    }
    if (config->loopMountPoint != NULL) {
        free(config->loopMountPoint);
    }
    if (config->batchType != NULL) {
        free(config->batchType);
    }
    if (config->system != NULL) {
        free(config->system);
    }
    if (config->imageBasePath != NULL) {
        free(config->imageBasePath);
    }
    if (config->udiRootPath != NULL) {
        free(config->udiRootPath);
    }
    if (config->udiRootSiteInclude != NULL) {
        free(config->udiRootSiteInclude);
    }
    if (config->sshPath != NULL) {
        free(config->sshPath);
    }
    if (config->etcPath != NULL) {
        free(config->etcPath);
    }
    if (config->kmodBasePath != NULL) {
        free(config->kmodBasePath);
    }
    if (config->kmodPath != NULL) {
        free(config->kmodPath);
    }
    if (config->kmodCacheFile != NULL) {
        free(config->kmodCacheFile);
    }
    if (config->servers != NULL) {
        size_t idx = 0;
        for (idx = 0; idx < config->serversCount; idx++) {
            free(config->servers[idx]->server);
            free(config->servers[idx]);
        }
        free(config->servers);
    }
    if (config->siteFs != NULL) {
        size_t idx = 0;
        for (idx = 0; idx < config->siteFsCount; idx++) {
            free(config->siteFs[idx]);
        }
        free(config->siteFs);
    }
    free(config);
}

void fprint_UdiRootConfig(FILE *fp, UdiRootConfig *config) {
    size_t idx = 0;

    if (config == NULL || fp == NULL) return;

    fprintf(fp, "***** UdiRootConfig *****\n");
    fprintf(fp, "nodeContextPrefix = %s\n", 
        (config->nodeContextPrefix != NULL ? config->nodeContextPrefix : ""));
    fprintf(fp, "udiMountPoint = %s\n", 
        (config->udiMountPoint != NULL ? config->udiMountPoint : ""));
    fprintf(fp, "loopMountPoint = %s\n", 
        (config->loopMountPoint != NULL ? config->loopMountPoint : ""));
    fprintf(fp, "batchType = %s\n", 
        (config->batchType != NULL ? config->batchType : ""));
    fprintf(fp, "system = %s\n", 
        (config->system != NULL ? config->system : ""));
    fprintf(fp, "imageBasePath = %s\n", 
        (config->imageBasePath != NULL ? config->imageBasePath : ""));
    fprintf(fp, "udiRootPath = %s\n", 
        (config->udiRootPath != NULL ? config->udiRootPath : ""));
    fprintf(fp, "udiRootSiteInclude = %s\n", 
        (config->udiRootSiteInclude != NULL ? config->udiRootSiteInclude : ""));
    fprintf(fp, "sshPath = %s\n", 
        (config->sshPath != NULL ? config->sshPath : ""));
    fprintf(fp, "etcPath = %s\n", 
        (config->etcPath != NULL ? config->etcPath : ""));
    fprintf(fp, "kmodBasePath = %s\n", 
        (config->kmodBasePath != NULL ? config->kmodBasePath : ""));
    fprintf(fp, "kmodPath = %s\n", 
        (config->kmodPath != NULL ? config->kmodPath : ""));
    fprintf(fp, "kmodCacheFile = %s\n", 
        (config->kmodCacheFile != NULL ? config->kmodCacheFile : ""));
    fprintf(fp, "Image Gateway Servers = %lu servers\n",  config->serversCount);
    for (idx = 0; idx < config->serversCount; idx++) {
        fprintf(fp, "    %s:%d\n", config->servers[idx]->server,
            config->servers[idx]->port);
    }
    fprintf(fp, "Site FS Bind-mounts = %lu fs\n", config->siteFsCount);
    for (idx = 0; idx < config->siteFsCount; idx++) {
        fprintf(fp, "    %s\n", config->siteFs[idx]);
    }
    fprintf(fp, "***** END UdiRootConfig *****\n");
}

int validate_UdiRootConfig(UdiRootConfig *config, int validateFlags) {
    return 0;
}

static char *_trim(char *str) {
    char *ptr = str;
    ssize_t len = 0;
    if (str == NULL) return NULL;
    for ( ; isspace(*ptr) && *ptr != 0; ptr++) {
        // that's it
    }
    if (*ptr == 0) return ptr;
    len = strlen(ptr) - 1;
    for ( ; isspace(*(ptr + len)) && len > 0; len--) {
        *(ptr + len) = 0;
    }
    return ptr;
}

static int _assign(char *key, char *value, void *t_config) {
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
    } else if (strcmp(key, "udiRootSiteInclude") == 0) {
        config->udiRootSiteInclude = strdup(value);
        if (config->udiRootSiteInclude == NULL) return 1;
    } else if (strcmp(key, "sshPath") == 0) {
        config->sshPath = strdup(value);
        if (config->sshPath == NULL) return 1;
    } else if (strcmp(key, "etcPath") == 0) {
        config->etcPath = strdup(value);
        if (config->etcPath == NULL) return 1;
    } else if (strcmp(key, "kmodBasePath") == 0) {
        struct utsname uts;
        size_t kmodLength = 0;
        memset(&uts, 0, sizeof(struct utsname));
        if (uname(&uts) != 0) {
            fprintf(stderr, "FAILED to get uname data!\n");
            return 1;
        }
        config->kmodBasePath = strdup(value);
        if (config->kmodBasePath == NULL) return 1;
        kmodLength = strlen(config->kmodBasePath);
        kmodLength += strlen(uts.release);
        kmodLength += 2;
        config->kmodPath = (char *) malloc(sizeof(char) * kmodLength);
        snprintf(config->kmodPath, kmodLength, "%s/%s", config->kmodBasePath, uts.release);
    } else if (strcmp(key, "kmodCacheFile") == 0) {
        config->kmodCacheFile = strdup(value);
        if (config->kmodCacheFile == NULL) return 1;
    } else if (strcmp(key, "siteFs") == 0) {
        char *search = value;
        char *ptr = NULL;
        while ((ptr = strtok(search, " ")) != NULL) {
            search = NULL;
            config->siteFs = (char **) realloc(config->siteFs, sizeof(char*) * (config->siteFsCount+1));
            if (config->siteFs == NULL) return 1;
            config->siteFs[config->siteFsCount] = strdup(ptr);
            config->siteFsCount++;
        }
    } else if (strcmp(key, "imageGateway") == 0) {
        char *search = value;
        char *ptr = NULL;
        while ((ptr = strtok(search, " ")) != NULL) {
            char *dPtr = NULL;
            ImageGwServer *newServer = (ImageGwServer*) malloc(sizeof(ImageGwServer));
            if (newServer == NULL) return 1;
            dPtr = strchr(ptr, ':');
            if (dPtr == NULL) {
                newServer->port = IMAGEGW_PORT_DEFAULT;
            } else {
                *dPtr++ = 0;
                newServer->port = atoi(dPtr);
            }
            newServer->server = strdup(ptr);

            search = NULL;
            config->servers = (ImageGwServer **) realloc(config->servers, sizeof(ImageGwServer*) * (config->serversCount+1));
            if (config->servers == NULL) return 1;
            config->servers[config->serversCount] = newServer;
            config->serversCount++;
        }
    } else if (strcmp(key, "batchType") == 0) {
        config->batchType = strdup(value);
        if (config->batchType == NULL) return 1;
    } else if (strcmp(key, "system") == 0) {
        config->system = strdup(value);
        if (config->system == NULL) return 1;
    } else if (strcmp(key, "nodeContextPrefix") == 0) {
        config->nodeContextPrefix = strdup(value);
        if (config->nodeContextPrefix == NULL) return 1;
    } else {
        printf("Couldn't understand key: %s\n", key);
        return 2;
    }
    return 0; 
}

static int _validateConfigFile() {
    struct stat st;
    memset(&st, 0, sizeof(struct stat));

    if (stat(UDIROOT_CONFIG, &st) != 0) {
        return 1;
    }
#ifndef NO_ROOT_OWN_CHECK
    if (st.st_uid != 0) {
        return UDIROOT_VAL_CFGFILE;
    }
#endif
    if (st.st_mode & (S_IWOTH | S_IWGRP)) {
        return UDIROOT_VAL_CFGFILE;
    }
    return 0;
}
