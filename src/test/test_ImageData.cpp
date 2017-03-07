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

#include "ImageData.h"
#include "UdiRootConfig.h"
#include "utility.h"

#include <CppUTest/CommandLineTestRunner.h>

extern "C" {
extern int _ImageData_assign(const char *key, const char *value, void *t_image);
}

TEST_GROUP(ImageDataTestGroup) {
};

TEST(ImageDataTestGroup, ConfigAssign_basic) {
    int ret = 0;
    ImageData image;
    memset(&image, 0, sizeof(ImageData));

    ret = _ImageData_assign(NULL, NULL, NULL);
    CHECK(ret == 1);

    ret = _ImageData_assign("ENV", "PATH=/bin:/usr/bin", &image);
    CHECK(ret == 0);

    free_ImageData(&image, 0);

}

TEST(ImageDataTestGroup, FilterString_basic) {
    CHECK(imageDesc_filterString(NULL, NULL) == NULL);
    char *output = imageDesc_filterString("echo test; rm -rf thing1", NULL);
    CHECK(strcmp(output, "echotestrm-rfthing1") == 0);
    free(output);
    output = imageDesc_filterString("V4l1d-str1ng.input", NULL);
    CHECK(strcmp(output, "V4l1d-str1ng.input") == 0);
    free(output);
    output = imageDesc_filterString("", NULL);
    CHECK(output != NULL);
    CHECK(strlen(output) == 0);
    free(output);

    output = imageDesc_filterString("/this/is/not/allowed", NULL);
    CHECK(strcmp(output, "thisisnotallowed") == 0);
    free(output);
    output = imageDesc_filterString("/this/is/allowed", "local");
    CHECK(strcmp(output, "/this/is/allowed") == 0);
    free(output);
}

TEST(ImageDataTestGroup, parseImageDescriptor) {
    UdiRootConfig config;
    config.defaultImageType = strdup("docker");

    char *input = NULL;
    char *tag = NULL;
    char *type = NULL;
    int ret = 0;

    /* check valid input with explicit well-formed descriptor */
    input = strdup("docker:ubuntu:14.04");
    ret = parse_ImageDescriptor(input, &type, &tag, &config);
    CHECK(ret == 0);
    CHECK(tag != NULL && strcmp(tag, "ubuntu:14.04") == 0);
    CHECK(type != NULL && strcmp(type, "docker") == 0);
    free(tag);
    free(type);
    free(input);

    /* check valid input with implicit type used by descriptor */
    tag = NULL;
    type = NULL;
    input = strdup("ubuntu:14.04");
    ret = parse_ImageDescriptor(input, &type, &tag, &config);
    CHECK(ret == 0);
    CHECK(type != NULL && strcmp(type, "docker") == 0);
    CHECK(tag != NULL && strcmp(tag, "ubuntu:14.04") == 0);
    free(tag);
    free(type);
    free(input);

    /* check that NULL pointers for inputs are rejected */
    tag = NULL;
    type = NULL;
    input = strdup("test");
    CHECK(parse_ImageDescriptor(NULL, &type, &tag, &config) == -1);
    CHECK(parse_ImageDescriptor(input, NULL, &tag, &config) == -1);
    CHECK(parse_ImageDescriptor(input, &type, NULL, &config) == -1);
    CHECK(parse_ImageDescriptor(input, &type, &tag, NULL) == -1);
    free(input);

    /* check invalid descriptor with NULL default */
    free(config.defaultImageType);
    config.defaultImageType = NULL;
    input = strdup("invalid");
    CHECK(parse_ImageDescriptor(input, &type, &tag, &config) == -1);

    /* check invalid descriptor owing to invalid default */
    config.defaultImageType = strdup("invalid");
    CHECK(parse_ImageDescriptor(input, &type, &tag, &config) == -1);
    free(input);
    free(config.defaultImageType);

    /* check that invalid input is properly screened */
    config.defaultImageType = strdup("docker");
    input = strdup("this is *incorrect*");
    type = NULL;
    tag = NULL;
    CHECK(parse_ImageDescriptor(input, &type, &tag, &config) == 0);
    CHECK(type != NULL && strcmp(type, "docker") == 0);
    CHECK(tag != NULL && strcmp(tag, "thisisincorrect") == 0);
    free(type);
    free(tag);
}

int main(int argc, char** argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
