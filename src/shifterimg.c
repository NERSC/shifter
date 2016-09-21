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
#include <termios.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <munge.h>
#include <curl/curl.h>
#include <errno.h>
#include <json-c/json.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "utility.h"
#include "UdiRootConfig.h"
#include "ImageData.h"

enum ImageGwAction {
    MODE_LOOKUP = 0,
    MODE_PULL,
    MODE_IMAGES,
    MODE_LOGIN,
    MODE_PULL_NONBLOCK,
    MODE_EXPIRE,
    MODE_AUTOEXPIRE,
    MODE_INVALID
};

typedef struct _LoginCredential {
    char *system;
    char *location;
    char *cred;
} LoginCredential;

struct options {
    int verbose;
    enum ImageGwAction mode;
    char *type;
    char *tag;
    char *rawtype;
    char *rawtag;
    char *location;
    char *rawlocation;
    LoginCredential **loginCredentials;
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
    fprintf(output, "Usage: shifterimg [-h|-v] <mode> <type:tag>\n\n");
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

static struct termios origTermState;
static int termfd = -1;

void modtermSignalHandler(int signum) {
    if (termfd != -1){ 
        tcsetattr(termfd, TCSANOW, &origTermState);
    }
    exit(128+signum);
}

int getttycred(const char *system, char **username, char **password) {
    FILE *read_fp = NULL;
    FILE *write_fp = stderr;
    char buffer[1024];
    struct termios currState;
    int mod = 0;
    int opened_readfp = 0;

    memset(&origTermState, 0, sizeof(struct termios));
    memset(&currState, 0, sizeof(struct termios));

    read_fp = fopen("/dev/tty", "w+");
    opened_readfp = 1;
    if (read_fp == NULL) {
        read_fp = stdin;
        opened_readfp = 0;
        fprintf(write_fp, "failed to open dev/tty\n");
    }
    termfd = fileno(read_fp);

    fprintf(write_fp, "%s username: ", system);
    fflush(write_fp);
    if (fgets(buffer, 1024, read_fp) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && len < 1024 && buffer[len-1] == '\n') {
            buffer[len-1] = 0;
        }
        *username = strdup(buffer);
        mod++;
    }

    /* get current terminal state */
    tcgetattr(termfd, &origTermState); 
    tcgetattr(termfd, &currState); 


    /* catch common termination/suspend signals to reset term */
    signal(SIGTERM, modtermSignalHandler);
    signal(SIGINT, modtermSignalHandler);
    signal(SIGSTOP, modtermSignalHandler);

    /* disable echoing */
    currState.c_lflag &= ~ECHO;
    tcsetattr(termfd, TCSANOW, &currState);
    

    fprintf(write_fp, "%s password: ", system);
    fflush(write_fp);

    /* read in password */
    if (fgets(buffer, 1024, read_fp) != NULL) {
        size_t pwdlen = strlen(buffer);
        if (pwdlen > 0 && pwdlen < 1024 && buffer[pwdlen-1] == '\n') {
            buffer[pwdlen-1] = 0;
        }
        *password = strdup(buffer);
        mod++;
    }
    fprintf(write_fp, "\n");

    /* reset terminal */
    tcsetattr(termfd, TCSANOW, &origTermState);

    /* reset signals */
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGSTOP, SIG_DFL);

    if (opened_readfp) {
        fclose(read_fp);
        read_fp = NULL;
    }

    return mod == 2 ? 0 : 1;
}

