#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "UdiRootConfig.h"

static char *_trim(char *);
static int _assign(UdiRootConfig *config, char *key, char *value);
static int _validateConfigFile();

int parse_UdiRootConfig(UdiRootConfig *config, int validateFlags) {
    FILE *fp = NULL;
    char *linePtr = NULL;
    char *ptr = NULL;
    size_t lineSize = 0;
    ssize_t nRead = 0;
    int multiline = 0;

    char *key = NULL;
    char *value = NULL;
    char *tValue = NULL;
    size_t valueLen = 0;
    size_t tValueLen = 0;

    if (_validateConfigFile() != 0) {
        return UDIROOT_VAL_CFGFILE;
    }

    fp = fopen(UDIROOT_CONFIG, "r");
    if (fp == NULL) {
        return UDIROOT_VAL_PARSE;
    }
    while (!feof(fp) && !ferror(fp)) {
        nRead = getline(&linePtr, &nRead, fp);
        if (nRead <= 0) break;

        // get key/value pair
        if (!multiline) {
            ptr = strchr(linePtr, '=');
            if (ptr == NULL) continue;
            *ptr++ = 0;
            key = _trim(strdup(linePtr));
            if (key == NULL) {
                return 1;
            }
            tValue = _trim(ptr);
        } else {
            tValue = _trim(linePtr);
            multiline = 0;
        }

        // check to see if value extends over multiple lines
        if (tValue[strlen(tValue) - 1] == '\\') {
            multiline = 1;
            tValue[strlen(tValue) - 1] = 0;
            tValue = _trim(tValue);
        }

        // merge value and tValue
        tValueLen = strlen(tValue);
        value = (char *) realloc(value, sizeof(char)*(valueLen + tValueLen + 2));
        ptr = value + valueLen;
        *ptr = 0;
        strncat(value, " ", valueLen + 2);
        strncat(value, tValue, valueLen + tValueLen + 2);
        valueLen += tValueLen + 1;

        // if value is complete, assign
        if (multiline == 0) {
            ptr = _trim(value);

            _assign(config, key, ptr);
            if (value != NULL) {
                free(value);
            }
            if (key != NULL) {
                free(key);
            }
            key = NULL;
            value = NULL;
            valueLen = 0;
        }
    }
    return validate_UdiRootConfig(config, validateFlags);
}

void free_UdiRootConfig(UdiRootConfig *config) {
    if (config->nodeContextPrefix != NULL) {
        free(config->nodeContextPrefix);
        config->nodeContextPrefix = NULL;
    }
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
    if (config->imageBasePath != NULL) {
        free(config->imageBasePath);
        config->imageBasePath = NULL;
    }
    if (config->udiRootPath != NULL) {
        free(config->udiRootPath);
        config->udiRootPath = NULL;
    }
    if (config->udiRootSiteInclude != NULL) {
        free(config->udiRootSiteInclude);
        config->udiRootSiteInclude = NULL;
    }
    if (config->sshPath != NULL) {
        free(config->sshPath);
        config->sshPath = NULL;
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
    if (config->servers != NULL) {
        size_t idx = 0;
        for (idx = 0; idx < config->serversCount; idx++) {
            free(config->servers[idx]->server);
            free(config->servers[idx]);
        }
        free(config->servers);
        config->servers = NULL;
        config->serversCount = 0;
    }
    if (config->siteFs != NULL) {
        size_t idx = 0;
        for (idx = 0; idx < config->siteFsCount; idx++) {
            free(config->siteFs[idx]);
        }
        free(config->siteFs);
        config->siteFs = NULL;
        config->siteFsCount = 0;
    }
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
    fprintf(fp, "Image Gateway Servers = %d servers\n",  config->serversCount);
    for (idx = 0; idx < config->serversCount; idx++) {
        fprintf(fp, "    %s:%d\n", config->servers[idx]->server,
            config->servers[idx]->port);
    }
    fprintf(fp, "Site FS Bind-mounts = %d fs\n", config->siteFsCount);
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

static int _assign(UdiRootConfig *config, char *key, char *value) {
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
