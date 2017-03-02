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

#ifndef __UDIROOTCONFIG_INCLUDE
#define __UDIROOTCONFIG_INCLUDE

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "VolumeMap.h"
#include "gpu_support.h"
#include "mpi_support.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UDIROOT_VAL_CFGFILE 0x01
#define UDIROOT_VAL_PARSE   0x02
#define UDIROOT_VAL_SSH     0x04 
#define UDIROOT_VAL_KMOD    0x08
#define UDIROOT_VAL_FILEVAL 0x10
#define UDIROOT_VAL_ALL 0xffffffff

#ifndef IMAGEGW_PORT_DEFAULT
#define IMAGEGW_PORT_DEFAULT "7777"
#endif

typedef struct _ImageGwServer {
    char *server;
    int port;
} ImageGwServer;

typedef struct _UdiRootConfig {
    /* long term configurations coming from configuration file */
    char *udiMountPoint;
    char *loopMountPoint;
    char *batchType;
    char *defaultImageType;
    char *system;
    char *imageBasePath;
    char *udiRootPath;
    char *perNodeCachePath;
    size_t perNodeCacheSizeLimit;
    char **perNodeCacheAllowedFsType;
    char *sitePreMountHook;
    char *sitePostMountHook;
    char *optUdiImage;
    char *etcPath;
    char *kmodBasePath;
    char *kmodPath;
    char *kmodCacheFile;
    char *rootfsType;
    char **gwUrl;
    VolumeMap *siteFs;
    char *siteResources;
    char *siteMPISharedLibs;
    char *siteMPIDependencyLibs;
    char *siteMPIBins;
    char *siteMPIConfigurationFiles;
    char **siteEnv;
    char **siteEnvAppend;
    char **siteEnvPrepend;
    char **siteEnvUnset;
    int allowLocalChroot;
    int allowLibcPwdCalls;
    int populateEtcDynamically;
    int autoLoadKernelModule;
    int mountUdiRootWritable;
    int optionalSshdAsRoot;
    size_t maxGroupCount;
    size_t gatewayTimeout;
    size_t mountPropagationStyle;

    char *modprobePath;
    char *insmodPath;
    char *cpPath;
    char *mvPath;
    char *chmodPath;
    char *ddPath;
    char *mkfsXfsPath;

    /* support variables for above */
    size_t siteEnv_capacity;
    size_t siteEnvAppend_capacity;
    size_t siteEnvPrepend_capacity;
    size_t siteEnvUnset_capacity;
    size_t gwUrl_capacity;
    size_t gwUrl_size;
    size_t siteEnv_size;
    size_t siteEnvAppend_size;
    size_t siteEnvPrepend_size;
    size_t siteEnvUnset_size;
    size_t perNodeCacheAllowedFsType_capacity;
    size_t perNodeCacheAllowedFsType_size;

    /* current execution context */
    uid_t target_uid;
    gid_t target_gid;
    gid_t *auxiliary_gids;
    int nauxiliary_gids;
    char *username;
    char *sshPubKey;
    char *nodeIdentifier;
    char *jobIdentifier;
    dev_t *bindMountAllowedDevices;
    size_t bindMountAllowedDevices_sz;

    struct gpu_support_config gpu_config;
    struct mpi_support_config mpi_config;
} UdiRootConfig;

int parse_UdiRootConfig(const char *, UdiRootConfig *, int validateFlags);
void free_UdiRootConfig(UdiRootConfig *, int freeStruct);
size_t fprint_UdiRootConfig(FILE *, UdiRootConfig *);
int validate_UdiRootConfig(UdiRootConfig *, int validateFlags);

#ifdef __cplusplus
}
#endif

#endif
