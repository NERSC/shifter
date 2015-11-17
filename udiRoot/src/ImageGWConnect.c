/**
 *  @file ImageGWConnect.c
 *  @brief utility to perform image lookups
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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <munge.h>
#include <curl/curl.h>
#include <json-c/json.h>

#include "ImageData.h"
#include "utility.h"
#include "UdiRootConfig.h"

enum ImageGwAction {
    MODE_LOOKUP = 0,
    MODE_PULL,
    MODE_INVALID
};

struct options {
    int verbose;
    enum ImageGwAction mode;
    char *tag;
};

typedef struct _ImageGwState {
    char *message;
    int isJsonMessage;
    size_t expContentLen;
    size_t messageLen;
    size_t messageCurr;
    int messageComplete;
} ImageGwState;

void _usage(int ret) {
    FILE *output = stdout;
    fprintf(output, "Usage: imageGwConnect [-h|-v] <mode> <tag>\n\n");
    fprintf(output, "    Mode: lookup or pull\n");
    exit(ret);
}

void free_ImageGwState(ImageGwState *image) {
    if (image == NULL) return;
    if (image->message != NULL) free(image->message);
    free(image);
}

size_t handleResponseHeader(char *ptr, size_t sz, size_t nmemb, void *data) {
    ImageGwState *imageGw = (ImageGwState *) data;
    if (imageGw == NULL) {
        return 0;
    }
    if (strncmp(ptr, "HTTP", 4) == 0) {
    }

    char *colon = strchr(ptr, ':');
    char *key = NULL, *value = NULL;
    if (colon != NULL) {
        *colon = 0;
        value = colon + 1;
        key = shifter_trim(ptr);
        value = shifter_trim(colon + 1);
        if (strcasecmp(key, "Content-Type") == 0 && strcmp(value, "application/json") == 0) {
            imageGw->isJsonMessage = 1;
        }
        if (strcasecmp(key, "Content-Length") == 0) {
            imageGw->expContentLen = strtoul(value, NULL, 10);
        }
    }
    return nmemb;
}

size_t handleResponseData(char *ptr, size_t sz, size_t nmemb, void *data) {
    ImageGwState *imageGw = (ImageGwState *) data;
    if (imageGw == NULL || imageGw->messageComplete) {
        return 0;
    }
    if (sz != sizeof(char)) {
        return 0;
    }

    size_t before = imageGw->messageCurr;
    imageGw->message = alloc_strcatf(imageGw->message, &(imageGw->messageCurr), &(imageGw->messageLen), "%s", ptr);
    if (before + nmemb != imageGw->messageCurr) {
        /* error */
        return 0;
    }

    if (imageGw->messageCurr == imageGw->expContentLen) {
        imageGw->messageComplete = 1;
    }
    return nmemb;
}

ImageData *parseLookupResponse(ImageGwState *imageGw) {
    if (imageGw == NULL || !imageGw->isJsonMessage || !imageGw->messageComplete) {
        return NULL;
    }
    json_object *jObj = json_tokener_parse(imageGw->message);
    json_object_iter jIt;
    ImageData *image = NULL;

    if (jObj == NULL) {
        return NULL;
    }
    /*
    {
        "ENTRY": null,
        "ENV": null,
        "groupAcl": [],
        "id": "a5a467fddcb8848a80942d0191134c925fa16ffa9655c540acd34284f4f6375d",
        "itype": "docker",
        "last_pull": 1446245766.1146851,
        "status": "READY",
        "system": "cori",
        "tag": "ubuntu:14.04",
        "userAcl": []
    }
    */

    image = (ImageData *) malloc(sizeof(ImageData));
    memset(image, 0, sizeof(ImageData));
    json_object_object_foreachC(jObj, jIt) {
        enum json_type type = json_object_get_type(jIt.val);
        if (strcmp(jIt.key, "status") == 0 && type == json_type_string) {
            const char *val = json_object_get_string(jIt.val);
            if (val != NULL) {
                image->status = strdup(val);
            }
        } else if (strcmp(jIt.key, "id") == 0 && type == json_type_string) {
            const char *val = json_object_get_string(jIt.val);
            if (val != NULL) {
                image->identifier = strdup(val);
            }
        } else if (strcmp(jIt.key, "tag") == 0 && type == json_type_string) {
            const char *val = json_object_get_string(jIt.val);
            if (val != NULL) {
                image->tag = strdup(val);
            }
        } else if (strcmp(jIt.key, "itype") == 0) {
            const char *val = json_object_get_string(jIt.val);
            if (val != NULL) {
                image->type = strdup(val);
            }
        }
    }

    json_object_put(jObj);  /* apparently this weirdness frees the json object */
    return image;
}

