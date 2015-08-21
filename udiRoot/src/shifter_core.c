/** @file shifter_core.c
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
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/types.h>

#include "ImageData.h"
#include "UdiRootConfig.h"
#include "shifter_core.h"
#include "utility.h"
#include "VolumeMap.h"
#include "MountList.h"

#ifndef ROOTFS_TYPE
#define ROOTFS_TYPE "tmpfs"
#endif



static int _bindMount(MountList *mounts, const char *from, const char *to, int ro, int overwrite);
static int _copyFile(const char *source, const char *dest, int keepLink, uid_t owner, gid_t group, mode_t mode);

/*! Bind subtree of static image into UDI rootfs */
/*!
  Bind mount directories and large files (copy symlinks and small files) from
  image VFS location to prepared UDI VFS location.  This functionality performs
  same basic need as overlayfs for systems not supporting overlayfs.

  \param relpath The subtree to examine
  \param imageData Metadata about image
  \param config configuration for this invocation of setupRoot
  \param udiConfig global configuration for udiRoot
  \param copyFlag if zero, use bind mounts; if one recursively copy
 */
int bindImageIntoUDI(
        const char *relpath,
        ImageData *imageData,
        UdiRootConfig *udiConfig,
        int copyFlag
) {
    char udiRoot[PATH_MAX];
    char imgRoot[PATH_MAX];
    char mntBuffer[PATH_MAX];
    char srcBuffer[PATH_MAX];
    DIR *subtree = NULL;
    struct dirent *dirEntry = NULL;
    struct stat statData;
    char *itemname = NULL;
    int rc = 0;

    MountList mountCache;
    memset(&mountCache, 0, sizeof(MountList));

    if (relpath == NULL || strlen(relpath) == 0 || imageData == NULL ||
            udiConfig == NULL)
    {
        return 1;
    }

#define MKDIR(dir, perm) if (mkdir(dir, perm) != 0) { \
    fprintf(stderr, "FAILED to mkdir %s. Exiting.\n", dir); \
    goto _bindImgUDI_unclean; \
}
#define BINDMOUNT(mounts, from, to, ro, overwrite) if (_bindMount(mounts, from, to, ro, overwrite) != 0) { \
    fprintf(stderr, "BIND MOUNT FAILED from %s to %s\n", from, to); \
    goto _bindImgUDI_unclean; \
}

    memset(&statData, 0, sizeof(struct stat));

    if (parse_MountList(&mountCache) != 0) {
        fprintf(stderr, "FAILED to read existing mounts.\n");
        return 1;
    }
    setSort_MountList(&mountCache, MOUNT_SORT_FORWARD);

    /* calculate important base paths */
    snprintf(udiRoot, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;

    if (imageData->useLoopMount) {
        snprintf(imgRoot, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->loopMountPoint);
        imgRoot[PATH_MAX-1] = 0;
    } else {
        snprintf(imgRoot, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, imageData->filename);
        imgRoot[PATH_MAX-1] = 0;
    }

    /* start traversing through image subtree */
    snprintf(srcBuffer, PATH_MAX, "%s/%s", imgRoot, relpath);
    srcBuffer[PATH_MAX-1] = 0;
    subtree = opendir(srcBuffer);
    if (subtree == NULL) {
        /* desired path is not a directory we can see, skip */
        rc = 1;
        goto _bindImgUDI_unclean;
    }
    while ((dirEntry = readdir(subtree)) != NULL) {
        if (strcmp(dirEntry->d_name, ".") == 0 || strcmp(dirEntry->d_name, "..") == 0) {
            continue;
        }
        itemname = userInputPathFilter(dirEntry->d_name, 0);
        if (itemname == NULL) {
            fprintf(stderr, "FAILED to correctly filter entry: %s\n", dirEntry->d_name);
            rc = 2;
            goto _bindImgUDI_unclean;
        }
        if (strlen(itemname) == 0) {
            free(itemname);
            continue;
        }

        /* check to see if UDI version already exists */
        snprintf(mntBuffer, PATH_MAX, "%s/%s/%s", udiRoot, relpath, itemname);
        mntBuffer[PATH_MAX-1] = 0;
        if (lstat(mntBuffer, &statData) == 0) {
            /* exists in UDI, skip */
            free(itemname);
            continue;
        }

        /* after filtering, lstat path to get details */
        snprintf(srcBuffer, PATH_MAX, "%s/%s/%s", imgRoot, relpath, itemname);
        srcBuffer[PATH_MAX-1] = 0;
        if (lstat(srcBuffer, &statData) != 0) {
            /* path didn't exist, skip */
            free(itemname);
            continue;
        }

        /* if target is a symlink, copy it */
        if (S_ISLNK(statData.st_mode)) {
            char *args[] = { strdup("cp"), strdup("-P"), 
                strdup(srcBuffer), strdup(mntBuffer), NULL
            };
            char **argsPtr = NULL;
            int ret = forkAndExecvp(args);
            for (argsPtr = args; *argsPtr != NULL; argsPtr++) {
                free(*argsPtr);
            }
            if (ret != 0) {
                fprintf(stderr, "Failed to copy %s to %s.\n", srcBuffer, mntBuffer);
                rc = 2;
                goto _bindImgUDI_unclean;
            }
            free(itemname);
            continue;
        }
        if (S_ISREG(statData.st_mode)) {
            if (statData.st_size < FILE_SIZE_LIMIT) {
                char *args[] = { strdup("cp"), strdup("-p"),
                    strdup(srcBuffer), strdup(mntBuffer), NULL
                };
                char **argsPtr = NULL;
                int ret = forkAndExecvp(args);
                for (argsPtr = args; *argsPtr != NULL; argsPtr++) {
                    free(*argsPtr);
                }
                if (ret != 0) {
                    fprintf(stderr, "Failed to copy %s to %s.\n", srcBuffer, mntBuffer);
                    rc = 2;
                    goto _bindImgUDI_unclean;
                }
            } else if (copyFlag == 0) {
                /* create the file */
                FILE *fp = fopen(mntBuffer, "w");
                fclose(fp);
                BINDMOUNT(&mountCache, srcBuffer, mntBuffer, 0, 1);
            }
            free(itemname);
            continue;
        }
        if (S_ISDIR(statData.st_mode)) {
            if (copyFlag == 0) {
                MKDIR(mntBuffer, 0755);
                BINDMOUNT(&mountCache, srcBuffer, mntBuffer, 0, 1);
            } else {
                char *args[] = { strdup("cp"), strdup("-rp"),
                    strdup(srcBuffer), strdup(mntBuffer), NULL
                };
                char **argsPtr = NULL;
                int ret = forkAndExecvp(args);
                for (argsPtr = args; *argsPtr != NULL; argsPtr++) {
                    free(*argsPtr);
                }
                if (ret != 0) {
                    fprintf(stderr, "Failed to copy %s to %s.\n", srcBuffer, mntBuffer);
                    rc = 2;
                    goto _bindImgUDI_unclean;
                }
            }
            free(itemname);
            continue;
        }
        /* no other types are supported */
        free(itemname);
    }
    closedir(subtree);

#undef MKDIR
#undef BINDMOUNT

    free_MountList(&mountCache, 0);
    return 0;

_bindImgUDI_unclean:
    free_MountList(&mountCache, 0);
    if (itemname != NULL) {
        free(itemname);
        itemname = NULL;
    }
    return rc;
}

/*! Copy a file or link as correctly as possible */
/*!
 * Copy file (or symlink) from source to dest.
 * \param source Filename to copy, must be an existing regular file or an
 *             existing symlink
 * \param dest Destination of copy, must be an existing directory name or a
 *             non-existing filename
 * \param keepLink If set to 1 copy symlink, if set to 0 copy file dereferenced
 *             value of symlink
 * \param owner uid to set dest, INVALID_USER uses source ownership
 * \param group gid to set dest, INVALID_GROUP uses source group ownership
 * \param mode octal mode of file, 0 keeps source permissions
 *
 * In all cases stick/setuid bits will be removed.
 */