int doLogin(struct options *options, UdiRootConfig *udiConfig) {
    char *username = NULL;
    char *password = NULL;
    char *cred = NULL;
    char *munge_cred = NULL;

    if (options->location == NULL) {
        options->location = strdup("default");
    }

    int ret = getttycred(options->location, &username, &password);
    if (ret != 0) {
        fprintf(stderr, "FAILED to read credentials from tty!\n");
    }
    cred = alloc_strgenf("%s:%s", username, password);

    munge_ctx_t ctx = munge_ctx_create();
    munge_encode(&munge_cred, ctx, cred, strlen(cred)); 
    munge_ctx_destroy(ctx);

    free(cred);
    free(username);
    free(password);
    cred = username = password = NULL;

    /* figure out where to insert credentials */
    LoginCredential **lcptr = options->loginCredentials;
    for ( ; lcptr && *lcptr; lcptr++) {
        if ((*lcptr)->system && (*lcptr)->location && 
                strcmp((*lcptr)->system, udiConfig->system) == 0 &&
                strcmp((*lcptr)->location, options->location) == 0) {

            break;
        }
    }

    /* if they were found, replace existing */
    if (lcptr && *lcptr) {
        if ((*lcptr)->cred != NULL) {
            free((*lcptr)->cred);
        }
        (*lcptr)->cred = munge_cred;
    } else {
        /* append to end of list */
        size_t count = 0;
        if (lcptr) count = lcptr - options->loginCredentials;
        LoginCredential **tmp = (LoginCredential **) realloc(options->loginCredentials, sizeof(LoginCredential *) * (count+2));
        if (tmp == NULL) {
            fprintf(stderr, "FAILED to allocate memory to expand credential list\n");
            goto _error;
        }
        options->loginCredentials = tmp;
        lcptr = options->loginCredentials + count;
        *lcptr = (LoginCredential *) malloc(sizeof(LoginCredential));
        (*lcptr)->system = strdup(udiConfig->system);
        (*lcptr)->location = strdup(options->location);
        (*lcptr)->cred = munge_cred;

        /* NULL-terminate the list */
        lcptr++;
        *lcptr = NULL;
    }

    /* write out credentials */
    struct passwd *pwd = getpwuid(getuid());
    char *creddir = NULL;
    char *path = NULL;
    if (pwd != NULL) {
        FILE *out = NULL;
        creddir = alloc_strgenf("%s/.udiRoot", pwd->pw_dir);
        path = alloc_strgenf("%s/.udiRoot/.cred", pwd->pw_dir);
        if (creddir == NULL || path == NULL) {
            fprintf(stderr, "FAILED to allocate memory for paths\n");
            goto _error;
        }
        if (mkdir(creddir, 0700) != 0) {
            if (errno != EEXIST) {
                fprintf(stderr, "FAILED to create directory %s: %s\n", creddir, strerror(errno));
                goto _error;
            }
        }
        out = fopen(path, "w");
        if (out == NULL) {
            fprintf(stderr, "FAILED to open credentials for writing!\n");
            goto _error;
        }
        lcptr = options->loginCredentials;
        for ( ; lcptr && *lcptr; lcptr++) {
            fprintf(out, "%s:%s=%s\n", (*lcptr)->system, (*lcptr)->location, (*lcptr)->cred);
        }
        fclose(out);
        out = NULL;

        if (chmod(path, 0600) != 0) {
            fprintf(stderr, "FAILED to correctly set permissions on %s\n", path);
            goto _error;
        }
    }
    if (path != NULL) {
        free(path);
        path = NULL;
    }
    if (creddir != NULL) {
        free(creddir);
        creddir = NULL;
    }
    return 0;
_error:
    if (path != NULL) {
        free(path);
        path = NULL;
    }
    if (creddir != NULL) {
        free(creddir);
        creddir = NULL;
    }
    return 1;
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
            char **tmp = (char **) realloc(ret, sizeof(char *) * (count + 11));
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
        strVal = NULL;
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
    ImageGwImageRec *image = NULL;

    if (jObj == NULL) {
        return NULL;
    }
    image = parseImageJson(jObj);

    json_object_put(jObj);  /* apparently this weirdness frees the json object */
    return image;
}

