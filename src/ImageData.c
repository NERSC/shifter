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
#include <limits.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "ImageData.h"
#include "UdiRootConfig.h"
#include "utility.h"
#include "shifter_mem.h"

#define ENV_ALLOC_SIZE 512
#define VOL_ALLOC_SIZE 256

static const char *allowedImageTypes[] = {
    "docker",
    "custom",
    "id",
    "local",
    NULL
};

size_t _convert_to_list(const char *text, uid_t **uids, size_t *n_uids);
int _ImageData_assign(const char *key, const char *value, void *t_imageData);
char *_ImageData_filterString(const char *input, int allowSlash);

/*! Contact image gateway to lookup mapping between tag/type and identifier */
/*!
 * Contact the image gateway to lookup the detailed image identifier for the
 * requested image tag.  It is legal to lookup an identifier mislabeled as a
 * tag.  This provides a deterministic and trusted path for always getting a
 * valid identifier.  If imageType is "id", then imageTag is assumed to already
 * be an identifier and no lookup will occur, a copy of imageTag will be
 * returned.
 *
 * \param imageType the image type, any string the gateway might understand,
 *      special values include "local" and "id".  If either of the special
 *      values, then a copy of the imageTag is returned.  Any other value will
 *      cause a gateway lookup to occur.
 * \param imageTag user understandable "name" for an image. In the case of
 *      imageType == "local", then imageTag is understood to be a path.  In the
 *      case of imageType == "id", then imageTag is understood to be an already
 *      looked-up identifier.  In other cases, imageTag is provided to the
 *      gateway as the key lookup.
 * \param verbose level of output (1 for much, 0 for terse)
 * \param config UDI configuration object
 *
 * \returns An allocated string referring to the successfully looked-up image
 *      identifier.  NULL if nothing found.
 */
char *lookup_ImageIdentifier(
        const char *imageType,
        const char *imageTag,
        int verbose,
        UdiRootConfig *config)
{
    char lookupCmd[PATH_MAX];
    FILE *pp = NULL;
    int status = 0;
    char *lineBuffer = NULL;
    char *identifier = NULL;
    char *ptr = NULL;
    size_t lineBuffer_size = 0;
    size_t nread = 0;

    if (imageType == NULL || imageTag == NULL || config == NULL) return NULL;
    if (strlen(imageType) == 0 || strlen(imageTag) == 0) return NULL;

    if (strcmp(imageType, "id") == 0 || strcmp(imageType, "local") == 0) {
        return _strdup(imageTag);
    }

    snprintf(lookupCmd, PATH_MAX, "%s/bin/shifterimg lookup %s:%s",
            config->udiRootPath, imageType, imageTag);

    pp = popen(lookupCmd, "r");
    while (!feof(pp) && !ferror(pp)) {
        nread = getline(&lineBuffer, &lineBuffer_size, pp);
        if (nread == 0 || feof(pp) || ferror(pp)) break;
        lineBuffer[nread] = 0;
        ptr = shifter_trim(lineBuffer);
        if (ptr == NULL) {
            goto _lookupImageIdentifier_error;
        }

        if (strncmp(ptr, "ENV:", 4) == 0) {
            ptr += 4;
            ptr = shifter_trim(ptr);
            if (ptr == NULL) {
                goto _lookupImageIdentifier_error;
            }
        } else if (strncmp(ptr, "ENTRY:", 6) == 0) {
            ptr += 6;
            ptr = shifter_trim(ptr);
            if (ptr == NULL) {
                goto _lookupImageIdentifier_error;
            }
        } else if (identifier == NULL && strchr(ptr, ':') == NULL) {
            /* this is the image id */
            identifier = _strdup(ptr);
            break;
        }
    }
    status = pclose(pp);
    pp = NULL;
    if (WEXITSTATUS(status) != 0) {
        goto _lookupImageIdentifier_error;
    }
    if (lineBuffer != NULL) {
        free(lineBuffer);
        lineBuffer = NULL;
    }
    return identifier;
_lookupImageIdentifier_error:
    if (pp != NULL) {
        pclose(pp);
    }
    if (lineBuffer != NULL) {
        free(lineBuffer);
    }
    if (identifier != NULL) {
        free(identifier);
    }
    return NULL;
}