int _copyFile(const char *source, const char *dest, int keepLink, uid_t owner, gid_t group, mode_t mode) {
    struct stat destStat;
    struct stat sourceStat;
    char *cmdArgs[5] = { NULL, NULL, NULL, NULL, NULL };
    char **ptr = NULL;
    size_t cmdArgs_idx = 0;
    int isLink = 0;
    mode_t tgtMode = mode;

    if (dest == NULL || source == NULL || strlen(dest) == 0 || strlen(source) == 0) {
        fprintf(stderr, "Invalid arguments for _copyFile\n");
        goto _copyFile_unclean;
    }
    if (stat(dest, &destStat) == 0) {
        /* check if dest is a directory */
        if (!S_ISDIR(destStat.st_mode)) {
            fprintf(stderr, "Destination path %s exists and is not a directory. Will not copy\n", dest);
            goto _copyFile_unclean;
        }
    }
    if (stat(source, &sourceStat) != 0) {
        fprintf(stderr, "Source file %s does not exist. Cannot copy\n", source);
        goto _copyFile_unclean;
    } else {
        if (S_ISLNK(sourceStat.st_mode)) {
            isLink = 1;
        } else if (S_ISDIR(sourceStat.st_mode)) {
            fprintf(stderr, "Source path %s is a directory. Will not copy\n", source);
            goto _copyFile_unclean;
        }
    }

    cmdArgs[cmdArgs_idx++] = strdup("cp");
    if (isLink == 1 && keepLink == 1) {
        cmdArgs[cmdArgs_idx++] = strdup("-P");
    }
    cmdArgs[cmdArgs_idx++] = strdup(source);
    cmdArgs[cmdArgs_idx++] = strdup(dest);
    cmdArgs[cmdArgs_idx++] = NULL;

    if (forkAndExecvp(cmdArgs) != 0) {
        fprintf(stderr, "Failed to copy %s to %s\n", source, dest);
        goto _copyFile_unclean;
    }

    if (owner != INVALID_USER && group != INVALID_GROUP) {
        if (owner == INVALID_USER) owner = sourceStat.st_uid;
        if (group == INVALID_GROUP) group = sourceStat.st_gid;
        if (chown(dest, owner, group) != 0) {
            fprintf(stderr, "Failed to set ownership to %d:%d on %s\n", owner, group, dest);
            goto _copyFile_unclean;
        }
    }

    if (mode == 0) tgtMode = sourceStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
    tgtMode &= ~(S_ISUID | S_ISGID | S_ISVTX);
    if (chmod(dest, tgtMode) != 0) {
        fprintf(stderr, "Failed to set permissions on %s to %o\n", dest, tgtMode);
        goto _copyFile_unclean;
    }

    for (ptr = cmdArgs; *ptr != NULL; ptr++) {
        free(*ptr);
        *ptr = NULL;
    }
    return 0;
_copyFile_unclean:
    for (ptr = cmdArgs; *ptr != NULL; ptr++) {
        free(*ptr);
        *ptr = NULL;
    }
    return 1;
}

/*! Setup all required files/paths for site mods to the image */
/*!
  Setup all required files/paths for site mods to the image.  This should be
  called before performing any bindmounts or other discovery of the user image.
  Any paths created here should then be ignored by all other setup function-
  ality; i.e., no bind-mounts over these locations/paths.

  The site-configured bind-mounts will also be performed here.

  \param username username for group-file filtering
  \param minNodeSpec nodelist specification
  \param udiConfig global configuration for udiRoot
  \return 0 for succss, 1 otherwise
*/
int prepareSiteModifications(const char *username, const char *minNodeSpec, UdiRootConfig *udiConfig) {

    /* construct path to "live" copy of the image. */
    char udiRoot[PATH_MAX];
    char mntBuffer[PATH_MAX];
    char srcBuffer[PATH_MAX];
    char **volPtr = NULL;
    const char **fnamePtr = NULL;
    int ret = 0;
    struct stat statData;
    MountList mountCache;

    const char *mandatorySiteEtcFiles[4] = {
        "passwd", "group", "nsswitch.conf", NULL
    };
    const char *copyLocalEtcFiles[3] = {
        "hosts", "resolv.conf", NULL
    };

    memset(&mountCache, 0, sizeof(MountList));
    snprintf(udiRoot, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;
    if (chdir(udiRoot) != 0) {
        fprintf(stderr, "FAILED to chdir to %s. Exiting.\n", udiRoot);
        return 1;
    }

    /* get list of current mounts for this namespace */
    if (parse_MountList(&mountCache) != 0) {
        fprintf(stderr, "FAILED to get list of current mount points\n");
        return 1;
    }

    /* create all the directories needed for initial setup */
#define _MKDIR(dir, perm) if (mkdir(dir, perm) != 0) { \
    fprintf(stderr, "FAILED to mkdir %s. Exiting.\n", dir); \
    ret = 1; \
    goto _prepSiteMod_unclean; \
}
#define _BINDMOUNT(mountCache, from, to, ro, overwrite) if (_bindMount(mountCache, from, to, ro, overwrite) != 0) { \
    fprintf(stderr, "BIND MOUNT FAILED from %s to %s\n", from, to); \
    ret = 1; \
    goto _prepSiteMod_unclean; \
}

    _MKDIR("etc", 0755);
    _MKDIR("etc/local", 0755);
    _MKDIR("etc/udiImage", 0755);
    _MKDIR("opt", 0755);
    _MKDIR("opt/udiImage", 0755);
    _MKDIR("var", 0755);
    _MKDIR("var/spool", 0755);
    _MKDIR("var/run", 0755);
    _MKDIR("proc", 0755);
    _MKDIR("sys", 0755);
    _MKDIR("dev", 0755);
    _MKDIR("tmp", 0777);

    /* run site-defined pre-mount procedure */
    if (strlen(udiConfig->sitePreMountHook) > 0) {
        char *args[] = {
            strdup("/bin/sh"), strdup(udiConfig->sitePreMountHook), NULL
        };
        char **argsPtr = NULL;
        int ret = forkAndExecvp(args);
        for (argsPtr = args; *argsPtr != NULL; argsPtr++) {
            free(*argsPtr);
        }
        if (ret != 0) {
            fprintf(stderr, "Site premount hook failed. Exiting.\n");
            ret = 1;
            goto _prepSiteMod_unclean;
        }
    }

    /* do site-defined mount activities */
    for (volPtr = udiConfig->siteFs; *volPtr != NULL; volPtr++) {
        char to_buffer[PATH_MAX];
        snprintf(to_buffer, PATH_MAX, "%s/%s", udiRoot, *volPtr);
        to_buffer[PATH_MAX-1] = 0;
        _MKDIR(to_buffer, 0755);
        _BINDMOUNT(&mountCache, *volPtr, to_buffer, 0, 1);
    }

    /* run site-defined post-mount procedure */
    if (strlen(udiConfig->sitePostMountHook) > 0) {
        char *args[] = {
            strdup("/bin/sh"), strdup(udiConfig->sitePostMountHook), NULL
        };
        char **argsPtr = NULL;
        int ret = forkAndExecvp(args);
        for (argsPtr = args; *argsPtr != NULL; argsPtr++) {
            free(*argsPtr);
        }
        if (ret != 0) {
            fprintf(stderr, "Site premount hook failed. Exiting.\n");
            ret = 1;
            goto _prepSiteMod_unclean;
        }
    }

    /* setup site needs for /etc */
    snprintf(mntBuffer, PATH_MAX, "%s/etc/local", udiRoot);
    mntBuffer[PATH_MAX-1] = 0;
    _BINDMOUNT(&mountCache, "/etc", mntBuffer, 0, 1);

    /* copy needed local files */
    for (fnamePtr = copyLocalEtcFiles; *fnamePtr != NULL; fnamePtr++) {
        char source[PATH_MAX];
        char dest[PATH_MAX];
        snprintf(source, PATH_MAX, "%s/etc/local/%s", udiRoot, *fnamePtr);
        snprintf(dest, PATH_MAX, "%s/etc/%s", udiRoot, *fnamePtr);
        source[PATH_MAX - 1] = 0;
        dest[PATH_MAX - 1] = 0;
        if (_copyFile(source, dest, 1, 0, 0, 0644) != 0) {
            fprintf(stderr, "Failed to copy %s to %s\n", source, dest);
            goto _prepSiteMod_unclean;
        }
    }


    /* validate that the mandatorySiteEtcFiles do not exist yet */
    for (fnamePtr = mandatorySiteEtcFiles; *fnamePtr != NULL; fnamePtr++) {
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/etc/%s", udiRoot, *fnamePtr);
        if (stat(path, &statData) == 0) {
            fprintf(stderr, "%s already exists! ALERT!\n", path);
            goto _prepSiteMod_unclean;
        }
    }

    /* --> loop over everything in site etc-files and copy into image etc */
    snprintf(srcBuffer, PATH_MAX, "%s/%s", udiConfig->nodeContextPrefix, udiConfig->etcPath);
    srcBuffer[PATH_MAX-1] = 0;
    memset(&statData, 0, sizeof(struct stat));
    if (stat(srcBuffer, &statData) == 0) {
        DIR *etcDir = opendir(srcBuffer);
        struct dirent *entry = NULL;
        while ((entry = readdir(etcDir)) != NULL) {
            char *filename = NULL;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            filename = userInputPathFilter(entry->d_name, 0);
            if (filename == NULL) {
                fprintf(stderr, "FAILED to allocate filename string.\n");
                goto _prepSiteMod_unclean;
            }
            snprintf(srcBuffer, PATH_MAX, "%s/%s/%s", udiConfig->nodeContextPrefix, udiConfig->etcPath, filename);
            srcBuffer[PATH_MAX-1] = 0;
            snprintf(mntBuffer, PATH_MAX, "%s/etc/%s", udiRoot, filename);
            mntBuffer[PATH_MAX-1] = 0;
            free(filename);

            if (lstat(srcBuffer, &statData) != 0) {
                fprintf(stderr, "Couldn't find source file, check if there are illegal characters: %s\n", srcBuffer);
                goto _prepSiteMod_unclean;
            }

            if (lstat(mntBuffer, &statData) == 0) {
                fprintf(stderr, "Couldn't copy %s because file already exists.\n", mntBuffer);
                goto _prepSiteMod_unclean;
            } else {
                char *args[] = { strdup("cp"), strdup("-p"), strdup(srcBuffer), strdup(mntBuffer), NULL };
                char **argsPtr = NULL;
                int ret = forkAndExecvp(args);
                for (argsPtr = args; *argsPtr != NULL; argsPtr++) {
                    free(*argsPtr);
                }
                if (ret != 0) {
                    fprintf(stderr, "Failed to copy %s to %s.\n", srcBuffer, mntBuffer);
                    goto _prepSiteMod_unclean;
                }
            }
        }
        closedir(etcDir);
    } else {
        fprintf(stderr, "Couldn't stat udiRoot etc dir: %s\n", srcBuffer);
        ret = 1;
        goto _prepSiteMod_unclean;
    }

    /* validate that the mandatorySiteEtcFiles now exist */
    for (fnamePtr = mandatorySiteEtcFiles; *fnamePtr != NULL; fnamePtr++) {
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/etc/%s", udiRoot, *fnamePtr);
        if (stat(path, &statData) != 0) {
            fprintf(stderr, "%s does not exist! ALERT!\n", path);
            goto _prepSiteMod_unclean;
        }
        if (chown(path, 0, 0) != 0) {
            fprintf(stderr, "failed to chown %s to userid 0\n", path);
            goto _prepSiteMod_unclean;
        }
        if (chmod(path, 0644) != 0) {
            fprintf(stderr, "failed to chmod %s to 0644\n", path);
            goto _prepSiteMod_unclean;
        }
    }

    /* filter the group file */
    {
        char *mvArgs[5];
        char **argsPtr = NULL;
        char fromGroupFile[PATH_MAX];
        char toGroupFile[PATH_MAX];
        snprintf(fromGroupFile, PATH_MAX, "%s/etc/group", udiRoot);
        snprintf(toGroupFile, PATH_MAX, "%s/etc/group.orig", udiRoot);
        fromGroupFile[PATH_MAX - 1] = 0;
        toGroupFile[PATH_MAX - 1] = 0;
        mvArgs[0] = strdup("mv");
        mvArgs[1] = strdup(fromGroupFile);
        mvArgs[2] = strdup(toGroupFile);
        mvArgs[3] = NULL;
        ret = forkAndExecvp(mvArgs);
        for (argsPtr = mvArgs; *argsPtr != NULL; argsPtr++) {
            free(*argsPtr);
        }
        if (ret != 0) {
            fprintf(stderr, "Failed to rename %s to %s\n", fromGroupFile, toGroupFile);
            goto _prepSiteMod_unclean;
        }

        if (filterEtcGroup(fromGroupFile, toGroupFile, username) != 0) {
            fprintf(stderr, "Failed to filter group file %s\n", fromGroupFile);
            goto _prepSiteMod_unclean;
        }
    }

    /* recursively copy /opt/udiImage (to allow modifications) */
    if (udiConfig->optUdiImage != NULL) {
        snprintf(srcBuffer, PATH_MAX, "%s/%s/", udiConfig->nodeContextPrefix, udiConfig->optUdiImage);
        srcBuffer[PATH_MAX-1] = 0;
        if (stat(srcBuffer, &statData) != 0) {
            fprintf(stderr, "FAILED to stat udiImage source directory: %s\n", srcBuffer);
            goto _prepSiteMod_unclean;
        }
        snprintf(mntBuffer, PATH_MAX, "%s/opt", udiRoot);
        mntBuffer[PATH_MAX-1] = 0;

        if (stat(mntBuffer, &statData) != 0) {
            fprintf(stderr, "FAILED to stat udiImage target directory: %s\n", mntBuffer);
            goto _prepSiteMod_unclean;
        } else {
            char *args[] = {strdup("cp"), strdup("-rp"),
                strdup(srcBuffer), strdup(mntBuffer), NULL
            };
            char *chmodArgs[] = {strdup("chmod"), strdup("-R"),
                strdup("a+rX"), strdup(mntBuffer), NULL
            };
            char **argsPtr = NULL;
            int ret = forkAndExecvp(args);
            if (ret == 0) {
                ret = forkAndExecvp(chmodArgs);
                if (ret != 0) {
                    fprintf(stderr, "FAILED to fix permissions on %s.\n", mntBuffer);
                }
            } else {
                fprintf(stderr, "FAILED to copy %s to %s.\n", srcBuffer, mntBuffer);
            }
            for (argsPtr = args; *argsPtr != NULL; argsPtr++) free(*argsPtr);
            for (argsPtr = chmodArgs; *argsPtr != NULL; argsPtr++) free(*argsPtr);
            if (ret != 0) {
                goto _prepSiteMod_unclean;
            }
        }
    }

    /* setup hostlist for current allocation 
       format of minNodeSpec is "host1/16 host2/16" for 16 copies each of host1 and host2 */
    if (minNodeSpec != NULL) {
        if (writeHostFile(minNodeSpec, udiConfig) != 0) {
            fprintf(stderr, "FAILED to write out hostsfile\n");
            goto _prepSiteMod_unclean;
        }
    }

    /***** setup linux needs ******/
    /* mount /proc */
    snprintf(mntBuffer, PATH_MAX, "%s/proc", udiRoot);
    mntBuffer[PATH_MAX-1] = 0;
    if (mount(NULL, mntBuffer, "proc", MS_NOSUID|MS_NOEXEC|MS_NODEV, NULL) != 0) {
        fprintf(stderr, "FAILED to mount /proc\n");
        ret = 1;
        goto _prepSiteMod_unclean;
    }

    /* mount /sys */
    snprintf(mntBuffer, PATH_MAX, "%s/sys", udiRoot);
    mntBuffer[PATH_MAX-1] = 0;
    _BINDMOUNT(&mountCache, "/sys", mntBuffer, 0, 1);

    /* mount /dev */
    snprintf(mntBuffer, PATH_MAX, "%s/dev", udiRoot);
    mntBuffer[PATH_MAX-1] = 0;
    _BINDMOUNT(&mountCache, "/dev", mntBuffer, 0, 1);

    /* mount /tmp */
    snprintf(mntBuffer, PATH_MAX, "%s/tmp", udiRoot);
    mntBuffer[PATH_MAX-1] = 0;
    _BINDMOUNT(&mountCache, "/tmp", mntBuffer, 0, 1);

    /* mount any mount points under /dev */
    /*
    for (mntPtr = mountCache; *mntPtr != NULL; mntPtr++) {
        if (strncmp(*mntPtr, "/dev/", 4) == 0) {
            snprintf(mntBuffer, PATH_MAX, "%s/%s", udiRoot, *mntPtr);
            mntBuffer[PATH_MAX-1] = 0;
            _BINDMOUNT(mountCache, *mntPtr, mntBuffer, 0);
        }
    }
    */

