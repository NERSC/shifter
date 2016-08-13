/** @file shifter_core.h
 *  @brief Library for setting up and tearing down user-defined images
 *
 *  @author Douglas M. Jacobsen <dmjacobsen@lbl.gov>
 */

/* Shifter, Copyright (c) 2016, The Regents of the University of California,
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

#ifndef __SHFTR_CORE_INCLUDE
#define __SHFTR_CORE_INCLUDE

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

#ifdef __cplusplus
extern "C" {
#endif

#define INVALID_USER UINT_MAX
#define INVALID_GROUP UINT_MAX
#define FILE_SIZE_LIMIT 5242880

int setupUserMounts(VolumeMap *map, UdiRootConfig *udiConfig);
int setupVolumeMapMounts(MountList *mountCache, VolumeMap *map, const char *fromPrefix, const char *toPrefix, dev_t createTo, UdiRootConfig *udiConfig);
int userMountFilter(char *udiRoot, char *filtered_from, char *filtered_to, char *flags);
int isKernelModuleLoaded(const char *name);
int loadKernelModule(const char *name, const char *path, UdiRootConfig *udiConfig);
int mountImageVFS(ImageData *imageData, const char *username, const char *minNodeSpec, UdiRootConfig *udiConfig);
int mountImageLoop(ImageData *imageData, UdiRootConfig *udiConfig);
int loopMount(const char *imagePath, const char *loopMountPath, ImageFormat format, UdiRootConfig *udiConfig, int readonly);
int destructUDI(UdiRootConfig *udiConfig, int killSshd);
int bindImageIntoUDI(const char *relpath, ImageData *imageData, UdiRootConfig *udiConfig, int copyFlag);
int prepareSiteModifications(const char *username, const char *minNodeSpec, UdiRootConfig *udiConfig);
int setupImageSsh(char *sshPubKey, char *username, uid_t uid, gid_t gid, UdiRootConfig *udiConfig);
int startSshd(UdiRootConfig *udiConfig);
int filterEtcGroup(const char *dest, const char *from, const char *username, size_t maxGroups);
int remountUdiRootReadonly(UdiRootConfig *udiConfig);
int forkAndExecv(char *const *argvs);
int forkAndExecvSilent(char *const *argvs);
pid_t findSshd(void);
int killSshd(void);
char **parseMounts(size_t *n_mounts);
char *generateShifterConfigString(const char *, ImageData *, VolumeMap *);
int saveShifterConfig(const char *, ImageData *, VolumeMap *, UdiRootConfig *);
int compareShifterConfig(const char *, ImageData*, VolumeMap *, UdiRootConfig *);
int unmountTree(MountList *mounts, const char *base);
int validateUnmounted(const char *path, int subtree);
int isSharedMount(const char *);
int writeHostFile(const char *minNodeSpec, UdiRootConfig *udiConfig);

/** shifter_copyenv
  * copy current process environ into a newly allocated array with newly
  * allocated strings
  *
  * @return copy of the environment, caller is responsible to deal with memory
  */
char **shifter_copyenv(void);
int shifter_putenv(char ***env, char *var);
int shifter_appendenv(char ***env, char *var);
int shifter_prependenv(char ***env, char *var);
int shifter_unsetenv(char ***env, char *var);
int shifter_setupenv(char ***env, ImageData *image, UdiRootConfig *udiConfig);
struct passwd *shifter_getpwuid(uid_t tgtuid, UdiRootConfig *config);
struct passwd *shifter_getpwnam(const char *tgtnam, UdiRootConfig *config);

/**
  * @brief read one record from group formatted field
  *
  * Reads an already open file line-by-line and returns a single group record.
  * This is really an internal function intended to be used for parsing the
  * shifter-specific group-file, and is implemented because various libc
  * cannot be trusted to properly parse HUGE group files with large numbers of
  * members in a reliable way. This function is intended to be called multiple
  * times to parse a group file. User is responsible to free the contents of
  * @p linebuf and @p grmembuf at the conclusion of all calls. This function
  * gets around libc limitations by delegating memory management to calling
  * function and not implementing any membership quantity limits.
  * 
  * Usage example:
  * @code
  * FILE *fp = fopen("group", "r");
  * struct group grbuf, *gr = NULL;
  * char *linebuf = NULL;
  * size_t linebuf_sz = 0;
  * char **grmembuf = NULL;
  * size_t grmembuf_sz = 0;
  * for ( ; ; ) {
  *     gr = shifter_fgetgrent(fp, &grbuf, &linebuf, &linebuf_sz,
  *         &grmembuf, &grmembuf_sz);
  *     if (!gr) break;
  *     // do something with group data in gr
  * }
  * if (linebuf) free(linebuf);
  * if (grmembuf) free(grmembuf);
  * close(fp);
  * @endcode
  *
  * @param input pointer to an open group file
  * @param gr pointer to an already-allocated (struct group) data structure
  * @param linebuf pointer to string buffer for all character data, must not be
  *        NULL, however may point to a NULL pointer. May be modified.
  * @param linebuf_sz pointer to length of @p linebuf buffer.  May be modified.
  * @param grmembuf pointer to store array of group members. May be modified.
  * @param grmembuf_sz pointer to length of @p grmembuf buffer. May be modified.
  * @return Returns @p gr upon success, NULL otherwise.  NULL if invalid input.
  */
struct group *
shifter_fgetgrent(FILE *input, struct group *gr, char **linebuf,
    size_t *linebuf_sz, char ***grmembuf, size_t *grmembuf_sz);

/**
 * @brief Generate group list for a user from shifter-specific group file
 *
 * Utility function to generate a list of groups from the shifter-specific group
 * file. @p groups may get reallocated to store group list.
 * 
 * Usage example:
 * @code
 * // assume UdiRootConfig is already parse and stored in config
 * gid_t *grpList = NULL;
 * size_t ngroups = 0;
 * if (shifter_getgrouplist("foo", fooGrp, &grpList, &ngroups, config) != 0) {
 *     // ERROR
 *     ...
 * }
 * // Use Groups...
 * @endcode
 *
 * @param user string container username to search for
 * @param group primary gid to seed gidlist with (from passwd file)
 * @param groups list of groups, will be reallocated and modified. Cannot be
 *        NULL, but may be a pointer to NULL.
 * @param ngroups store number of found groups, will be modified.  Cannot be
 *        NULL.
 * @param config pointer to UdiRootConfig configuration
 * @return 0 upon sucess, -1 otherwise
 */
int
shifter_getgrouplist(const char *user, gid_t group, gid_t **groups,
    size_t *ngroups, UdiRootConfig *config);

int setupPerNodeCacheFilename(UdiRootConfig *udiConfig, VolMapPerNodeCacheConfig *, char *, size_t);
int setupPerNodeCacheBackingStore(VolMapPerNodeCacheConfig *cache, const char *from_buffer, UdiRootConfig *udiConfig);
int makeUdiMountPrivate(UdiRootConfig *udiConfig);
char **getSupportedFilesystems();
int supportsFilesystem(char *const * fsTypes, const char *fsType);

#ifdef __cplusplus
}
#endif

#endif
