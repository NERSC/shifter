#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "UdiRootConfig.h"

static char *_trim(char *);
static int _validateConfigFile();
static int _assign(UdiRootConfig *config, const char *key, const char *value);

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
            key = _trim(linePtr);
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
        value = (char *) realloc(&value, sizeof(char)*(valueLen + tValueLen + 2));
        ptr = value + valueLen;
        *ptr = 0;
        strncat(value, " ", valueLen + 2);
        strncat(value, tValue, valueLen + tValueLen + 2);

        // if value is complete, assign
        if (multiline == 0) {
            ptr = _trim(value);

            _assign(config, key, ptr);
            free(value);
        }
    }
}

static char *_trim(char *str) {
    char *ptr = str;
    ssize_t len = 0;
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

static int _assign(UdiRootConfig *config, const char *key, const char *value) {
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
        config->kmodBasePath = strdup(value);
        if (config->kmodBasePath == NULL) return 1;
    } else if (strcmp(key, "kmodCacheFile") == 0) {
        config->kmodCacheFile = strdup(value);
        if (config->kmodCacheFile == NULL) return 1;
    } else if (strcmp(key, "siteFs") == 0) {
        char *search = value;
        while ((ptr = strtok(search, " ")) != NULL) {
            search = NULL;
            config->siteFs = (char **) realloc(config->siteFs, sizeof(char*) * (config->siteFsCount+1));
            if (config->siteFs == NULL) return 1;
            config->siteFs[config->siteFsCount] = strdup(ptr);
            config->siteFsCount++;
        }
    } else if (strcmp(key, "imageGateway") == 0) {
        char *search = value;
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
            config->servers = (ImageGwServer **) realloc(config->servers, sizeof(ImageGwServer*) * (config->serverCount+1));
            if (config->servers == NULL) return 1;
            config->servers[config->serverCount] = newServer;
            config->serverCount++;
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
        return 2;
    }
    return 0; 
}
