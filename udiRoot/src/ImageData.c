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

#include <sys/types.h>
#include <sys/stat.h>

#include "ImageData.h"
#include "UdiRootConfig.h"
#include "utility.h"

#define ENV_ALLOC_SIZE 512
#define VOL_ALLOC_SIZE 256

static int _assign(char *key, char *value, void *t_imageData);

int parse_ImageData(char *type, char *identifier, UdiRootConfig *config, ImageData *image) {
    char *fname = NULL;
    size_t fname_len = 0;
    const char *extension = NULL;
    int ret = 0;

    fname_len = strlen(config->imageBasePath) + strlen(identifier) + 7;
    fname = (char *) malloc(sizeof(char) * fname_len);
    if (fname == NULL) {
        return 1;
    }
    snprintf(fname, fname_len, "%s/%s.meta", config->imageBasePath, identifier);

    ret = shifter_parseConfig(fname, ':', image, _assign);
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
        case FORMAT_INVALID:
            extension = "invalid";
            image->useLoopMount = 0;
            break;
    };

    fname_len = strlen(config->imageBasePath) + strlen(identifier) + strlen(extension) + 3;
    image->filename = (char *) malloc(sizeof(char)*fname_len);
    if (image->filename == NULL) {
        return 1;
    }
    snprintf(image->filename, fname_len, "%s/%s.%s", config->imageBasePath, identifier, extension);

    return 0;
}

void free_ImageData(ImageData *image) {
    if (image->env != NULL) {
        char **envPtr = NULL;
        for (envPtr = image->env ; *envPtr != NULL; envPtr++) {
            free(*envPtr);
        }
        free(image->env);
    }
    if (image->filename != NULL) {
        free(image->filename);
    }
    if (image->entryPoint != NULL) {
        free(image->entryPoint);
    }
    if (image->volume != NULL) {
        char **volPtr = NULL;
        for (volPtr = image->volume; *volPtr != NULL; volPtr++) {
            free(*volPtr);
        }
        free(image->volume);
    }
    free(image);
}

void fprint_ImageData(FILE *fp, ImageData *image) {
    const char *cptr = NULL;
    char **tptr = NULL;

    if (image == NULL || fp == NULL) return;

    fprintf(fp, "***** ImageData *****\n");
    switch(image->format) {
        case FORMAT_VFS: cptr = "VFS"; break;
        case FORMAT_EXT4: cptr = "EXT4"; break;
        case FORMAT_SQUASHFS: cptr = "SQUASHFS"; break;
        case FORMAT_CRAMFS: cptr = "CRAMFS"; break;
        case FORMAT_INVALID: cptr = "INVALID"; break;
    }
    fprintf(fp, "Image Format: %s\n", cptr);
    fprintf(fp, "Filename: %s\n", (image->filename ? image->filename : ""));
    fprintf(fp, "Image Env: %lu defined variables\n", (image->envPtr - image->env));
    for (tptr = image->env; tptr && *tptr; tptr++) {
        fprintf(fp, "    %s\n", *tptr);
    }
    fprintf(fp, "EntryPoint: %s\n", (image->entryPoint != NULL ? image->entryPoint : ""));
    fprintf(fp, "Volume Mounts: %lu mount points\n", (image->volPtr - image->volume));
    for (tptr = image->volume; tptr && *tptr; tptr++) {
        fprintf(fp, "    %s\n", *tptr);
    }
    fprintf(fp, "***** END ImageData *****\n");
}

static int _assign(char *key, char *value, void *t_image) {
    ImageData *image = (ImageData *) t_image;

    if (strcmp(key, "FORMAT") == 0) {
        if (strcmp(value, "VFS") == 0) {
            image->format = FORMAT_VFS;
        } else if (strcmp(value, "ext4") == 0) {
            image->format = FORMAT_EXT4;
        } else if (strcmp(value, "squashfs") == 0) {
            image->format = FORMAT_SQUASHFS;
        } else if (strcmp(value, "cramfs") == 0) {
            image->format = FORMAT_CRAMFS;
        } else {
            image->format = FORMAT_INVALID;
        }
    } else if (strcmp(key, "ENV") == 0) {
        char **tmp = NULL;
        if (image->env == NULL || image->envPtr - image->env >= image->env_capacity) {
            size_t cnt = image->envPtr - image->env;
            tmp = realloc(image->env, sizeof(char*) * (image->env_capacity + ENV_ALLOC_SIZE));
            if (tmp == NULL) {
                return 1;
            }
            image->env_capacity += ENV_ALLOC_SIZE;
            image->env = tmp;
            image->envPtr = tmp + cnt;
        }
        *(image->envPtr) = strdup(value);
        image->envPtr++;
        *(image->envPtr) = NULL;

    } else if (strcmp(key, "ENTRY") == 0) {
        image->entryPoint = strdup(value);
        if (image->entryPoint == NULL) {
            return 1;
        }
    } else if (strcmp(key, "VOLUME") == 0) {
        char **tmp = NULL;
        if (image->volume == NULL || image->volPtr - image->volume >= image->volume_capacity) {
            size_t cnt = image->volPtr - image->volume;
            tmp = realloc(image->volume, sizeof(char*) * (image->volume_capacity + VOL_ALLOC_SIZE));
            if (tmp == NULL) {
                return 1;
            }
            image->volume_capacity += VOL_ALLOC_SIZE;
            image->volume = tmp;
            image->volPtr = tmp + cnt;
        }
        *(image->volPtr) = strdup(value);
        image->volPtr++;
        *(image->volPtr) = NULL;
    } else {
        printf("Couldn't understand key: %s\n", key);
        return 2;
    }
    return 0; 
}