int parse_ImageData(char *type, char *identifier, UdiRootConfig *config, ImageData *image) {
    char *fname = NULL;
    size_t fname_len = 0;
    const char *extension = NULL;
    int ret = 0;

    if (identifier == NULL || config == NULL || image == NULL) {
        return 1;
    }

    if (type != NULL && strcmp(type, "local") == 0) {
        image->identifier = _strdup(identifier);
        image->filename = _strdup(identifier);
        image->format = FORMAT_VFS;
        image->entryPoint = NULL;
        image->cmd = NULL;
        image->uids = NULL;
        image->gids = NULL;
        return 0;
    }

    fname_len = strlen(config->imageBasePath) + strlen(identifier) + 7;
    fname = (char *) _malloc(sizeof(char) * fname_len);
    snprintf(fname, fname_len, "%s/%s.meta", config->imageBasePath, identifier);

    ret = shifter_parseConfig(fname, ':', image, _ImageData_assign);
    free(fname);

    if (ret != 0) {
        return ret;
    }

    switch (image->format) {
        case FORMAT_VFS:
            extension = "";
            image->useLoopMount = 0;
            break;
        case FORMAT_EXT4:
            extension = "ext4";
            image->useLoopMount = 1;
            break;
        case FORMAT_SQUASHFS:
            extension = "squashfs";
            image->useLoopMount = 1;
            break;
        case FORMAT_CRAMFS:
            extension = "cramfs";
            image->useLoopMount = 1;
            break;
        case FORMAT_XFS:
            extension = "xfs";
            image->useLoopMount = 1;
            break;
        case FORMAT_INVALID:
            extension = "invalid";
            image->useLoopMount = 0;
            break;
    };

    fname_len = strlen(config->imageBasePath) + strlen(identifier) + strlen(extension) + 3;
    image->filename = (char *) _malloc(sizeof(char)*fname_len);
    snprintf(image->filename, fname_len, "%s/%s.%s", config->imageBasePath, identifier, extension);

    image->identifier = _strdup(identifier);

    return 0;
}

void free_ImageData(ImageData *image, int freeStruct) {
    if (image == NULL) return;

    if (image->env != NULL) {
        char **envPtr = NULL;
        for (envPtr = image->env ; *envPtr != NULL; envPtr++) {
            free(*envPtr);
        }
        free(image->env);
        image->env = NULL;
    }
    if (image->filename != NULL) {
        free(image->filename);
        image->filename = NULL;
    }
    if (image->entryPoint != NULL) {
        free(image->entryPoint);
        image->entryPoint = NULL;
    }
    if (image->volume != NULL) {
        char **volPtr = NULL;
        for (volPtr = image->volume; *volPtr != NULL; volPtr++) {
            free(*volPtr);
        }
        free(image->volume);
        image->volume = NULL;
    }
    if (image->identifier != NULL) {
        free(image->identifier);
        image->identifier = NULL;
    }
    if (image->tag != NULL) {
        free(image->tag);
        image->tag = NULL;
    }
    if (image->type != NULL) {
        free(image->type);
        image->type = NULL;
    }
    if (freeStruct == 1) {
        free(image);
    }
}

/** fprint_ImageData
 *  Displays formatted output of the image data structure.
 *  Returns number of bytes written to fp.
 *
 *  Parameters:
 *  fp - pointer to file buffer
 *  image - pointer to ImageData
 *
 *  Error conditions: if fp is NULL or in error at beginning of execution,
 *  returns 0.
 */
size_t fprint_ImageData(FILE *fp, ImageData *image) {
    const char *cptr = NULL;
    char **tptr = NULL;
    size_t nWrite = 0;

    if (fp == NULL) return 0;

    nWrite += fprintf(fp, "***** ImageData *****\n");
    if (image == NULL) {
        nWrite += fprintf(fp, "Null Image - Nothing to Display\n");
        return nWrite;
    }

    switch(image->format) {
        case FORMAT_VFS: cptr = "VFS"; break;
        case FORMAT_EXT4: cptr = "EXT4"; break;
        case FORMAT_SQUASHFS: cptr = "SQUASHFS"; break;
        case FORMAT_CRAMFS: cptr = "CRAMFS"; break;
        case FORMAT_XFS: cptr = "XFS"; break;
        case FORMAT_INVALID: cptr = "INVALID"; break;
    }
    nWrite += fprintf(fp, "Image Format: %s\n", cptr);
    nWrite += fprintf(fp, "Filename: %s\n", (image->filename ? image->filename : ""));
    nWrite += fprintf(fp, "Image Env: %lu defined variables\n", image->env_size);
    for (tptr = image->env; tptr && *tptr; tptr++) {
        nWrite += fprintf(fp, "    %s\n", *tptr);
    }
    nWrite += fprintf(fp, "Image EntryPoint:\n");
    for (tptr = image->entryPoint; tptr && *tptr; tptr++) {
        nWrite += fprintf(fp, "    %s\n", *tptr);
    }
    nWrite += fprintf(fp, "Volume Mounts: %lu mount points\n", image->volume_size);
    for (tptr = image->volume; tptr && *tptr; tptr++) {
        nWrite += fprintf(fp, "    %s\n", *tptr);
    }
    nWrite += fprintf(fp, "***** END ImageData *****\n");
    return nWrite;
}


