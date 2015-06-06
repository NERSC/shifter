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

static char *_trim(char *);
static int _assign(char *key, char *value, void *t_imageData);

int parse_ImageData(char *type, char *identifier, UdiRootConfig *config, ImageData *image) {
    char *md_fname = NULL;
    size_t md_fname_len = 0;
    int ret = 0;

    md_fname_len = strlen(config->imageBasePath) + strlen(identifier) + 7;
    md_fname = (char *) malloc(sizeof(char) * md_fname_len);
    snprintf(md_fname, md_fname_len, "%s/%s.meta", config->imageBasePath, identifier);

    ret = shifter_parseConfig(md_fname, image, _assign);

    free(md_fname);

    return ret;
}

void free_ImageData(ImageData *image) {
    size_t idx = 0;
    if (image->env != NULL) {
        for (idx = 0; idx < image->envCount; idx++) {
            free(image->env[idx]);
        }
        free(image->env);
    }
    if (image->entryPoint != NULL) {
        free(image->entryPoint);
    }
    if (image->volume != NULL) {
        for (idx = 0; idx < image->volumeCount; idx++) {
            free(image->volume[idx]);
        }
        free(image->volume);
    }
    free(image);
}

void fprint_ImageData(FILE *fp, ImageData *image) {
    size_t idx = 0;
    const char *cptr = NULL;

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
        image->env = (char **) realloc(image->env, sizeof(char *) * (image->envCount + 1));
        image->env[image->envCount] = strdup(value);
        image->envCount++;
        if (image->env[image->envCount - 1] == NULL) {
            return 1;
        }
    } else if (strcmp(key, "ENTRY") == 0) {
        image->entryPoint = strdup(value);
        if (image->entryPoint == NULL) {
            return 1;
        }
    } else if (strcmp(key, "VOLUME") == 0) {
        image->volume = (char **) realloc(image->volume, sizeof(char *) * (image->volumeCount + 1));
        image->volume[image->volumeCount] = strdup(value);
        image->volumeCount++;
        if (image->volume[image->volumeCount - 1] == NULL) {
            return 1;
        }
    } else {
        printf("Couldn't understand key: %s\n", key);
        return 2;
    }
    return 0; 
}