#undef _MKDIR
#undef _BINDMOUNT

    return 0;
_prepSiteMod_unclean:
    free_MountList(&mountCache, 0);
    destructUDI(udiConfig, 0);
    return ret;
}

/*! Write out hostsfile into image */
/*!
 * Writes out an MPI-style hostsfile, e.g., one element per task.
 *     nid00001
 *     nid00001
 *     nid00002
 *     nid00002
 * For a two node/four task job.
 * Written to /var/hostsfile within the image.
 *
 * \param minNodeSpec string formatted like "nid00001/2 nid00002/2" for above
 * \param udiConfig UDI configuration object
 *
 * \returns 0 upon success, 1 upon failure
 */
int writeHostFile(const char *minNodeSpec, UdiRootConfig *udiConfig) {
    char *minNode = NULL;
    char *sptr = NULL;
    char *eptr = NULL;
    char *limit = NULL;
    FILE *fp = NULL;
    char filename[PATH_MAX];
    int idx = 0;

    if (minNodeSpec == NULL || udiConfig == NULL) return 1;

    minNode = strdup(minNodeSpec);
    limit = minNode + strlen(minNode);

    snprintf(filename, PATH_MAX, "%s%s/var/hostsfile",  udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    filename[PATH_MAX-1] = 0;

    fp = fopen(filename, "w");
    if (fp == NULL) {
        fprintf(stderr, "FAILED to open hostsfile for writing: %s\n", filename);
        goto _writeHostFile_error;
    }

    sptr = minNode;
    while (sptr < limit) {
        /* find hostname */
        char *hostname = sptr;
        int count = 0;
        eptr = strchr(sptr, '/');
        if (eptr == NULL) {
            /* parse error, should be hostname/number (e.g., nid00001/24) */
            fprintf(stderr, "FAILED to identify hostname when writing hosts list\n");
            goto _writeHostFile_error;
        }
        *eptr = 0;

        /* find count */
        sptr = eptr + 1;
        eptr = strchr(sptr, ' ');
        if (eptr == NULL) eptr = sptr + strlen(sptr);
        *eptr = 0;
        count = atoi(sptr);
        if (count == 0) {
            /* parse error, not a number */
            goto _writeHostFile_error;
        }

        /* write out */
        for (idx = 0; idx < count; idx++) {
            fprintf(fp, "%s\n", hostname);
        }

        sptr = eptr + 1;
    }
    fclose(fp);
    fp = NULL;
    free(minNode);
    minNode = NULL;
    return 0;

_writeHostFile_error:
    if (minNode != NULL) {
        free(minNode);
    }
    if (fp != NULL) {
        fclose(fp);
    }
    return 1;
}

int mountImageVFS(ImageData *imageData, const char *username, const char *minNodeSpec, UdiRootConfig *udiConfig) {
    struct stat statData;
    char udiRoot[PATH_MAX];

#define _MKDIR(dir, perm) if (mkdir(dir, perm) != 0) { \
    fprintf(stderr, "FAILED to mkdir %s. Exiting.\n", dir); \
    goto _mountImgVfs_unclean; \
}

    snprintf(udiRoot, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);

    if (stat(udiRoot, &statData) != 0) {
        _MKDIR(udiRoot, 0755);
    }

#undef _MKDIR
#define BIND_IMAGE_INTO_UDI(subtree, img, udiConfig, copyFlag) \
    if (bindImageIntoUDI(subtree, img, udiConfig, copyFlag) > 1) { \
        fprintf(stderr, "FAILED To setup \"%s\" in %s\n", subtree, udiRoot); \
        goto _mountImgVfs_unclean; \
    }

    /* mount a new rootfs to work in */
    if (mount(NULL, udiRoot, ROOTFS_TYPE, MS_NOSUID|MS_NODEV, NULL) != 0) {
        fprintf(stderr, "FAILED to mount rootfs on %s\n", udiRoot);
        perror("   --- REASON: ");
        goto _mountImgVfs_unclean;
    }
    if (chmod(udiRoot, 0755) != 0) {
        fprintf(stderr, "FAILED to chmod \"%s\" to 0755.\n", udiRoot);
        goto _mountImgVfs_unclean;
    }

    /* get our needs injected first */
    if (prepareSiteModifications(username, minNodeSpec, udiConfig) != 0) {
        fprintf(stderr, "FAILED to properly setup site modifications\n");
        goto _mountImgVfs_unclean;
    }

    /* copy/bind mount pieces into prepared site */
    BIND_IMAGE_INTO_UDI("/", imageData, udiConfig, 0);
    BIND_IMAGE_INTO_UDI("/var", imageData, udiConfig, 0);
    BIND_IMAGE_INTO_UDI("/opt", imageData, udiConfig, 0);

    /* setup sshd configuration */

    /* copy image /etc into place */
    BIND_IMAGE_INTO_UDI("/etc", imageData, udiConfig, 1);

#undef BIND_IMAGE_INTO_UDI
    return 0;

_mountImgVfs_unclean:
    /* do needed unmounts */
    return 1;
}