ImageGwImageRec *parsePullResponse(ImageGwState *imageGw) {
    if (imageGw == NULL || !imageGw->isJsonMessage || !imageGw->messageComplete) {
        return NULL;
    }
    json_object *jObj = json_tokener_parse(imageGw->message);
    ImageGwImageRec *image = NULL;

    if (jObj == NULL) {
        return NULL;
    }
    image = parseImageJson(jObj);

    json_object_put(jObj);  /* apparently this weirdness frees the json object */
    return image;
}

int imgCompare(const void *ta, const void *tb) {
    const ImageGwImageRec *a = (const ImageGwImageRec *) ta;
    const ImageGwImageRec *b = (const ImageGwImageRec *) tb;
    if (!a && !b) return 0;
    if (!a && b) return 1;
    if (a && !b) return -1;
    if (!(a->tag) && !(b->tag)) return 0;
    if (!(a->tag) && (b->tag)) return 1;
    if ((a->tag) && !(b->tag)) return -1;
    if (!(a->tag[0]) && !(b->tag[0])) return 0;
    if (!(a->tag[0]) && (b->tag[0])) return 1;
    if ((a->tag[0]) && !(b->tag[0])) return -1;
    return strcmp(a->tag[0], b->tag[0]);
}

char *json_escape_string(const char *input) {
    char *output = NULL;
    const char *rptr = NULL;
    char *wptr = NULL;
    if (input == NULL || strlen(input) == 0) {
        return NULL;
    }

    /* worst case is everything is escaped, so double input len */
    output = (char *) malloc(sizeof(char) * (strlen(input) * 2 + 1));
    if (output == NULL) return NULL;
    for (rptr = input, wptr = output; rptr && *rptr; rptr++, wptr++) {
        switch (*rptr) {
            case '"':
            case '\\':
                *wptr = '\\';
                wptr++;
                *wptr = *rptr;
                break;
            case '\n':
                *wptr = '\\';
                wptr++;
                *wptr = 'n';
                break;
            case '\b':
                *wptr = '\\';
                wptr++;
                *wptr = 'b';
                break;
            case '\f':
                *wptr = '\\';
                wptr++;
                *wptr = 'f';
                break;
            case '\r':
                *wptr = '\\';
                wptr++;
                *wptr = 'r';
                break;
            case '\t':
                *wptr = '\\';
                wptr++;
                *wptr = 't';
                break;
            default:
                *wptr = *rptr;
                break;
        }
    }
    *wptr = '\0';
    return output;
}

char *constructAuthMessage(struct options *config, UdiRootConfig *udiConfig) {
    munge_ctx_t ctx = munge_ctx_create();
    char *msg = NULL;
    size_t msg_curr = 0;
    size_t msg_len = 0;
    char *buffer = NULL;
    char *json_location = NULL;
    char *json_credential = NULL;
    int cnt = 0;

    msg = alloc_strcatf(msg, &msg_curr, &msg_len, "{\"authorized_locations\":{");

    LoginCredential **lcptr = config->loginCredentials;
    for ( ; lcptr && *lcptr; lcptr++) {
        LoginCredential *lc = *lcptr;
        if (lc->system && strcmp(udiConfig->system, lc->system) == 0) {
            uid_t uid = 0;
            munge_err_t ret;
            int len = 0;
            ret = munge_decode(lc->cred, ctx, (void **) &buffer, &len, &uid, NULL);
            if (ret != EMUNGE_SUCCESS && ret != EMUNGE_CRED_EXPIRED && ret != EMUNGE_CRED_REPLAYED) {
                /* allowed to read or re-read these but that's it */
                if (buffer != NULL) {
                    free(buffer);
                    buffer = NULL;
                }
                continue;
            }
            if (uid != getuid()) {
                if (buffer != NULL) {
                    free(buffer);
                    buffer = NULL;
                }
                continue;
            }
            json_location = json_escape_string(lc->location);
            json_credential = json_escape_string(buffer);
            if (json_location && json_credential) {
                msg = alloc_strcatf(msg, &msg_curr, &msg_len, "%s\"%s\":\"%s\"",
                        cnt++ > 0 ? "," : "", json_location, json_credential);
            }
            if (json_location) {
                free(json_location);
                json_location = NULL;
            }
            if (json_credential) {
                free(json_credential);
                json_credential = NULL;
            }
            if (buffer != NULL) {
                free(buffer);
                buffer = NULL;
            }
        }
    }
    if (cnt > 0) {
        msg = alloc_strcatf(msg, &msg_curr, &msg_len, "}}");
        return msg;
    }
    free(msg);
    return NULL;
}

