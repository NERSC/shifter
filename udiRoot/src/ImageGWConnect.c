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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <munge.h>
#include <curl/curl.h>
#include <json-c/json.h>

#include "ImageData.h"
#include "utility.h"
#include "UdiRootConfig.h"

typedef struct _ImageGwState {
    char *message;
    int isJsonMessage;
    size_t expContentLen;
    size_t messageLen;
    size_t messageCurr;
    int messageComplete;
} ImageGwState;

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

ImageGwState *queryGateway(char *baseUrl, char *tag, UdiRootConfig *config) {
    const char *url = alloc_strgenf("%s/api/lookup/%s/docker/%s/", baseUrl, config->system, tag);
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

    if (config->gatewayTimeout > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, config->gatewayTimeout);
    }

    err = curl_easy_perform(curl);
    if (err) {
        exit(1);
    }
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code == 200) {
        if (imageGw->messageComplete) {
            ImageData *image = parseLookupResponse(imageGw);
            if (image != NULL) {
                printf("%s\n", image->identifier);
            }
            free_ImageData(image, 1);
        }
    } else {
        free_ImageGwState(imageGw);
        return NULL;
    }

    return imageGw;
}

int main(int argc, char **argv) {
    UdiRootConfig udiConfig;
    ImageGwState *imgGw = NULL;
    char *tag = NULL;
    CURL *curl = curl_easy_init();

    memset(&udiConfig, 0, sizeof(UdiRootConfig));
    curl_global_init(CURL_GLOBAL_ALL);

    if (parse_UdiRootConfig(CONFIG_FILE, &udiConfig, UDIROOT_VAL_ALL) != 0) {
        fprintf(stderr, "FAILED to parse udiRoot configuration.\n");
        exit(1);
    }

    if (argc < 2) {
        fprintf(stderr, "No tag supplied.\n");
        exit(1);
    }
    tag = curl_easy_escape(curl, argv[1], strlen(argv[1]));

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
        imgGw = queryGateway(gateways[idx], tag, &udiConfig);
        if (imgGw != NULL) {
            break;
        }
    }

    for (idx = 0; idx < nGateways; idx++) {
        free(gateways[idx]);
    }
    free(gateways);

    free(tag);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return imgGw != NULL;
}