/**
 * _ImageData_assign - utility function to write data into ImageData structure when
 * parsing configuration file.
 *
 * Parameters:
 * key - identifier from configuration file
 * value - value from configuration file
 * t_image - anonymous pointer which *should* point to an ImageData structure
 *
 * Returns:
 * 0 if value successfully set or appended
 * 1 if invalid input
 * 2 invalid key
 */
int _ImageData_assign(const char *key, const char *value, void *t_image) {
    ImageData *image = (ImageData *) t_image;

    if (image == NULL || key == NULL || value == NULL) {
        return 1;
    }

    if (strcmp(key, "FORMAT") == 0) {
        if (strcmp(value, "VFS") == 0) {
            image->format = FORMAT_VFS;
        } else if (strcmp(value, "ext4") == 0) {
            image->format = FORMAT_EXT4;
        } else if (strcmp(value, "squashfs") == 0) {
            image->format = FORMAT_SQUASHFS;
        } else if (strcmp(value, "cramfs") == 0) {
            image->format = FORMAT_CRAMFS;
        } else if (strcmp(value, "xfs") == 0) {
            image->format = FORMAT_XFS;
        } else {
            image->format = FORMAT_INVALID;
        }
    } else if (strcmp(key, "ENV") == 0) {
        char **tmp = image->env + image->env_size;
        strncpy_StringArray(value, strlen(value), &tmp, &(image->env), &(image->env_capacity), ENV_ALLOC_SIZE);
        image->env_size++;

    } else if (strcmp(key, "ENTRY") == 0) {
        if (is_json_array(value)) {
            char *tmp = _strdup(value);
            image->entryPoint = split_json_array(tmp);
            free(tmp);
        } else {
            image->entryPoint = make_string_array(value);
        }
        if (image->entryPoint == NULL) {
            return 1;
        }
    } else if (strcmp(key, "CMD") == 0) {
        if (is_json_array(value)){
            /* tokenize json array */
            char *tmp = _strdup(value);
            image->cmd = split_json_array(tmp);
            free(tmp);
        } else {
            image->cmd = make_string_array(value);
        }
        if (image->cmd == NULL) {
            return 1;
        }
    } else if (strcmp(key, "WORKDIR") == 0) {
        image->workdir = _strdup(value);
        if (image->workdir == NULL) {
            return 1;
        }
    } else if (strcmp(key, "USERACL") == 0) {
        if (_convert_to_list(value, &image->uids, &image->n_uids) == 0) {
            fprintf(stderr, "ERROR: faild to read USERACLs\n");
            return 1;
        }
    } else if (strcmp(key, "GROUPACL") == 0) {
        if (_convert_to_list(value, &image->gids, &image->n_gids) == 0) {
            fprintf(stderr, "ERROR: failed to read GROUPACLs\n");
            return 1;
        }
    } else if (strcmp(key, "VOLUME") == 0) {
        char **tmp = image->volume + image->volume_size;
        char *tvalue = _ImageData_filterString(value, 1);
        strncpy_StringArray(tvalue, strlen(tvalue), &tmp, &(image->volume), &(image->volume_capacity), VOL_ALLOC_SIZE);
        image->volume_size++;
        free(tvalue);
    } else {
        printf("WARNING: Couldn't understand key: %s\n", key);
    }
    return 0;
}

size_t _convert_to_list(const char *text, uid_t **uids, size_t *n_uids) {
    size_t id_capacity = 0;
    size_t id_count = 0;
    char *ptr = NULL;
    char *buffer = NULL;
    char *search = NULL;
    char *svPtr = NULL;

    if (!text || text[0] == '\0' || !uids || !n_uids) {
        return 0;
    }
    if (*uids) {
        free(*uids);
        *uids = NULL;
    }
    *n_uids = 0;

    buffer = _strdup(text);
    search = buffer;
    svPtr = NULL;
    while ((ptr = strtok_r(search, ",", &svPtr)) != NULL) {
        search = NULL;
        if (id_capacity < id_count + 2) {
            size_t alloc_size = sizeof(uid_t) * (id_count + 2) * 2;
            *uids = _realloc(*uids, alloc_size);
            id_capacity = (id_count + 2) * 2;
        }
        *uids[id_count++] = atoi(ptr);
        if (id_count > 1000) {
            fprintf(stderr, "ERROR: id list growing too large.\n");
            goto error;
        }
    }
    *n_uids = id_count;

    free(buffer);
    buffer = NULL;
    return id_count;
error:
    free(buffer);
    buffer = NULL;
    if (*uids) {
        free(*uids);
        *uids = NULL;
    }
    *n_uids = 0;
    return 0;
}

