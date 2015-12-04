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
#include <time.h>
#include <munge.h>
#include <curl/curl.h>
#include <json-c/json.h>

#include "utility.h"
#include "UdiRootConfig.h"

enum ImageGwAction {
    MODE_LOOKUP = 0,
    MODE_PULL,
    MODE_IMAGES,
    MODE_LOGIN,
    MODE_INVALID
};

struct options {
    int verbose;
    enum ImageGwAction mode;
    char *type;
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

typedef struct _ImageGwImageRec {
    char *entryPoint;
    char **env;
    char *workdir;
    char **groupAcl;
    char *identifier;
    char *type;
    char *status;
    double last_pull;
    char *system;
    char **tag;
    char **userAcl;
} ImageGwImageRec;


void _usage(int ret) {
    FILE *output = stdout;
    fprintf(output, "Usage: imageGwConnect [-h|-v] <mode> <type:tag>\n\n");
    fprintf(output, "    Mode: images, lookup, or pull\n");
    exit(ret);
}

void free_ImageGwState(ImageGwState *image) {
    if (image == NULL) return;
    if (image->message != NULL) free(image->message);
    free(image);
}

void free_ImageGwImageRec(ImageGwImageRec *ptr, int free_struct) {
    if (ptr == NULL) return;
    char *strings[] = {
        ptr->entryPoint,
        ptr->workdir,
        ptr->identifier,
        ptr->type,
        ptr->status,
        ptr->system,
        NULL
    };
    char **stringArrays[] = {
        ptr->env,
        ptr->tag,
        ptr->groupAcl,
        ptr->userAcl,
        NULL
    };

    char **strPtr = strings;
    while (strPtr && *strPtr) {
        free(*strPtr);
        strPtr++;
    }
    char ***strArrPtr = stringArrays;
    while (strArrPtr && *strArrPtr) {
        strPtr = *strArrPtr;
        while (strPtr && *strPtr) {
            free(*strPtr);
            strPtr++;
        }
        free(*strArrPtr);
        strArrPtr++;
    }

    memset(ptr, 0, sizeof(ImageGwImageRec));
    if (free_struct) {
        free(ptr);
    }
}


int doLogin() {
    return 0;
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

int jsonParseString(json_object *json_data, char **value) {
    if (value == NULL || json_data == NULL) {
        return 1;
    }

    enum json_type type = json_object_get_type(json_data);
    if (type != json_type_string) {
        *value = NULL;
        return 1;
    }

    *value = strdup(json_object_get_string(json_data));
    return 0;
}

int jsonParseDouble(json_object *json_data, double *value) {
    if (value == NULL || json_data == NULL) {
        return 1;
    }
    enum json_type type = json_object_get_type(json_data);
    if (type != json_type_double) {
        *value = 0;
        return 1;
    }
    *value = json_object_get_double(json_data);

    return 0;
}

int jsonParseStringArray(json_object *json_data, char ***values) {
    if (json_data == NULL || values == NULL) {
        return 1;
    }

    enum json_type type = json_object_get_type(json_data);
    if (type != json_type_array) {
        *values = NULL;
        return 1;
    }

    int len = json_object_array_length(json_data);
    int idx = 0;
    size_t capacity = 0;
    char **ret = NULL;
    char **ptr = ret;
    for (idx = 0; idx < len; idx++) {
        json_object *obj = json_object_array_get_idx(json_data, idx);
        if (obj == NULL) {
            continue;
        }
        char *value = NULL;
        if (jsonParseString(obj, &value) != 0) {
            continue;
        }
        if (value == NULL) {
            continue;
        }

        size_t count = ptr - ret;
        if (count >= capacity) {
            char **tmp = (char **) realloc(ret, sizeof(char **) * (count + 11));
            if (tmp == NULL) {
                break;
            }
            ret = tmp;
            ptr = ret + count;
            capacity = count + 10;
        }
        *ptr++ = value;
        *ptr = NULL;
    }
    *values = ret;
    return 0;
}

ImageGwImageRec *parseImageJson(json_object *json_data) {
    if (json_data == NULL) {
        return NULL;
    }

    ImageGwImageRec *image = (ImageGwImageRec *) malloc(sizeof(ImageGwImageRec));
    json_object_iter jIt;
    memset(image, 0, sizeof(ImageGwImageRec));

    char *strVal = NULL;
    char **arrVal = NULL;
    double dblVal = 0.0;

    json_object_object_foreachC(json_data, jIt) {
        enum json_type type = json_object_get_type(jIt.val);
        int ok = 0;
        if (type == json_type_string) {
            ok = jsonParseString(jIt.val, &strVal);
        } else if (type == json_type_array) {
            ok = jsonParseStringArray(jIt.val, &arrVal);
        } else if (type == json_type_double) {
            ok = jsonParseDouble(jIt.val, &dblVal);
        }
        if (ok == 0) {
            if (strcasecmp(jIt.key, "entry") == 0 && strVal != NULL) {
                image->entryPoint = strVal;
                strVal = NULL;
            } else if (strcasecmp(jIt.key, "env") == 0 && arrVal != NULL) {
                image->env = arrVal;
                arrVal = NULL;
            } else if (strcasecmp(jIt.key, "workdir") == 0 && strVal != NULL) {
                image->workdir = strVal;
                strVal = NULL;
            } else if (strcasecmp(jIt.key, "groupAcl") == 0 && arrVal != NULL) {
                image->groupAcl = arrVal;
                arrVal = NULL;
            } else if (strcasecmp(jIt.key, "id") == 0 && strVal != NULL) {
                image->identifier = strVal;
                strVal = NULL;
            } else if (strcasecmp(jIt.key, "itype") == 0 && strVal != NULL) {
                image->type = strVal;
                strVal = NULL;
            } else if (strcasecmp(jIt.key, "last_pull") == 0 && dblVal != 0) {
                image->last_pull = dblVal;
            } else if (strcasecmp(jIt.key, "status") == 0 && strVal != NULL) {
                image->status = strVal;
                strVal = NULL;
            } else if (strcasecmp(jIt.key, "system") == 0 && strVal != NULL) {
                image->system = strVal;
                strVal = NULL;
            } else if (strcasecmp(jIt.key, "tag") == 0 && arrVal != NULL) {
                image->tag = arrVal;
                arrVal = NULL;
            } else if (strcasecmp(jIt.key, "tag") == 0 && strVal != NULL) {
                image->tag = (char **) malloc(sizeof(char *) * 2);
                image->tag[0] = strVal;
                image->tag[1] = NULL;
                strVal = NULL;
            } else if (strcasecmp(jIt.key, "userAcl") == 0 && arrVal != NULL) {
                image->userAcl = arrVal;
                arrVal = NULL;
            }
        }

        strVal = NULL;
        if (strVal != NULL) {
            free(strVal);
            strVal = NULL;
        }
        dblVal = 0.0;
        if (arrVal != NULL) {
            char **ptr = arrVal;
            while (ptr && *ptr) {
                free(*ptr);
                ptr++;
            }
            free(arrVal);
            arrVal = NULL;
        }
    }
    return image;
}

ImageGwImageRec **parseImagesResponse(ImageGwState *imageGw) {
    if (imageGw == NULL || !imageGw->isJsonMessage || !imageGw->messageComplete) {
        return NULL;
    }
    json_object *jObj = json_tokener_parse(imageGw->message);
    json_object_iter jIt;
    ImageGwImageRec **images = NULL;
    size_t images_count = 0;
    size_t images_capacity = 0;

    json_object_object_foreachC(jObj, jIt) {
        if (strcmp(jIt.key, "list") == 0 || strcmp(jIt.key, "data") == 0) {
            enum json_type type = json_object_get_type(jIt.val);
            if (type != json_type_array) {
                /* ERROR */
                continue;
            }
            json_object_iter jArrIt;
            int len = json_object_array_length(jIt.val);
            int idx = 0;
            for (idx = 0; idx < len; idx++) {
                json_object *val = json_object_array_get_idx(jIt.val, idx);

                ImageGwImageRec *image = parseImageJson(val);
                if (image != NULL) {
                    while (images_count >= images_capacity) {
                        ImageGwImageRec **tmp = (ImageGwImageRec **) realloc(images, sizeof(ImageGwImageRec *) * (images_capacity + 11));
                        if (tmp != NULL) {
                            images = tmp;
                            images_capacity += 10;
                        } else {
                            /* ERROR */
                        }
                    }
                    images_count++;
                    images[images_count - 1] = image;
                    images[images_count] = NULL;
                }
            }
        }
    }
    json_object_put(jObj);  /* apparently this weirdness frees the json object */
    return images;
}

ImageGwImageRec *parseLookupResponse(ImageGwState *imageGw) {
    if (imageGw == NULL || !imageGw->isJsonMessage || !imageGw->messageComplete) {
        return NULL;
    }
    json_object *jObj = json_tokener_parse(imageGw->message);
    json_object_iter jIt;
    ImageGwImageRec *image = NULL;

    if (jObj == NULL) {
        return NULL;
    }
    image = parseImageJson(jObj);

    json_object_put(jObj);  /* apparently this weirdness frees the json object */
    return image;
}

ImageGwState *queryGateway(char *baseUrl, char *type, char *tag, struct options *config, UdiRootConfig *udiConfig) {
    const char *modeStr = NULL;
    if (config->mode == MODE_LOOKUP) {
        modeStr = "lookup";
    } else if (config->mode == MODE_PULL) {
        modeStr = "pull";
    } else if (config->mode == MODE_IMAGES) {
        modeStr = "list";
    } else {
        modeStr = "invalid";
    }
    const char *url = NULL;
    if (tag != NULL) {
        url = alloc_strgenf("%s/api/%s/%s/%s/%s/", baseUrl, modeStr, udiConfig->system, type, tag);
        free(type);
        free(tag);
    } else {
        url = alloc_strgenf("%s/api/%s/%s/", baseUrl, modeStr, udiConfig->system);
    }
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
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
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
                ImageGwImageRec *image = parseLookupResponse(imageGw);
                if (image != NULL) {
                    printf("%s\n", image->identifier);
                }
                free_ImageGwImageRec(image, 1);
            } else if (config->mode == MODE_PULL) {
            } else if (config->mode == MODE_IMAGES) {
                ImageGwImageRec **images = parseImagesResponse(imageGw);
                if (images != NULL && *images != NULL) {
                    ImageGwImageRec **ptr = images;
                    for (ptr = images; ptr != NULL && *ptr != NULL; ptr++) {
                        ImageGwImageRec *image = *ptr;
                        char **tagPtr = image->tag;
                        while (tagPtr && *tagPtr) {
                            time_t pull_time = image->last_pull;
                            struct tm time_struct;
                            char time_str[100];
                            memset(&time_struct, 0, sizeof(struct tm));
                            if (localtime_r(&pull_time, &time_struct) == NULL) {
                                /* if above generated an error, re-zero so we display obvious nonsense */
                                memset(&time_struct, 0, sizeof(struct tm));
                            }
                            strftime(time_str, 100, "%Y-%m-%dT%H:%M:%S", &time_struct);

                            printf("%-10s %-10s %-8s %-.10s   %s %-30s\n", image->system, image->type, image->status, image->identifier, time_str, *tagPtr);
                            tagPtr++;
                        }
                        free_ImageGwImageRec(image, 1);
                    }
                }
                if (images != NULL) {
                    free(images);
                }
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
    if (remaining == 0) {
        fprintf(stderr, "Must specify mode (images, lookup, pull)!\n");
        _usage(1);
    }
    config->mode = MODE_INVALID;
    if (strcmp(argv[optind], "lookup") == 0) {
        config->mode = MODE_LOOKUP;
    } else if (strcmp(argv[optind], "pull") == 0) {
        config->mode = MODE_PULL;
    } else if (strcmp(argv[optind], "images") == 0) {
        config->mode = MODE_IMAGES;
    } else if (strcmp(argv[optind], "login") == 0) {
        config->mode = MODE_LOGIN;
    }
    if (config->mode == MODE_INVALID) {
        fprintf(stderr, "Invalid mode specified\n");
        _usage(1);
    }

    if (remaining > 1) {
        CURL *curl = curl_easy_init();
        optind++;

        char *type = argv[optind];
        char *ptr = strchr(type, ':');
        if (ptr == NULL) {
            fprintf(stderr, "Must specify imageType:imageTag..., e.g., docker:ubuntu:latest\n");
            _usage(1);
        }

        *ptr++ = 0;
        config->type = curl_easy_escape(curl, type, strlen(type));
        config->tag = curl_easy_escape(curl, ptr, strlen(ptr));
        ptr--;
        *ptr = ':';

        curl_easy_cleanup(curl);
    }
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

    if (config.mode == MODE_LOGIN) {
        return doLogin();
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
        imgGw = queryGateway(gateways[idx], config.type, config.tag, &config, &udiConfig);
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
