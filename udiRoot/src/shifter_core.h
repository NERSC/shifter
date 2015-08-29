/** @file shifter_core.h
 *  @brief Library for setting up and tearing down user-defined images
 *
 *  @author Douglas M. Jacobsen <dmjacobsen@lbl.gov>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *  
 * You are under no obligation whatsoever to provide any bug fixes, patches, or
 * upgrades to the features, functionality or performance of the source code
 * ("Enhancements") to anyone; however, if you choose to make your Enhancements
 * available either publicly, or directly to Lawrence Berkeley National
 * Laboratory, without imposing a separate written license agreement for such
 * Enhancements, then you hereby grant the following license: a  non-exclusive,
 * royalty-free perpetual license to install, use, modify, prepare derivative
 * works, incorporate into other computer software, distribute, and sublicense
 * such enhancements or derivative works thereof, in binary and source code
 * form.
 */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>

#include "ImageData.h"
#include "UdiRootConfig.h"
#include "VolumeMap.h"
#include "MountList.h"

#define INVALID_USER UINT_MAX
#define INVALID_GROUP UINT_MAX
#define FILE_SIZE_LIMIT 5242880

int setupUserMounts(ImageData *imageData, VolumeMap *map, UdiRootConfig *udiConfig);
int userMountFilter(char *udiRoot, char *filtered_from, char *filtered_to, char *flags);
int isKernelModuleLoaded(const char *name);
int loadKernelModule(const char *name, const char *path, UdiRootConfig *udiConfig);
int mountImageVFS(ImageData *imageData, const char *username, const char *minNodeSpec, UdiRootConfig *udiConfig);
int mountImageLoop(ImageData *imageData, UdiRootConfig *udiConfig);
int destructUDI(UdiRootConfig *udiConfig, int killSshd);
int bindImageIntoUDI(const char *relpath, ImageData *imageData, UdiRootConfig *udiConfig, int copyFlag);
int prepareSiteModifications(const char *username, const char *minNodeSpec, UdiRootConfig *udiConfig);
int setupImageSsh(char *sshPubKey, char *username, uid_t uid, UdiRootConfig *udiConfig);
int startSshd(UdiRootConfig *udiConfig);
int filterEtcGroup(const char *dest, const char *from, const char *username);
int forkAndExecvp(char *const *args);
int forkAndExecv(char *const *argvs);
int killSshd(void);
char **parseMounts(size_t *n_mounts);
char *userInputPathFilter(const char *input, int allowSlash);
char *generateShifterConfigString(const char *, ImageData *, VolumeMap *);
int readShifterConfigString(const char *configStr, char **user, char **imageIdentifier, VolumeMap *volMap);
int saveShifterConfig(const char *, ImageData *, VolumeMap *, UdiRootConfig *);
int compareShifterConfig(const char *, ImageData*, VolumeMap *, UdiRootConfig *);
int unmountTree(MountList *mounts, const char *base);
int validateUnmounted(const char *path, int subtree);
int isSharedMount(const char *);
int writeHostFile(const char *minNodeSpec, UdiRootConfig *udiConfig);