int mountImageLoop(ImageData *imageData, UdiRootConfig *udiConfig) {
    char loopMount[PATH_MAX];
    char imagePath[PATH_MAX];
    char mountExec[PATH_MAX];
    struct stat statData;
    if (imageData == NULL || udiConfig == NULL) {
        return 1;
    }
    if (imageData->useLoopMount == 0) {
        return 0;
    }
    if (udiConfig->loopMountPoint == NULL || strlen(udiConfig->loopMountPoint) == 0) {
        return 1;
    }
#define MKDIR(dir, perm) if (mkdir(dir, perm) != 0) { \
    fprintf(stderr, "FAILED to mkdir %s. Exiting.\n", dir); \
    goto _mntImgLoop_unclean; \
}
#define LOADKMOD(name, path) if (loadKernelModule(name, path, udiConfig) != 0) { \
    fprintf(stderr, "FAILED to load %s kernel module.\n", name); \
    goto _mntImgLoop_unclean; \
}
#define LOOPMOUNT(mountExec, from, to, imgtype) { \
    char *args[] = {strdup(mountExec), \
        strdup("-o"), strdup("loop,autoclear,ro,nosuid,nodev"), \
        strdup(from), strdup(to), \
        NULL}; \
    char **argsPtr = NULL; \
    int ret = forkAndExecvp(args); \
    for (argsPtr = args; *argsPtr != NULL; argsPtr++) { \
        free(*argsPtr); \
    } \
    if (ret != 0) { \
        fprintf(stderr, "FAILED to mount image %s (%s) on %s\n", from, imgtype, to); \
        goto _mntImgLoop_unclean; \
    } \
}

    if (stat(udiConfig->loopMountPoint, &statData) != 0) {
        MKDIR(udiConfig->loopMountPoint, 0755);
    }
    snprintf(loopMount, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->loopMountPoint);
    loopMount[PATH_MAX-1] = 0;
    snprintf(imagePath, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, imageData->filename);
    imagePath[PATH_MAX-1] = 0;
    snprintf(mountExec, PATH_MAX, "%s%s/sbin/mount", udiConfig->nodeContextPrefix, udiConfig->udiRootPath);

    if (stat(mountExec, &statData) != 0) {
        fprintf(stderr, "udiRoot mount executable missing: %s\n", mountExec);
        goto _mntImgLoop_unclean;
    } else if (statData.st_uid != 0 || statData.st_mode & S_IWGRP || statData.st_mode & S_IWOTH || !(statData.st_mode & S_IXUSR)) {
        fprintf(stderr, "udiRoot mount has incorrect ownership or permissions: %s\n", mountExec);
        goto _mntImgLoop_unclean;
    }

    if (stat("/dev/loop0", &statData) != 0) {
        LOADKMOD("loop", "drivers/block/loop.ko");
    }
    if (imageData->format == FORMAT_EXT4) {
        LOADKMOD("mbcache", "fs/mbcache.ko");
        LOADKMOD("jbd2", "fs/jbd2/jbd2.ko");
        LOADKMOD("ext4", "fs/ext4/ext4.ko");
        LOOPMOUNT(mountExec, imagePath, loopMount, "ext4");
    } else if (imageData->format == FORMAT_SQUASHFS) {
        LOADKMOD("squashfs", "fs/squashfs/squashfs.ko");
        LOOPMOUNT(mountExec, imagePath, loopMount, "squashfs");
    } else if (imageData->format == FORMAT_CRAMFS) {
        LOADKMOD("cramfs", "fs/cramfs/cramfs.ko");
        LOOPMOUNT(mountExec,imagePath, loopMount, "cramfs");
    } else {
        fprintf(stderr, "ERROR: unknown image format.\n");
        goto _mntImgLoop_unclean;
    }

#undef LOADKMOD
#undef MKDIR
#undef LOOPMOUNT
    return 0;
_mntImgLoop_unclean:
    return 1;
} 

int setupUserMounts(ImageData *imageData, VolumeMap *map, UdiRootConfig *udiConfig) {
    char **from = NULL;
    char **to = NULL;
    char **flags = NULL;
    char *filtered_from = NULL;
    char *filtered_to = NULL;
    char *filtered_flags = NULL;
    char from_buffer[PATH_MAX];
    char to_buffer[PATH_MAX];
    char udiRoot[PATH_MAX];
    struct stat statData;
    int ro = 0;
    MountList mountCache;

    if (imageData == NULL || udiConfig == NULL) {
        return 1;
    }
    if (map == NULL || map->n == 0) {
        return 0;
    }

#define _BINDMOUNT(mountCache, from, to, ro, overwrite) if (_bindMount(mountCache, from, to, ro, overwrite) != 0) { \
    fprintf(stderr, "BIND MOUNT FAILED from %s to %s\n", from, to); \
    goto _setupUserMounts_unclean; \
}

    memset(&mountCache, 0, sizeof(MountList));
    if (parse_MountList(&mountCache) != 0) {
        return 1;
    }

    snprintf(udiRoot, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;

    from = map->from;
    to = map->to;
    flags = map->flags;
    for (; *from && *to; from++, to++, flags++) {
        filtered_from = userInputPathFilter(*from, 1);
        filtered_to =   userInputPathFilter(*to, 1);
        filtered_flags = userInputPathFilter(*flags, 1);
        snprintf(from_buffer, PATH_MAX, "%s/%s", udiConfig->nodeContextPrefix, filtered_from);
        from_buffer[PATH_MAX-1] = 0;
        snprintf(to_buffer, PATH_MAX, "%s/%s", udiRoot, filtered_to);
        to_buffer[PATH_MAX-1] = 0;

        if (stat(from_buffer, &statData) != 0) {
            fprintf(stderr, "FAILED to find user volume from: %s\n", from_buffer);
            goto _setupUserMounts_unclean;
        }
        if (!S_ISDIR(statData.st_mode)) {
            fprintf(stderr, "FAILED from location is not directory: %s\n", from_buffer);
            goto _setupUserMounts_unclean;
        }
        if (stat(to_buffer, &statData) != 0) {
            fprintf(stderr, "FAILED to find user volume to: %s\n", to_buffer);
            goto _setupUserMounts_unclean;
        }
        if (!S_ISDIR(statData.st_mode)) {
            fprintf(stderr, "FAILED to location is not directory: %s\n", to_buffer);
            goto _setupUserMounts_unclean;
        }
        if (validateVolumeMap(filtered_from, filtered_to, filtered_flags) != 0) {
            fprintf(stderr, "FAILED illegal user-requested mount: %s %s %s\n", filtered_from, filtered_to, filtered_flags);
            goto _setupUserMounts_unclean;
        }
        ro = 0;
        if (strcmp(filtered_flags, "ro") == 0) {
            ro = 1;
        }
        _BINDMOUNT(&mountCache, from_buffer, to_buffer, ro, 1);
    }

#undef _BINDMOUNT
    free_MountList(&mountCache, 0);
    return 0;

_setupUserMounts_unclean:
    if (filtered_from != NULL) {
        free(filtered_from);
    }
    if (filtered_to != NULL) {
        free(filtered_to);
    }
    if (filtered_flags != NULL) {
        free(filtered_flags);
    }
    free_MountList(&mountCache, 0);
    return 1;
}

char *generateShifterConfigString(const char *user, ImageData *image, VolumeMap *volumeMap) {
    char *str = NULL;
    char *volMapSig = NULL;
    if (image == NULL || volumeMap == NULL) return NULL;

    volMapSig = getVolMapSignature(volumeMap);

    str = alloc_strgenf(
            "{\"identifier\":\"%s\","
            "\"user\":\"%s\","
            "\"volMap\":\"%s\"}",
            image->identifier,
            user,
            (volMapSig == NULL ? "" : volMapSig)
    );

    if (volMapSig != NULL) {
        free(volMapSig);
    }
    return str;
}