ImageGwState *queryGateway(char *baseUrl, char *type, char *tag, struct options *config, UdiRootConfig *udiConfig) {
    const char *modeStr = NULL;
    if (config->mode == MODE_LOOKUP) {
        modeStr = "lookup";
    } else if (config->mode == MODE_PULL || config->mode == MODE_PULL_NONBLOCK) {
        modeStr = "pull";
    } else if (config->mode == MODE_IMAGES) {
        modeStr = "list";
    } else if (config->mode == MODE_EXPIRE) {
        modeStr = "expire";
    } else if (config->mode == MODE_AUTOEXPIRE) {
        modeStr = "autoexpire";
    } else {
        modeStr = "invalid";
    }
    const char *url = NULL;
    if (tag != NULL) {
        url = alloc_strgenf("%s/api/%s/%s/%s/%s/", baseUrl, modeStr, udiConfig->system, type, tag);
    } else {
        url = alloc_strgenf("%s/api/%s/%s/", baseUrl, modeStr, udiConfig->system);
    }
    CURL *curl = NULL;
    CURLcode err;
    char *cred = NULL;
    struct curl_slist *headers = NULL;
    char *authstr = NULL;
    ImageGwState *imageGw = (ImageGwState *) malloc(sizeof(ImageGwState));
    memset(imageGw, 0, sizeof(ImageGwState));

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);

    munge_ctx_t ctx = munge_ctx_create();

    char *cred_message = constructAuthMessage(config, udiConfig);
    if (cred_message == NULL) {
        munge_encode(&cred, ctx, "", 0); 
    } else {
        size_t len = strlen(cred_message);
        munge_encode(&cred, ctx, cred_message, len);
        memset(cred_message, 0, sizeof(char)*len);
        free(cred_message);
        cred_message = NULL;
    }

    authstr = alloc_strgenf("authentication:%s", cred);
    if (authstr == NULL) {
        exit(1);
    }
    free(cred);
    cred = NULL;
    munge_ctx_destroy(ctx);

    headers = curl_slist_append(headers, authstr);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, handleResponseHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, imageGw);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handleResponseData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, imageGw);

    if (config->mode == MODE_PULL || config->mode == MODE_PULL_NONBLOCK) {
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    }

    if (udiConfig->gatewayTimeout > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, udiConfig->gatewayTimeout);
    }

    err = curl_easy_perform(curl);
    if (err) {
        if (err == 7) { // 7 means Failed to connect to host.
          printf("ERROR: failed to contact the image gateway.\n");
        } else {
          printf("err %d\n", err);
        }
        goto _fail_valid_args;
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
            } else if (config->mode == MODE_PULL_NONBLOCK) {
                time_t curr_timet = time(NULL);
                struct tm *curr = localtime(&curr_timet);
                char timebuf[128];
                strftime(timebuf, 128, "%Y-%m-%dT%H:%M:%S", curr);
                ImageGwImageRec *image = parsePullResponse(imageGw);
                if (image != NULL) {

                    printf("%s%s Pulling Image: %s:%s, status: %s%s",
                            config->verbose ? "" : "\r\x1b[2K",
                            timebuf, config->rawtype, config->rawtag, image->status,
                            config->verbose ? "\n" : "");
                    fflush(stdout);
                    if (strcmp(image->status, "MISSING") == 0 ||
                        strcmp(image->status, "INIT") == 0 ||
                        strcmp(image->status, "PENDING") == 0 ||
                        strcmp(image->status, "PULLING") == 0 ||
                        strcmp(image->status, "EXAMINATION") == 0 ||
                        strcmp(image->status, "CONVERSION") == 0 ||
                        strcmp(image->status, "TRANSFER") == 0) {
                        free_ImageGwImageRec(image, 1);
                        return imageGw;
                    } else {
                        printf("\n");
                        free_ImageGwImageRec(image, 1);
                        free_ImageGwState(imageGw);
                        return NULL;
                    }
                } else {
                    free_ImageGwState(imageGw);
                    return NULL;
                }
            } else if (config->mode == MODE_PULL) {
                ImageGwImageRec *image = parsePullResponse(imageGw);
                if (image != NULL) {
                    for ( ; ; ) {
                        usleep(500000);
                        config->mode = MODE_PULL_NONBLOCK;
                        /* query again */
                        ImageGwState *gwState = queryGateway(baseUrl, type, tag, config, udiConfig);
                        if (gwState == NULL) break;
                        free_ImageGwState(gwState);
                        config->mode = MODE_PULL;
                    }
                    free_ImageGwImageRec(image, 1);
                    image = NULL;
                }
            } else if (config->mode == MODE_IMAGES) {
                ImageGwImageRec **images = parseImagesResponse(imageGw);
                if (images != NULL && *images != NULL) {
                    size_t count = 0;
                    size_t lidx = 0;
                    ImageGwImageRec **ptr = NULL;
                    for (ptr = images; ptr != NULL && *ptr != NULL; ptr++) {
                        ImageGwImageRec *image = *ptr;
                        char **tagPtr = image->tag;
                        while (tagPtr && *tagPtr) {
                            count++;
                            tagPtr++;
                        }
                    }
                    if (count == 0) {
                        if (images != NULL) {
                            for (ptr = images; ptr && *ptr; ptr++) {
                                free_ImageGwImageRec(*ptr, 1);
                            }
                            free(images);
                            images = NULL;
                        }
                        goto _fail_valid_args;
                    }
                    ImageGwImageRec *limages = (ImageGwImageRec *) malloc(sizeof(ImageGwImageRec) * count);
                    for (ptr = images; ptr != NULL && *ptr != NULL; ptr++) {
                        ImageGwImageRec *image = *ptr;
                        char **tagPtr = image->tag;
                        while (tagPtr && *tagPtr) {
                            memcpy(&(limages[lidx]), image, sizeof(ImageGwImageRec));
                            limages[lidx].tag = (char **) malloc(sizeof(char *) * 1);
                            limages[lidx].tag[0] = *tagPtr;
                            lidx++;
                            tagPtr++;
                        }
                    }
                    qsort(limages, count, sizeof(ImageGwImageRec), imgCompare);
                    for (lidx = 0; lidx < count; lidx++) {
                        ImageGwImageRec *image = &(limages[lidx]);
                        char *tag = image->tag[0];
                        time_t pull_time = image->last_pull;
                        struct tm time_struct;
                        char time_str[100];
                        memset(&time_struct, 0, sizeof(struct tm));
                        if (localtime_r(&pull_time, &time_struct) == NULL) {
                            /* if above generated an error, re-zero so we display obvious nonsense */
                            memset(&time_struct, 0, sizeof(struct tm));
                        }
                        strftime(time_str, 100, "%Y-%m-%dT%H:%M:%S", &time_struct);

                        printf("%-10s %-10s %-8s %-.10s   %s %-30s\n", image->system, image->type, image->status, image->identifier, time_str, tag);
                    }
                    free(limages);
                }
                if (images != NULL) {
                    ImageGwImageRec **ptr = NULL;
                    for (ptr = images; ptr && *ptr; ptr++) {
                        free_ImageGwImageRec(*ptr, 1);
                    }
                    free(images);
                    images = NULL;
                }
            }
        }
    } else {
        if (config->verbose) {
            printf("Got response: %ld\nMessage: %s\n", http_code, imageGw->message);
        }
        free_ImageGwState(imageGw);
        return NULL;
    }

    return imageGw;
