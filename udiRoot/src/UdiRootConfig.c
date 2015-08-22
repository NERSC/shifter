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

    return validate_UdiRootConfig(config, validateFlags);
}

void free_UdiRootConfig(UdiRootConfig *config, int freeStruct) {
    if (config == NULL) return;

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
    if (config->sitePreMountHook != NULL) {
        free(config->sitePreMountHook);
    }
    if (config->sitePostMountHook != NULL) {
        free(config->sitePostMountHook);
    }
    if (config->optUdiImage != NULL) {
        free(config->optUdiImage);
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

    char **arrays[] = {config->gwName, config->gwPort, config->siteFs, NULL};
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
    char **fsPtr = NULL;
    size_t idx = 0;
    size_t written = 0;

    if (config == NULL || fp == NULL) return 0;

    written += fprintf(fp, "***** UdiRootConfig *****\n");
    written += fprintf(fp, "nodeContextPrefix = %s\n", 
        (config->nodeContextPrefix != NULL ? config->nodeContextPrefix : ""));
    written += fprintf(fp, "udiMountPoint = %s\n", 
        (config->udiMountPoint != NULL ? config->udiMountPoint : ""));
    written += fprintf(fp, "loopMountPoint = %s\n", 
        (config->loopMountPoint != NULL ? config->loopMountPoint : ""));
    written += fprintf(fp, "batchType = %s\n", 
        (config->batchType != NULL ? config->batchType : ""));
    written += fprintf(fp, "system = %s\n", 
        (config->system != NULL ? config->system : ""));
    written += fprintf(fp, "imageBasePath = %s\n", 
        (config->imageBasePath != NULL ? config->imageBasePath : ""));
    written += fprintf(fp, "udiRootPath = %s\n", 
        (config->udiRootPath != NULL ? config->udiRootPath : ""));
    written += fprintf(fp, "sitePreMountHook = %s\n",
        (config->sitePreMountHook != NULL ? config->sitePreMountHook : ""));
    written += fprintf(fp, "sitePostMountHook = %s\n",
        (config->sitePostMountHook != NULL ? config->sitePostMountHook : ""));
    written += fprintf(fp, "optUdiImage = %s\n", 
        (config->optUdiImage != NULL ? config->optUdiImage : ""));
    written += fprintf(fp, "etcPath = %s\n", 
        (config->etcPath != NULL ? config->etcPath : ""));
    written += fprintf(fp, "kmodBasePath = %s\n", 
        (config->kmodBasePath != NULL ? config->kmodBasePath : ""));
    written += fprintf(fp, "kmodPath = %s\n", 
        (config->kmodPath != NULL ? config->kmodPath : ""));
    written += fprintf(fp, "kmodCacheFile = %s\n", 
        (config->kmodCacheFile != NULL ? config->kmodCacheFile : ""));
    written += fprintf(fp, "Image Gateway Servers = %lu servers\n", config->gateway_size);
    for (idx = 0; idx < config->gateway_size; idx++) {
        char *gwName = config->gwName[idx];
        char *gwPort = config->gwPort[idx];
        written += fprintf(fp, "    %s:%s\n", gwName, gwPort);
    }
    written += fprintf(fp, "Site FS Bind-mounts = %lu fs\n", config->siteFs_size);
    for (fsPtr = config->siteFs; *fsPtr != NULL; fsPtr++) {
        written += fprintf(fp, "    %s\n", *fsPtr);
    }
    written += fprintf(fp, "***** END UdiRootConfig *****\n");
    return written;
}

int validate_UdiRootConfig(UdiRootConfig *config, int validateFlags) {
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
        config->allowLocalChroot = atoi(value) != 0;
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
        char *valueDup = strdup(value);
        char *search = valueDup;
        char *ptr = NULL;
        while ((ptr = strtok(search, " ")) != NULL) {
            char **siteFsPtr = config->siteFs + config->siteFs_size;
            strncpy_StringArray(ptr, strlen(ptr), &siteFsPtr, &(config->siteFs), &(config->siteFs_capacity), SITEFS_ALLOC_BLOCK);
            config->siteFs_size++;
            search = NULL;
        }
        free(valueDup);
    } else if (strcmp(key, "imageGateway") == 0) {
        char *valueDup = strdup(value);
        char *search = valueDup;
        int ret = 0;
        char *ptr = NULL;
        while ((ptr = strtok(search, " ")) != NULL) {
            char *dPtr = strchr(ptr, ':');
            const char *portStr = NULL;
            char **gwNamePtr = config->gwName + config->gateway_size;
            char **gwPortPtr = config->gwPort + config->gateway_size;
            search = NULL;
            if (dPtr == NULL) {
                portStr = IMAGEGW_PORT_DEFAULT;
            } else {
                int port = 0;
                *dPtr++ = 0;
                port = atoi(dPtr);
                if (port != 0) {
                    portStr = dPtr;
                }
            }
            if (ptr != NULL && portStr != NULL) {
                strncpy_StringArray(ptr, strlen(ptr), &(gwNamePtr), &(config->gwName), &(config->gwName_capacity), SERVER_ALLOC_BLOCK);
                strncpy_StringArray(portStr, strlen(portStr), &(gwPortPtr), &(config->gwPort), &(config->gwPort_capacity), SERVER_ALLOC_BLOCK);
                config->gateway_size++;
            } else {
                fprintf(stderr, "FAILED to understand image gateway: %s, port %s\n", ptr, portStr);
                ret = 1;
            }
        }
        free(valueDup);
        return ret;
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

static int _validateConfigFile(const char *configFile) {
    struct stat st;
    memset(&st, 0, sizeof(struct stat));

    if (stat(configFile, &st) != 0) {
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

#ifdef _TESTHARNESS_UDIROOTCONFIG
#include <CppUTest/CommandLineTestRunner.h>

TEST_GROUP(UdiRootConfigTestGroup) {
};

TEST(UdiRootConfigTestGroup, ParseUdiRootConfig_basic) {
    UdiRootConfig config;

    memset(&config, 0, sizeof(UdiRootConfig));
    int ret = parse_UdiRootConfig("test_udiRoot.conf", &config, 0);
    printf("value: %d\n", ret);
    CHECK(ret == 0);
    CHECK(strcmp(config.system, "testSystem") == 0);
    free_UdiRootConfig(&config, 0);
}

TEST(UdiRootConfigTestGroup, ParseUdiRootConfig_display) {
    UdiRootConfig config;
    memset(&config, 0, sizeof(UdiRootConfig));
    printf("about to parse\n");
    int ret = parse_UdiRootConfig("test_udiRoot.conf", &config, 0);
    CHECK(ret == 0);
    FILE *output = fopen("ParseUdiRootConfig_display.out", "w");
    CHECK(output != NULL);
    size_t nwrite = fprint_UdiRootConfig(output, &config);
    fclose(output);
    free_UdiRootConfig(&config, 0);
}


int main(int argc, char** argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
#endif