int saveShifterConfig(const char *user, ImageData *image, VolumeMap *volumeMap, UdiRootConfig *udiConfig) {
    char saveFilename[PATH_MAX];
    FILE *fp = NULL;
    char *configString = generateShifterConfigString(user, image, volumeMap);

    if (configString == NULL) {
        goto _saveShifterConfig_error;
    }

    snprintf(saveFilename, PATH_MAX, "%s%s/var/shifterConfig.json",
            udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    fp = fopen(saveFilename, "w");
    if (fp == NULL) {
        goto _saveShifterConfig_error;
    }
    fprintf(fp, "%s\n", configString);
    fclose(fp);
    fp = NULL;

    free(configString);
    configString = NULL;

    return 0;
_saveShifterConfig_error:
    if (fp != NULL) {
        fclose(fp);
    }
    if (configString != NULL) {
        free(configString);
    }
    return 1;
}

int compareShifterConfig(const char *user, ImageData *image, VolumeMap *volumeMap, UdiRootConfig *udiConfig) {
    char configFilename[PATH_MAX];
    FILE *fp = NULL;
    char *configString = generateShifterConfigString(user, image, volumeMap);
    char *buffer = NULL;
    size_t len = 0;
    size_t nread = 0;
    int cmpVal = 0;

    if (configString == NULL) {
        goto _compareShifterConfig_error;
    }
    len = strlen(configString);
    buffer = (char *) malloc(sizeof(char) * len);

    snprintf(configFilename, PATH_MAX, "%s%s/var/shifterConfig.json",
            udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);

    fp = fopen(configFilename, "r");
    if (fp == NULL) {
        goto _compareShifterConfig_error;
    }

    nread = fread(buffer, sizeof(char), len, fp);
    fclose(fp);
    fp = NULL;

    if (nread == len) {
        cmpVal = memcmp(configString, buffer, sizeof(char) * len);
    } else { 
        cmpVal = -1;
    }

    free(configString);
    configString = NULL;

    free(buffer);
    buffer = NULL;

    return cmpVal;
_compareShifterConfig_error:
    if (fp != NULL) {
        fclose(fp);
    }
    if (configString != NULL) {
        free(configString);
    }
    if (buffer != NULL) {
        free(buffer);
    }
    return -1;
}

int setupImageSsh(char *sshPubKey, char *username, uid_t uid, UdiRootConfig *udiConfig) {
    struct stat statData;
    char udiImage[PATH_MAX];
    char sshdConfigPath[PATH_MAX];
    char sshdConfigPathNew[PATH_MAX];
    const char *keyType[5] = {"dsa", "ecdsa", "rsa","ed25519", NULL};
    const char **keyPtr = NULL;
    char *lineBuf = NULL;
    size_t lineBuf_size = 0;

    FILE *inputFile = NULL;
    FILE *outputFile = NULL;

    MountList mountCache;
    memset(&mountCache, 0, sizeof(MountList));

#define _BINDMOUNT(mounts, from, to, ro, overwrite) if (_bindMount(mounts, from, to, ro, overwrite) != 0) { \
    fprintf(stderr, "BIND MOUNT FAILED from %s to %s\n", from, to); \
    goto _setupImageSsh_unclean; \
}

    if (parse_MountList(&mountCache) != 0) {
        fprintf(stderr, "FAILED to parse existing mounts\n");
        goto _setupImageSsh_unclean;
    }

    snprintf(udiImage, PATH_MAX, "%s%s/opt/udiImage", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    udiImage[PATH_MAX-1] = 0;
    if (stat(udiImage, &statData) != 0) {
        fprintf(stderr, "FAILED to find udiImage path, cannot setup sshd\n");
        goto _setupImageSsh_unclean;
    }
    if (chdir(udiImage) != 0) {
        fprintf(stderr, "FAILED to chdir to %s\n", udiImage);
        goto _setupImageSsh_unclean;
    }

    /* generate ssh host keys */
    for (keyPtr = keyType; *keyPtr != NULL; keyPtr++) {
        char keygenExec[PATH_MAX];
        char keyFileName[PATH_MAX];
        char *args[8];
        char **argPtr = NULL;
        int ret = 0;

        snprintf(keygenExec, PATH_MAX, "%s/bin/ssh-keygen", udiImage);
        keygenExec[PATH_MAX-1] = 0;
        snprintf(keyFileName, PATH_MAX, "%s/etc/ssh_host_%s_key", udiImage, *keyPtr);
        args[0] = strdup(keygenExec);
        args[1] = strdup("-t");
        args[2] = strdup(*keyPtr);
        args[3] = strdup("-f");
        args[4] = strdup(keyFileName);
        args[5] = strdup("-N");
        args[6] = strdup("");
        args[7] = NULL;

        ret = forkAndExecv(args);
        for (argPtr = args; *argPtr != NULL; argPtr++) {
            free(*argPtr);
        }

        if (ret != 0) {
            fprintf(stderr, "Failed to generate key of type %s\n", *keyPtr);
            goto _setupImageSsh_unclean;
        }
    }

    /* rewrite sshd_config */
    snprintf(sshdConfigPath, PATH_MAX, "%s/etc/sshd_config", udiImage);
    sshdConfigPath[PATH_MAX - 1] = 0;
    snprintf(sshdConfigPathNew, PATH_MAX, "%s/etc/sshd_config.new", udiImage);
    sshdConfigPathNew[PATH_MAX - 1] = 0;

    inputFile = fopen(sshdConfigPath, "r");
    outputFile = fopen(sshdConfigPathNew, "w");

    if (inputFile == NULL || outputFile == NULL) {
        fprintf(stderr, "Could not open sshd_config for reading or writing\n");
        goto _setupImageSsh_unclean;
    }

    while (!feof(inputFile) && !ferror(inputFile)) {
        size_t nbytes = getline(&lineBuf, &lineBuf_size, inputFile);
        char *ptr = NULL;
        if (nbytes == 0) break;
        ptr = shifter_trim(lineBuf);
        if (strcmp(ptr, "AllowUsers ToBeReplaced") == 0) {
            fprintf(outputFile, "AllowUsers %s\n", username);
        } else {
            fprintf(outputFile, "%s\n", ptr);
        }
    }
    fclose(inputFile);
    fclose(outputFile);
    inputFile = NULL;
    outputFile = NULL;
    if (lineBuf != NULL) {
        free(lineBuf);
        lineBuf = NULL;
    }
    if (stat(sshdConfigPathNew, &statData) != 0) {
        fprintf(stderr, "FAILED to find new sshd_config file, cannot setup sshd\n");
        goto _setupImageSsh_unclean;
    } else {
        char *moveCmd[] = { strdup("mv"),
            strdup(sshdConfigPathNew),
            strdup(sshdConfigPath),
            NULL
        };
        char **argsPtr = NULL;
        int ret = forkAndExecvp(moveCmd);
        for (argsPtr = moveCmd; *argsPtr != NULL; argsPtr++) {
            free(*argsPtr);
        }
        if (ret != 0) {
            fprintf(stderr, "FAILED to replace sshd_config with configured version.\n");
            goto _setupImageSsh_unclean;
        }
    }
    chown(sshdConfigPath, 0, 0);
    chmod(sshdConfigPath, S_IRUSR);

    if (sshPubKey != NULL && strlen(sshPubKey) > 0) {
        char buffer[PATH_MAX];
        snprintf(buffer, PATH_MAX, "%s/etc/user_auth_keys", udiImage);
        buffer[PATH_MAX - 1] = 0;
        outputFile = fopen(buffer, "w");
        if (outputFile == NULL) {
            fprintf(stderr, "FAILED to open user_auth_keys for writing\n");
            goto _setupImageSsh_unclean;
        }
        fprintf(outputFile, "%s\n", sshPubKey);
        fclose(outputFile);
        outputFile = NULL;
        chown(buffer, uid, 0);
        chmod(buffer, S_IRUSR); /* user read only */

    }

    {
        char from[PATH_MAX];
        char to[PATH_MAX];

        snprintf(from, PATH_MAX, "%s/bin/ssh", udiImage);
        snprintf(to, PATH_MAX, "%s%s/usr/bin/ssh", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
        from[PATH_MAX-1] = 0;
        to[PATH_MAX-1] = 0;
        if (stat(to, &statData) == 0) {
            _BINDMOUNT(&mountCache, from, to, 1, 1);
        }
        snprintf(from, PATH_MAX, "%s/bin/ssh", udiImage);
        snprintf(to, PATH_MAX, "%s%s/bin/ssh", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
        from[PATH_MAX - 1] = 0;
        to[PATH_MAX - 1] = 0;
        if (stat(to, &statData) == 0) {
            _BINDMOUNT(&mountCache, from, to, 1, 1);
        }
        snprintf(from, PATH_MAX, "%s/etc/ssh_config", udiImage);
        snprintf(to, PATH_MAX, "%s%s/etc/ssh/ssh_config", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
        if (stat(to, &statData) == 0) {
            char *args[] = {strdup("cp"),
                strdup("-p"),
                strdup(from),
                strdup(to),
                NULL
            };
            char **argsPtr = NULL;
            int ret = forkAndExecvp(args);
            for (argsPtr = args; *argsPtr != NULL; argsPtr++) {
                free(*argsPtr);
            }
            if (ret == 0) {
                fprintf(stderr, "FAILED to copy ssh_config to %s\n", to);
                goto _setupImageSsh_unclean;
            }
        }

        snprintf(to, PATH_MAX, "%s%s/var/empty", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
        if (mkdir(to, 0700) != 0) {
            fprintf(stderr, "FAILED to create /var/empty\n");
            goto _setupImageSsh_unclean;
        }
    }
#undef _BINDMOUNT
    return 0;
_setupImageSsh_unclean:
    if (inputFile != NULL) {
        fclose(inputFile);
        inputFile = NULL;
    }
    if (outputFile != NULL) {
        fclose(outputFile);
        outputFile = NULL;
    }
    if (lineBuf != NULL) {
        free(lineBuf);
        lineBuf = NULL;
    }
    return 1;
}

/**
  * startSshd
  * chroots into image and runs the secured sshd
  */
int startSshd(UdiRootConfig *udiConfig) {
    char chrootPath[PATH_MAX];
    pid_t pid = 0;

    snprintf(chrootPath, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    chrootPath[PATH_MAX - 1] = 0;

    if (chdir(chrootPath) != 0) {
        fprintf(stderr, "FAILED to chdir to %s while attempted to start sshd\n", chrootPath);
        goto _startSshd_unclean;
    }

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "FAILED to fork while attempting to start sshd\n");
        goto _startSshd_unclean;
    }
    if (pid == 0) {
        if (chroot(chrootPath) != 0) {
            fprintf(stderr, "FAILED to chroot to %s while attempting to start sshd\n", chrootPath);
            /* no goto, this is the child, we want it to end if this failed */
        } else  {
            char *sshdArgs[2] = {
                strdup("/opt/udiImage/sbin/sshd"),
                NULL
            };
            execv(sshdArgs[0], sshdArgs);
            fprintf(stderr, "FAILED to exec sshd!\n");

        }
        /* if we fell through to here, there is a problem */
        exit(1);
    } else {
        int status = 0;
        do {
            pid_t ret = waitpid(pid, &status, 0);
            if (ret != pid) {
                fprintf(stderr, "waited for wrong pid: %d != %d\n", pid, ret);
                goto _startSshd_unclean;
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
        } else {
            status = 1;
        }
        return status;
    }
    return 0;
_startSshd_unclean:
    return 1;
}

int forkAndExecvp(char *const *args) {
    pid_t pid = 0;

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "FAILED to fork! Exiting.\n");
        return 1;
    }
    if (pid > 0) {
        /* this is the parent */
        int status = 0;
        do {
            pid_t ret = waitpid(pid, &status, 0);
            if (ret != pid) {
                fprintf(stderr, "This might be impossible: forked by couldn't wait, FAIL!\n");
                return 1;
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
        } else {
            status = 1;
        }
        return status;
    }
    /* this is the child */
    execvp(args[0], args);
    fprintf(stderr, "FAILED to execvp! Exiting.\n");
    exit(1);
    return 1;
}

int forkAndExecv(char *const *args) {
    pid_t pid = 0;

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "FAILED to fork! Exiting.\n");
        return 1;
    }
    if (pid > 0) {
        /* this is the parent */
        int status = 0;
        do {
            pid_t ret = waitpid(pid, &status, 0);
            if (ret != pid) {
                fprintf(stderr, "This might be impossible: forked by couldn't wait, FAIL!\n");
                return 1;
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
        } else {
            status = 1;
        }
        return status;
    }
    /* this is the child */
    execv(args[0], args);
    fprintf(stderr, "FAILED to execvp! Exiting.\n");
    exit(1);
    return 1;
}

static int _bindMount(MountList *mountCache, const char *from, const char *to, int ro, int overwriteMounts) {
    int ret = 0;
    char **ptr = NULL;
    char *to_real = NULL;
    unsigned long mountFlags = MS_BIND;
    unsigned long remountFlags = MS_REMOUNT|MS_BIND|MS_NOSUID;

    if (from == NULL || to == NULL || mountCache == NULL) {
        fprintf(stderr, "INVALID input to bind-mount. Fail\n");
        return 1;
    }

    to_real = realpath(to, NULL);
    if (to_real == NULL) {
        fprintf(stderr, "Couldn't lookup path %s, fail.\n", to);
        return 1;
    }

    /* not interested in mounting over existing mounts, prevents 
       things from getting into a weird state later. */
    ptr = find_MountList(mountCache, to_real);
    if (ptr != NULL) {
        if (overwriteMounts) {
            if (unmountTree(mountCache, to_real) != 0) {
                fprintf(stderr, "%s was already mounted, failed to unmount existing, fail.\n", to_real);
                ret = 1;
                goto _bindMount_exit;
            }
        } else {
            fprintf(stderr, "%s was already mounted, not allowed to unmount existingm fail.\n", to_real);
            ret = 1;
            goto _bindMount_exit;
        }
    }

    if (strcmp(from, "/dev") == 0) {
        mountFlags |= MS_REC;
        remountFlags |= MS_REC;
    }

    /* perform the initial bind-mount */
    ret = mount(from, to, "bind", mountFlags, NULL);
    if (ret != 0) {
        goto _bindMount_unclean;
    }
    insert_MountList(mountCache, to);

    /* if the source is exactly /dev or starts with /dev/ then 
       ALLOW device entires, otherwise remount with noDev */
    if (strcmp(from, "/dev") != 0 && strncmp(from, "/dev/", 5) != 0) {
        remountFlags |= MS_NODEV;
    }

    /* if readonly is requested, then do it */
    if (ro) {
        remountFlags |= MS_RDONLY;
    }

    /* remount the bind-mount to get the needed mount flags */
    ret = mount(from, to, "bind", remountFlags, NULL);
    if (ret != 0) {
        goto _bindMount_unclean;
    }
_bindMount_exit:
    if (to_real != NULL) {
        free(to_real);
        to_real = NULL;
    }
    return ret;
_bindMount_unclean:
    if (to_real != NULL) {
        ret = umount2(to_real, UMOUNT_NOFOLLOW|MNT_DETACH);
        remove_MountList(mountCache, to_real);
        free(to_real);
        to_real = NULL;
    } else {
        ret = umount2(to, UMOUNT_NOFOLLOW|MNT_DETACH);
        remove_MountList(mountCache, to);
    }
    if (ret != 0) {
        fprintf(stderr, "ERROR: unclean exit from bind-mount routine. %s may still be mounted.\n", to);
    }
    return 1;
}

char *userInputPathFilter(const char *input, int allowSlash) {
    ssize_t len = 0;
    char *ret = NULL;
    const char *rptr = NULL;
    char *wptr = NULL;
    if (input == NULL) return NULL;

    len = strlen(input) + 1;
    ret = (char *) malloc(sizeof(char) * len);
    if (ret == NULL) return NULL;

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

/**
 * isSharedMount
 * Determine if the string " shared:" exists in /proc/<pid>/mountinfo for the
 * specified mountpoint.
 *
 * \param mountPoint the mountpoint to consider
 *
 * Returns:
 * 0: not shared
 * 1: shared
 * -1: error
 */
int isSharedMount(const char *mountPoint) {
    char filename[PATH_MAX];
    char *lineBuffer = NULL;
    size_t lineBuffer_size = 0;
    FILE *fp = NULL;
    pid_t pid = getpid();
    int rc = 0;

    snprintf(filename, PATH_MAX, "/proc/%d/mountinfo", pid);
    fp = fopen(filename, "r");

    if (fp == NULL) return -1;
    while (!feof(fp) && !ferror(fp)) {
        char *ptr = NULL;
        char *svptr = NULL;
        size_t n = getline(&lineBuffer, &lineBuffer_size, fp);
        if (n == 0 || feof(fp) || ferror(fp)) {
            break;
        }
        ptr = strtok_r(lineBuffer, " ", &svptr);
        ptr = strtok_r(NULL, " ", &svptr);
        ptr = strtok_r(NULL, " ", &svptr);
        ptr = strtok_r(NULL, " ", &svptr);
        ptr = strtok_r(NULL, " ", &svptr);

        if (strcmp(ptr, mountPoint) == 0) {
            ptr = strtok_r(NULL, "\0", &svptr); /* get rest of line */
            if (strstr(ptr, " shared:") != NULL) {
                rc = 1;
            }
            break;
        }
    }
    fclose(fp);
    if (lineBuffer != NULL) {
        free(lineBuffer);
    }

    return rc;
}

/*! Loads a kernel module if required */
/*!
 * Checks to see if the specified kernel module is already loaded, if so, does
 * nothing.  Otherwise, will try to load the module using modprobe (i.e., from 
 * the system /lib paths.  Finally, will attempt to load the from the shifter-
 * specific store installed with Shifter
 *
 * \param name name of kernel module
 * \param path path to kernel module within shifter structure
 * \param udiConfig UDI configuration structure
 */
int loadKernelModule(const char *name, const char *path, UdiRootConfig *udiConfig) {
    char kmodPath[PATH_MAX];
    FILE *fp = NULL;
    char *lineBuffer = NULL;
    size_t lineSize = 0;
    ssize_t nread = 0;
    struct stat statData;
    int loaded = 0;
    int ret = 0;

    if (name == NULL || strlen(name) == 0 || path == NULL || strlen(path) == 0 || udiConfig == NULL) {
        return 1;
    }

    fp = fopen("/proc/modules", "r");
    if (fp == NULL) {
        return 1;
    }
    while (!feof(fp) && !ferror(fp)) {
        char *ptr = NULL;
        char *svptr = NULL;
        nread = getline(&lineBuffer, &lineSize, fp);
        if (nread == 0 || feof(fp) || ferror(fp)) {
            break;
        }
        ptr = strtok_r(lineBuffer, " ", &svptr);
        if (strcmp(name, ptr) == 0) {
            loaded = 1;
            break;
        }
    }
    fclose(fp);
    if (lineBuffer != NULL) {
        free(lineBuffer);
        lineBuffer = NULL;
    }

    if (loaded) {
        return 0;
    }

    snprintf(kmodPath, PATH_MAX, "%s%s/%s", udiConfig->nodeContextPrefix, udiConfig->kmodPath, path);
    kmodPath[PATH_MAX-1] = 0;

    if (stat(kmodPath, &statData) == 0) {
        char *insmodArgs[] = {
            strdup("insmod"), 
            strdup(kmodPath), 
            NULL
        };
        char **argPtr = NULL;

        /* run insmod and clean up */
        ret = forkAndExecvp(insmodArgs);
        for (argPtr = insmodArgs; *argPtr != NULL; argPtr++) {
            free(*argPtr);
        }

        if (ret != 0) {
            fprintf(stderr, "FAILED to load kernel module %s (%s)\n", name, kmodPath);
            goto _loadKrnlMod_unclean;
        }
    } else {
        fprintf(stderr, "FAILED to find kernel modules %s (%s)\n", name, kmodPath);
        goto _loadKrnlMod_unclean;
    }
    return ret;
_loadKrnlMod_unclean:
    if (lineBuffer != NULL) {
        free(lineBuffer);
    }
    return ret;
}

/** filterEtcGroup
 *  many implementations of initgroups() do not deal with huge /etc/group 
 *  files due to a variety of limitations (like per-line limits).  this 
 *  function reads a given etcgroup file and filters the content to only
 *  include the specified user
 *
 *  \param group_dest_fname filename of filtered group file
 *  \param group_source_fname filename of to-be-filtered group file
 *  \param username user to identify
 */
int filterEtcGroup(const char *group_dest_fname, const char *group_source_fname, const char *username) {
    FILE *input = NULL;
    FILE *output = NULL;
    char *linePtr = NULL;
    size_t linePtr_size = 0;
    char *group_name = NULL;

    if (group_dest_fname == NULL || strlen(group_dest_fname) == 0
            || group_source_fname == NULL || strlen(group_source_fname) == 0
            || username == NULL || strlen(username) == 0) {
        fprintf(stderr, "Invalid arguments, cannot filter group file.\n");
        goto _filterEtcGroup_unclean;
    }

    input = fopen(group_source_fname, "r");
    output = fopen(group_dest_fname, "w");

    if (input == NULL || output == NULL) {
        fprintf(stderr, "Failed to open files, cannot filter group file.\n");
        goto _filterEtcGroup_unclean;
    }

    while (!feof(input) && !ferror(input)) {
        size_t nread = getline(&linePtr, &linePtr_size, input);
        char *ptr = NULL;
        char *svptr = NULL;

        char *token = NULL;
        gid_t gid = 0;
        size_t counter = 0;
        int foundUsername = 0;
        if (nread == 0) break;
        ptr = shifter_trim(linePtr);
        for (token = strtok_r(ptr, ":,", &svptr);
             token != NULL;
             token = strtok_r(NULL, ":,", &svptr)) {

            switch (counter) {
                case 0: group_name = strdup(token);
                        if (strcmp(group_name, username) == 0) {
                            foundUsername = 1;
                        }
                        break;
                case 1: break;
                case 2: gid = strtoul(token, NULL, 10);
                        break;
                default: if (strcmp(username, token) == 0) {
                            foundUsername = 1;
                        }
                        break;
            }
            counter++;
            if (foundUsername && gid != 0) break;
        }
        if (group_name != NULL && foundUsername == 1) {
            fprintf(output, "%s:x:%d:%s\n", group_name, gid, username);
        }
        if (group_name != NULL) {
            free(group_name);
            group_name = NULL;
        }
    }
    fclose(input);
    fclose(output);
    input = NULL;
    output = NULL;

    if (linePtr != NULL) {
        free(linePtr);
    }
    return 0;
_filterEtcGroup_unclean:
    if (input != NULL) {
        fclose(input);
        input = NULL;
    }
    if (output != NULL) {
        fclose(output);
        output = NULL;
    }
    if (linePtr != NULL) {
        free(linePtr);
        linePtr = NULL;
    }
    if (group_name != NULL) {
        free(group_name);
        group_name = NULL;
    }
    return 1;
}

int killSshd(void) {
    FILE *psOutput = popen("ps -eo pid,command", "r");
    char *linePtr = NULL;
    size_t linePtr_size = 0;
    if (psOutput == NULL) {
        fprintf(stderr, "FAILED to run ps to find sshd\n");
        goto _killSshd_unclean;
    }
    while (!feof(psOutput) && !ferror(psOutput)) {
        size_t nRead = getline(&linePtr, &linePtr_size, psOutput);
        char *ptr = NULL;
        char *command = NULL;
        if (nRead == 0) break;
        ptr = shifter_trim(linePtr);
        command = strchr(ptr, ' ');
        if (command != NULL) {
            *command++ = 0;
            if (strcmp(command, "/opt/udiImage/sbin/sshd") == 0) {
                pid_t pid = strtoul(ptr, NULL, 10);
                if (pid != 0) kill(pid, SIGTERM);
            }
        }
    }
    pclose(psOutput);
    psOutput = NULL;
    if (linePtr != NULL) {
        free(linePtr);
        linePtr = NULL;
    }
    return 0;
_killSshd_unclean:
    if (psOutput != NULL) {
        pclose(psOutput);
        psOutput = NULL;
    }
    if (linePtr != NULL) {
        free(linePtr);
        linePtr = NULL;
    }
    return 1;
}

/**
 * destructUDI
 * Unmounts all aspects of the UDI, possibly killing the sshd running first.
 * This is called in the unsetupRoot program when dealing with the global
 * linux namespace.  It should also be called when trying to get rid of an
 * existing UDI before setting up a new one in a private linux namespace.
 *
 * \param udiConfig configuration
 * \param killSsh flag to denote whether or not the udiRoot sshd should be killed
 *     (1 for yes, 0 for no)
 */
int destructUDI(UdiRootConfig *udiConfig, int killSsh) {
    char udiRoot[PATH_MAX];
    char loopMount[PATH_MAX];
    MountList mounts;
    size_t idx = 0;
    int rc = 1; /* assume failure */

    if (parse_MountList(&mounts) != 0) {
        /*error*/
    }
    snprintf(udiRoot, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;
    snprintf(loopMount, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->loopMountPoint);
    loopMount[PATH_MAX-1] = 0;
    for (idx = 0; idx < 10; idx++) {

        if (idx > 0) usleep(300);
        if (killSsh == 1) killSshd();

        if (unmountTree(&mounts, udiRoot) != 0) {
            continue;
        }
        if (unmountTree(&mounts, loopMount) != 0) {
            continue;
        }
        rc = 0; /* mark success */
        break;
    }
    free_MountList(&mounts, 0);
    return rc;
}

/**
 * unmountTree
 * Unmount everything under a particular base path.  Uses a MountList assumed
 * to be up-to-date with the current mount state of the process namespace.
 * unmountTree will remove any and all unmounted paths from the MountList.
 * unmountTree will change the sort order of the MountList, and will try
 * to restore it upon success or failure of the unmount operations.
 * unmountTree will try to unmount all paths inclusive and under a given base
 * path.  e.g., if base is "/a", then "/a", "/a/b", and "/a/b/c" will all be
 * unmounted.  These are done in reverse alphabetic order, meaning that 
 * "/a/b/c" will be unmounted first, then "/a/b", then "/a".
 * The first error encountered will stop all unmounts.
 *
 * \param mounts pointer to up-to-date MountList
 * \param base basepath to look for for unmounts
 *
 * Returns:
 * 0 of all possible paths were unmounted
 * 1 if some or none were unmounted
 */
int unmountTree(MountList *mounts, const char *base) {
    MountList mountCache;
    size_t baseLen = 0;
    char **ptr = NULL;
    MountListSortOrder origSorted = MOUNT_SORT_FORWARD;
    int rc = 0;

    if (mounts == NULL || base == NULL) return 1;

    memset(&mountCache, 0, sizeof(MountList));
    baseLen = strlen(base);
    if (baseLen == 0) return 1;

    origSorted = mounts->sorted;
    setSort_MountList(mounts, MOUNT_SORT_REVERSE);

    for (ptr = mounts->mountPointList; ptr && *ptr; ptr++) {
        if (strncmp(*ptr, base, baseLen) == 0) {
            rc = umount2(*ptr, UMOUNT_NOFOLLOW|MNT_DETACH);
/*
            char **argPtr = NULL;
            char *args[] = {
                strdup("umount"),
                strdup(*ptr), 
                NULL
            };
            rc = forkAndExecvp(args);
            for (argPtr = args; *argPtr != NULL; argPtr++) {
                free(*argPtr);
            }
*/
            if (rc != 0) {
                goto _unmountTree_exit;
            }
            insert_MountList(&mountCache, *ptr);
        }
    }
_unmountTree_exit:
    for (ptr = mountCache.mountPointList; ptr && *ptr; ptr++) {
        remove_MountList(mounts, *ptr);
    }
    free_MountList(&mountCache, 0);
    setSort_MountList(mounts, origSorted);
    return rc;
}

#ifdef _TESTHARNESS_SHIFTERCORE
#include <CppUTest/CommandLineTestRunner.h>
#ifdef NOTROOT
#define ISROOT 0
#else
#define ISROOT 1
#endif

TEST_GROUP(ShifterCoreTestGroup) {
    void setup() {
        bool isRoot = getuid() == 0;
        bool macroIsRoot = ISROOT == 1;
        if (isRoot && !macroIsRoot) {
            fprintf(stderr, "WARNING: the bulk of the functional tests are"
                    " disabled because the test suite is compiled with "
                    "-DNOTROOT, but could have run since you have root "
                    "privileges.");
        } else if (!isRoot && macroIsRoot) {
            fprintf(stderr, "WARNING: the test suite is built to run root-"
                    "privileged tests, but you don't have those privileges."
                    " Several tests will fail.");
        }
    }
};

TEST(ShifterCoreTestGroup, CopyFile_basic) {
    char buffer[PATH_MAX];
    char *ptr = NULL;
    int ret = 0;
    struct stat statData;
    
    getcwd(buffer, PATH_MAX);
    ptr = buffer + strlen(buffer);
    snprintf(ptr, PATH_MAX - (ptr - buffer), "/passwd");

    ret = _copyFile(NULL, buffer, 0, INVALID_USER, INVALID_GROUP, 0644);
    CHECK(ret != 0)
    ret = _copyFile("/etc/passwd", NULL, 0, INVALID_USER, INVALID_GROUP, 0644);
    CHECK(ret != 0)

    ret = _copyFile("/etc/passwd", buffer, 0, INVALID_USER, INVALID_GROUP, 0644);
    CHECK(ret == 0)

    ret = lstat(buffer, &statData);
    CHECK(ret == 0)
    CHECK((statData.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == 0644)

    ret = unlink(buffer);
    CHECK(ret == 0)

    ret = _copyFile("/etc/passwd", buffer, 0, INVALID_USER, INVALID_GROUP, 0755);
    CHECK(ret == 0)

    ret = lstat(buffer, &statData);
    CHECK(ret == 0)
    CHECK((statData.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == 0755)

    ret = unlink(buffer);
    CHECK(ret == 0)
}

#ifdef NOTROOT
IGNORE_TEST(ShifterCoreTestGroup, CopyFile_chown) {
#else
TEST(ShifterCoreTestGroup, CopyFile_chown) {
#endif
    char buffer[PATH_MAX];
    char *ptr = NULL;
    int ret = 0;
    struct stat statData;
    
    getcwd(buffer, PATH_MAX);
    ptr = buffer + strlen(buffer);
    snprintf(ptr, PATH_MAX - (ptr - buffer), "/passwd");

    ret = _copyFile("/etc/passwd", buffer, 0, 2, 2, 0644);
    CHECK(ret == 0)

    ret = lstat(buffer, &statData);
    CHECK(ret == 0)
    CHECK((statData.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == 0644)
    CHECK(statData.st_uid == 2)
    CHECK(statData.st_gid == 2)

    ret = unlink(buffer);
    CHECK(ret == 0)

    ret = _copyFile("/etc/passwd", buffer, 0, 2, 2, 0755);
    CHECK(ret == 0)

    ret = lstat(buffer, &statData);
    CHECK(ret == 0)
    CHECK((statData.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == 0755)
    CHECK(statData.st_uid == 2)
    CHECK(statData.st_gid == 2)

    ret = unlink(buffer);
    CHECK(ret == 0)
}

#ifdef NOTROOT
IGNORE_TEST(ShifterCoreTestGroup, isSharedMount_basic) {
#else
TEST(ShifterCoreTestGroup, isSharedMount_basic) {
#endif
    CHECK(unshare(CLONE_NEWNS) == 0);
    CHECK(mount(NULL, "/", NULL, MS_SHARED, NULL) == 0);

    CHECK(isSharedMount("/") == 1);

    CHECK(mount(NULL, "/", NULL, MS_PRIVATE, NULL) == 0);
    CHECK(isSharedMount("/") == 0);
}

#ifdef NOTROOT
IGNORE_TEST(ShifterCoreTestGroup, validatePrivateNamespace) {
#else
TEST(ShifterCoreTestGroup, validatePrivateNamespace) {
#endif
    struct stat statInfo;
    pid_t child = 0;

    CHECK(stat("/tmp", &statInfo) == 0);
    CHECK(stat("/tmp/test_shifter_core", &statInfo) != 0);
    
    child = fork();
    if (child == 0) {
        char currDir[PATH_MAX];
        struct stat localStatInfo;

        printf("about to unshare\n");
        if (unshare(CLONE_NEWNS) != 0) exit(1);
        if (isSharedMount("/") == 1) {
            printf("about to make private mount\n");
            if (mount(NULL, "/", NULL, MS_PRIVATE|MS_REC, NULL) != 0) exit(1);
        }
        printf("about to getcwd\n");
        if (getcwd(currDir, PATH_MAX) == NULL) exit(1);
        currDir[PATH_MAX - 1] = 0;

        printf("about to bind %s to /tmp\n", currDir);
        if (mount(currDir, "/tmp", NULL, MS_BIND, NULL) != 0) exit(1);
        printf("about to stat /tmp/test_shifter_core\n");
        if (stat("/tmp/test_shifter_core", &localStatInfo) == 0) {
            exit(0);
        }
        exit(1);
    } else if (child > 0) {
        int status = 0;
        waitpid(child, &status, 0);

        status = WEXITSTATUS(status);
        CHECK(status == 0);

        CHECK(stat("/tmp/test_shifter_core", &statInfo) != 0);
    }
}

TEST(ShifterCoreTestGroup, userInputPathFilter_basic) {
    char *filtered = userInputPathFilter("benign", 0);
    CHECK(strcmp(filtered, "benign") == 0);
    free(filtered);
    
    filtered = userInputPathFilter("benign; rm -rf *", 0);
    CHECK(strcmp(filtered, "benignrm-rf") == 0);
    free(filtered);

    filtered = userInputPathFilter("/path/to/something/great", 0);
    CHECK(strcmp(filtered, "pathtosomethinggreat") == 0);
    free(filtered);

    filtered = userInputPathFilter("/path/to/something/great", 1);
    CHECK(strcmp(filtered, "/path/to/something/great") == 0);
    free(filtered);
}

TEST(ShifterCoreTestGroup, writeHostFile_basic) { 
   char tmpDir[] = "/tmp/shifter.XXXXXX"; 
   char tmpDirVar[] = "/tmp/shifter.XXXXXX/var";
   char hostsFilename[] = "/tmp/shifter.XXXXXX/var/hostsfile";
   FILE *fp = NULL;
   char *linePtr = NULL;
   size_t linePtr_size = 0;
   int count = 0;

   CHECK(mkdtemp(tmpDir) != NULL);
   memcpy(tmpDirVar, tmpDir, sizeof(char) * strlen(tmpDir));
   memcpy(hostsFilename, tmpDir, sizeof(char) * strlen(tmpDir));
   CHECK(mkdir(tmpDirVar, 0755) == 0);

   UdiRootConfig config;
   memset(&config, 0, sizeof(UdiRootConfig));

   config.nodeContextPrefix = strdup("");
   config.udiMountPoint = tmpDir;

   int ret = writeHostFile("host1/4", &config);
   CHECK(ret == 0);

   fp = fopen(hostsFilename, "r");
   while (fp && !feof(fp) && !ferror(fp)) {
       size_t nread = getline(&linePtr, &linePtr_size, fp);
       if (nread == 0 || feof(fp) || ferror(fp)) break;

       if (count < 4) {
           CHECK(strcmp(linePtr, "host1\n") == 0);
           count++;
           continue;
       }
       FAIL("Got beyond where we should be");
   }
   fclose(fp);

   ret = writeHostFile(NULL, &config);
   CHECK(ret == 1);

   ret = writeHostFile("host1 host2", &config);
   CHECK(ret == 1);

   ret = writeHostFile("host1/24 host2/24 host3/24", &config);
   count = 0;
   fp = fopen(hostsFilename, "r");
   while (fp && !feof(fp) && !ferror(fp)) {
       size_t nread = getline(&linePtr, &linePtr_size, fp);
       if (nread == 0 || feof(fp) || ferror(fp)) break;

       if (count < 24) {
           CHECK(strcmp(linePtr, "host1\n") == 0);
           count++;
           continue;
       } else if (count < 48) {
           CHECK(strcmp(linePtr, "host2\n") == 0);
           count++;
           continue;
       } else if (count < 72) {
           CHECK(strcmp(linePtr, "host3\n") == 0);
           count++;
           continue;
       }
       FAIL("Got beyond where we should be");
   }
   fclose(fp);

   unlink(hostsFilename);
   rmdir(tmpDirVar);
   rmdir(tmpDir);

   if (linePtr != NULL) free(linePtr);
   free(config.nodeContextPrefix);
}

int main(int argc, char** argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

#endif