_fail_valid_args:
    if (imageGw != NULL) {
        free_ImageGwState(imageGw);
        imageGw = NULL;
    }
    return NULL;
}

int _assignLoginCredential(const char *key, const char *value, void *_data) {
    const char *ptr = strchr(key, ':');
    char *system = NULL;
    char *location = NULL;
    struct options *config = (struct options *) _data;

    if (ptr != NULL) {
        size_t count = 0;
        LoginCredential **lcptr = config->loginCredentials;
        for ( ; lcptr && *lcptr; lcptr++) {
            count++;
        }
        lcptr = (LoginCredential **) realloc(config->loginCredentials, sizeof(LoginCredential *) * (count + 2));
        if (lcptr == NULL) {
            goto _error;
        }
        system = (char *) malloc(sizeof(char)*((ptr - key) + 1));
        strncpy(system, key, (ptr - key));
        system[ptr - key] = 0;
        ptr++;
        location = strdup(ptr);

        config->loginCredentials = lcptr;
        lcptr = config->loginCredentials + count;
        *lcptr = (LoginCredential *) malloc(sizeof(LoginCredential));
        (*lcptr)->system = system;
        (*lcptr)->location = location;
        (*lcptr)->cred = strdup(value);
        lcptr++;
        *lcptr = NULL;
    }
    return 0;
_error:
    return 1;
}

