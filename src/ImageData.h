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

#ifndef __IMAGEDATA_INCLUDE
#define __IMAGEDATA_INCLUDE

#include "UdiRootConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _ImageFormat {
    FORMAT_VFS = 0,
    FORMAT_EXT4,
    FORMAT_SQUASHFS,
    FORMAT_CRAMFS,
    FORMAT_XFS,
    FORMAT_INVALID
} ImageFormat;

typedef struct _ImageData {
    ImageFormat format;     /*!< image format  */
    char *filename;         /*!< path to image */
    char **env;             /*!< array of environment variables */
    char *entryPoint;       /*!< default command used */
    char *workdir;          /*!< working dir of entrypoint */
    char **volume;          /*!< array of volume mounts */
    int useLoopMount;       /*!< flag if image requires loop mount */
    char *identifier;       /*!< Image identifier string */
    char *tag;              /*!< Image tag */
    char *type;             /*!< Image type */
    char *status;           /*!< Image status from gateway */

    size_t env_capacity;    /*!< Current # of allocated char* in env */
    size_t volume_capacity; /*!< Current # of allocated char* in volumes */
    size_t env_size;        /*!< Number of elements in env array */
    size_t volume_size;     /*!< Number of elements in volume array */
} ImageData;

char *lookup_ImageIdentifier(const char *imageType, const char *imageTag, int verbose, UdiRootConfig *);
int parse_ImageData(char *type, char *identifier, UdiRootConfig *, ImageData *);
void free_ImageData(ImageData *, int);
size_t fprint_ImageData(FILE *, ImageData *);


/**
 * parse_ImageDescriptor parses user-provided image descriptor strings
 * The fully formalized format for this is:
 *     imageType:imageIdentifyingTag
 * for example:
 *     docker:ubuntu:14.04
 *     docker:privateRegistry/repo:latest
 *     docker:someSpecificDockerUser/someRepo:1.4.2.3.4
 *     id:23429387329872...
 *     local:/
 *
 * or
 *     ubuntu:14.04
 * in which case no image type is specified which leads to the defaultImageType
 * from udiRoot.conf to be assumed.
 *
 * Once parsed these data are useful to either fully determine which image to
 * load (if type == id or type == local), or perform a lookup using from the
 * image manager via shifterimg (if type is anything else, but particularly
 * docker).
 *
 * The calling function will provide pointers to uninitialized char* instances
 * which are used to store the resulting type and tag values.  The calling 
 * function is responsible to free the stored strings for imageType and
 * imageTag.
 *
 * \param userinput user-provided string in above format for desired image
 * \param imageType string pointer to store calculated image type
 * \param imageTag string pointer to store calucated image tag
 * \param udiConfig UdiRootConfig configuration structure
 * \returns 0 upon success, -1 upon error, any error should be fatal
 */
int parse_ImageDescriptor(char *userinput, char **imageType, char **imageTag, UdiRootConfig *);

/**
  * imageDesc_filterString screens out disallowed characters from user input
  *
  * Allowed characters are [A-Za-z0-9_:.+-]
  * Depending on image type '/' is sometimes allowed
  *
  * Any other characters are simply screened out (removed and skipped over)
  *
  * \param target the user-input to filter
  * \param type if not-NULL can adjust the allowed characters based on avlue
  * \returns newly allocated filtered string
  */
char *imageDesc_filterString(char *target, const char *type);

#ifdef __cplusplus
}
#endif

#endif