ImageGwState *queryGateway(char *baseUrl, char *tag, struct options *config, UdiRootConfig *udiConfig) {
    const char *modeStr = NULL;
    if (config->mode == MODE_LOOKUP) {
        modeStr = "lookup";
    } else if (config->mode == MODE_PULL) {
        modeStr = "pull";
    } else {
        modeStr = "invalid";
    }
    const char *url = alloc_strgenf("%s/api/%s/%s/docker/%s/", baseUrl, modeStr, udiConfig->system, tag);
    CURL *curl = NULL;
    CURLcode err;
    char *cred = NULL;
    struct curl_slist *headers = NULL;
    char *authstr = NULL;
    size_t authstr_len = 0;
    ImageGwState *imageGw = (ImageGwState *) malloc(sizeof(ImageGwState));
    memset(imageGw, 0, sizeof(ImageGwState));

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);

    munge_ctx_t ctx = munge_ctx_create();
    munge_encode(&cred, ctx, "", 0); 
    authstr = alloc_strgenf("authentication:%s", cred);
    if (authstr == NULL) {
        exit(1);
    }
    free(cred);
    munge_ctx_destroy(ctx);

    headers = curl_slist_append(headers, authstr);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, handleResponseHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, imageGw);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handleResponseData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, imageGw);

    if (config->mode == MODE_PULL) {
        curl_easy_setopt(curl, CURLOPT_POST, 1);
    }

    if (udiConfig->gatewayTimeout > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, udiConfig->gatewayTimeout);
    }

    err = curl_easy_perform(curl);
    if (err) {
        printf("err %d\n", err);
        return NULL;
    }
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code == 200) {
        if (imageGw->messageComplete) {
            if (config->verbose) {
                printf("Message: %s\n", imageGw->message);
            }
            if (config->mode == MODE_LOOKUP) {
                ImageData *image = parseLookupResponse(imageGw);
                if (image != NULL) {
                    printf("%s\n", image->identifier);
                }
                free_ImageData(image, 1);
            } else if (config->mode == MODE_PULL) {
            }
        }
    } else {
        if (config->verbose) {
            printf("Got response: %d\nMessage: %s\n", http_code, imageGw->message);
        }
        free_ImageGwState(imageGw);
        return NULL;
    }

    return imageGw;
}

int parse_options(int argc, char **argv, struct options *config, UdiRootConfig *udiConfig) {
    int opt = 0;
    static struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"verbose", 0, 0, 'v'},
        {0, 0, 0, 0}
    };
    char *ptr = NULL;

    /* ensure that getopt processing stops at first non-option */
    setenv("POSIXLY_CORRECT", "1", 1);

    for ( ; ; ) {
        int longopt_index = 0;
        opt = getopt_long(argc, argv, "hv", long_options, &longopt_index);
        if (opt == -1) break;

        switch (opt) {
            case 'h':
                _usage(0);
                break;
            case 'v':
                config->verbose = 1;
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
        fprintf(stderr, "Must specify action (lookup or pull) and tag\n");
        _usage(1);
    }
    config->mode = MODE_INVALID;
    if (strcmp(argv[optind], "lookup") == 0) {
        config->mode = MODE_LOOKUP;
    } else if (strcmp(argv[optind], "pull") == 0) {
        config->mode = MODE_PULL;
    }
    if (config->mode == MODE_INVALID) {
        fprintf(stderr, "Invalid mode specified\n");
        _usage(1);
    }

    /* can safely do this because we checked earlier that argc had sufficient
       arguments (remaining == 2) */
    CURL *curl = curl_easy_init();
    optind++;
    config->tag = curl_easy_escape(curl, argv[optind], strlen(argv[optind]));
    curl_easy_cleanup(curl);
    return 0;
}

int main(int argc, char **argv) {
    UdiRootConfig udiConfig;
    struct options config;
    ImageGwState *imgGw = NULL;

    memset(&udiConfig, 0, sizeof(UdiRootConfig));
    memset(&config, 0, sizeof(struct options));
    curl_global_init(CURL_GLOBAL_ALL);

    if (parse_UdiRootConfig(CONFIG_FILE, &udiConfig, UDIROOT_VAL_ALL) != 0) {
        fprintf(stderr, "FAILED to parse udiRoot configuration.\n");
        exit(1);
    }

    if (parse_options(argc, argv, &config, &udiConfig) != 0) {
        fprintf(stderr, "FAILED to parse command line options.\n");
        exit(1);
    }

    /* get local copy of gateway urls */
    size_t nGateways = udiConfig.gwUrl_size;
    char **gateways = (char **) malloc(sizeof(char *) * nGateways);
    size_t idx = 0;
    for (idx = 0 ; idx < nGateways; idx++) {
        gateways[idx] = strdup(udiConfig.gwUrl[idx]);
    }

    /* seed our shuffle */
    srand(getpid() ^ time(NULL));

    /* shuffle the list in random order */
    for (idx = 0; idx < nGateways && nGateways - idx - 1 > 0; idx++) {
        size_t r = rand() % (nGateways - idx);
        char *tmp = gateways[idx];
        gateways[idx] = gateways[idx + r];
        gateways[idx + r] = tmp;
    }

    for (idx = 0; idx < nGateways; idx++) {
        imgGw = queryGateway(gateways[idx], config.tag, &config, &udiConfig);
        if (imgGw != NULL) {
            break;
        }
    }

    for (idx = 0; idx < nGateways; idx++) {
        free(gateways[idx]);
    }
    free(gateways);

    curl_global_cleanup();
    if (imgGw == NULL) return 1;
    return 0;
}