int parse_options(int argc, char **argv, struct options *config, UdiRootConfig *udiConfig) {
    int opt = 0;
    static struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"verbose", 0, 0, 'v'},
        {0, 0, 0, 0}
    };

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
    } else if (strcmp(argv[optind], "pullnb") == 0) {
        config->mode = MODE_PULL_NONBLOCK;
    } else if (strcmp(argv[optind], "images") == 0) {
        config->mode = MODE_IMAGES;
    } else if (strcmp(argv[optind], "login") == 0) {
        config->mode = MODE_LOGIN;
    } else if (strcmp(argv[optind], "expire") == 0) {
        config->mode = MODE_EXPIRE;
    } else if (strcmp(argv[optind], "autoexpire") == 0) {
        config->mode = MODE_AUTOEXPIRE;
    }
    if (config->mode == MODE_INVALID) {
        fprintf(stderr, "Invalid mode specified\n");
        _usage(1);
    }

    if (remaining > 1) {
        CURL *curl = curl_easy_init();
        optind++;

        char *type = NULL;
        char *tag = NULL;

        if (config->mode == MODE_LOGIN) {
            config->location = strdup(argv[optind]);
        } else {
            if (parse_ImageDescriptor(argv[optind], &type, &tag, udiConfig) != 0) {
                fprintf(stderr, "FAILED to parse image descriptor. Try specifying "
                        "both the type and descriptor, e.g., docker:ubuntu:latest"
                        "\n");
                _usage(1);
            }

            config->type = curl_easy_escape(curl, type, strlen(type));
            config->rawtype = type; /* TODO: does this need to be strdup'd? */
            config->tag = curl_easy_escape(curl, tag, strlen(tag));
            config->rawtag = strdup(tag);
        }

        curl_easy_cleanup(curl);
    }
    if (config->location == NULL) {
        config->location = strdup("default");
    }

    /* read any credentials that exist */
    struct passwd *pwd = getpwuid(getuid());
    if (pwd != NULL) {
        char *path = alloc_strgenf("%s/.udiRoot/.cred", pwd->pw_dir);
        if (path != NULL && access(path, F_OK) == 0) {
            shifter_parseConfig(path, '=', config, _assignLoginCredential);
        } 
        if (path != NULL) {
            free(path);
            path = NULL;
        }
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
        return doLogin(&config, &udiConfig);
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