char *_ImageData_filterString(const char *input, int allowSlash) {
    ssize_t len = 0;
    char *ret = NULL;
    const char *rptr = NULL;
    char *wptr = NULL;
    if (input == NULL) return NULL;

    len = strlen(input) + 1;
    ret = (char *) _malloc(sizeof(char) * len);

    rptr = input;
    wptr = ret;
    while (wptr - ret < len && *rptr != 0) {
        if (isalnum(*rptr) || *rptr == '_' || *rptr == ':' || *rptr == '.' || *rptr == '+' || *rptr == '-') {
            *wptr++ = *rptr;
        }
        if (allowSlash && *rptr == '/') {
            *wptr++ = *rptr;
        }
        rptr++;
    }
    *wptr = 0;
    return ret;
}

char *imageDesc_filterString(const char *input, const char *type) {
    int allowSlash = 0;
    if (type != NULL &&
        (strcmp(type, "local") == 0 || strcmp(type, "docker") == 0)) {

        allowSlash = 1;
    }
    return _ImageData_filterString(input, allowSlash);
}

int parse_ImageDescriptor(char *userinput, char **imageType, char **imageTag, UdiRootConfig *udiConfig) {
    int foundType = 0;
    char *type = NULL;
    char *tag = NULL;
    char *tmp = NULL;
    char *ptr = NULL;
    const char **ref = allowedImageTypes;

    if (imageType == NULL || imageTag == NULL ||
        userinput == NULL || strlen(userinput) == 0 ||
        udiConfig == NULL) {

        fprintf(stderr, "ERROR: NULL input when attempting to parse "
                "image descriptor\n");
        goto _error;
    }

    ptr = strchr(userinput, ':');
    if (ptr != NULL) {
        /* ptr might be an image type, or it might just be part of the tag */
        *ptr = 0;
        tmp = imageDesc_filterString(userinput, NULL);
        if (tmp == NULL) {
            fprintf(stderr, "ERROR: Failed to allocate filtered input string "
                    "when attempting to parse image descriptor.\n");
            goto _error;
        }

        /* check to see if it is in the approved list of types */
        for (ref = allowedImageTypes; ref && *ref; ref++) {
            if (strcmp(*ref, tmp) == 0) {
                foundType = 1;
                break;
            }
        }

        if (foundType) {
            type = _strdup(tmp);
            if (type == NULL) {
                fprintf(stderr, "ERROR: failed to copy type string (out of "
                        "mem?) when attempting to parse image descriptor.\n");
                goto _error;
            }
            *ptr = ':';
            ptr++;
        } else {
            *ptr = ':';
        }

        free(tmp);
        tmp = NULL;
    }

    /* if no type is found, then the whole userinput string must be the image
     * descriptor, so assume default image type from udiRoot.conf */
    if (!foundType && udiConfig->defaultImageType != NULL) {
        type = _strdup(udiConfig->defaultImageType);
        if (type == NULL) {
            fprintf(stderr, "ERROR: failed to copy type string (out of mem?) "
                    "when attempting to parse image descriptor.\n");
            goto _error;
        }
        ptr = userinput;
    }

    /* validate type */
    if (type == NULL || strlen(type) == 0) {
        fprintf(stderr, "FAILED to determine image type.  Either specify image "
                "type or set defaultImageType in udiRoot.conf\n");
        goto _error;
    }
    foundType = 0;
    for (ref = allowedImageTypes; ref && *ref; ref++) {
        if (strcmp(*ref, type) == 0) {
            foundType = 1;
            break;
        }
    }
    if (foundType == 0) {
        fprintf(stderr, "ERROR: requested image type %s is invalid.  Please "
                "check formatting or set defaultImageType in udiRoot.conf "
                "correctly\n", type);
        goto _error;
    }

    tag = imageDesc_filterString(ptr, type);
    if (tag == NULL || strlen(tag) == 0) {
        fprintf(stderr, "FAILED To filter or copy image descriptor when "
                "attempting to parse.  Out of memory or invalid input.\n");
        goto _error;
    }

    *imageType = type;
    *imageTag = tag;
    return 0;

_error:
    if (tmp != NULL) {
        free(tmp);
        tmp = NULL;
    }
    if (type != NULL) {
        free(type);
        type = NULL;
    }
    if (tag != NULL) {
        free(tag);
        tag = NULL;
    }
    return -1;
}

int check_image_permissions(uid_t actualUid, gid_t actualGid, gid_t *aux_gids,
                            int naux_gids, ImageData *imageData)
{
    size_t iidx = 0;
    size_t jidx = 0;

    if (imageData->n_uids == 0 && imageData->n_gids == 0)
        return 1;

    for (iidx = 0; iidx < imageData->n_uids; iidx++)
        if (imageData->uids[iidx] == actualUid)
            return 1;
    for (iidx = 0; iidx < imageData->n_gids; iidx++) {
        if (imageData->gids[iidx] == actualGid)
            return 1;
        for (jidx = 0; jidx < naux_gids; jidx++)
            if (imageData->gids[iidx] == aux_gids[jidx])
                return 1;
    }
    return 0;
}
