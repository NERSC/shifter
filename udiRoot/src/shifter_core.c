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


#define _GNU_SOURCE
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

#ifndef ROOTFS_TYPE
#define ROOTFS_TYPE "tmpfs"
#endif



static int _bindMount(char **mountCache, const char *from, const char *to, int ro);
static int _sortFsForward(const void *, const void *);
static int _sortFsReverse(const void *, const void *);
static int _copyFile(const char *source, const char *dest, int keepLink, uid_t owner, gid_t group, mode_t mode);
static char *_filterString(char *);

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

    char **mountCache = NULL;
    size_t n_mountCache = 0;

    if (relpath == NULL || strlen(relpath) == 0 || imageData == NULL ||
            udiConfig == NULL)
    {
        return 1;
    }

#define MKDIR(dir, perm) if (mkdir(dir, perm) != 0) { \
    fprintf(stderr, "FAILED to mkdir %s. Exiting.\n", dir); \
    goto _bindImgUDI_unclean; \
}
#define BINDMOUNT(mountCache, from, to, ro) if (_bindMount(mountCache, from, to, ro) != 0) { \
    fprintf(stderr, "BIND MOUNT FAILED from %s to %s\n", from, to); \
    goto _bindImgUDI_unclean; \
}

    memset(&statData, 0, sizeof(struct stat));

    mountCache = parseMounts(&n_mountCache);
    if (mountCache == NULL) {
        fprintf(stderr, "FAILED to read existing mounts.\n");
        return 1;
    }
    qsort(mountCache, n_mountCache, sizeof(char*), _sortFsForward);

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
        fprintf(stderr, "FAILED to opendir %s\n", srcBuffer);
        goto _bindImgUDI_unclean;
    }
    while ((dirEntry = readdir(subtree)) != NULL) {
        if (strcmp(dirEntry->d_name, ".") == 0 || strcmp(dirEntry->d_name, "..") == 0) {
            continue;
        }
        itemname = _filterString(dirEntry->d_name);
        if (itemname == NULL) {
            fprintf(stderr, "FAILED to correctly filter entry: %s\n", dirEntry->d_name);
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
            char *args[5] = { "cp", "-P", srcBuffer, mntBuffer, NULL };
            if (forkAndExecvp(args) != 0) {
                fprintf(stderr, "Failed to copy %s to %s.\n", srcBuffer, mntBuffer);
                goto _bindImgUDI_unclean;
            }
            free(itemname);
            continue;
        }
        if (S_ISREG(statData.st_mode)) {
            if (statData.st_size < FILE_SIZE_LIMIT) {
                char *args[5] = { "cp", "-p", srcBuffer, mntBuffer, NULL };
                if (forkAndExecvp(args) != 0) {
                    fprintf(stderr, "Failed to copy %s to %s.\n", srcBuffer, mntBuffer);
                    goto _bindImgUDI_unclean;
                }
            } else if (copyFlag == 0) {
                /* create the file */
                FILE *fp = fopen(mntBuffer, "w");
                fclose(fp);
                BINDMOUNT(mountCache, srcBuffer, mntBuffer, 0);
            }
            free(itemname);
            continue;
        }
        if (S_ISDIR(statData.st_mode)) {
            if (copyFlag == 0) {
                MKDIR(mntBuffer, 0755);
                BINDMOUNT(mountCache, srcBuffer, mntBuffer, 0);
            } else {
                char *args[5] = { "cp", "-rp", srcBuffer, mntBuffer, NULL };
                if (forkAndExecvp(args) != 0) {
                    fprintf(stderr, "Failed to copy %s to %s.\n", srcBuffer, mntBuffer);
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

    if (mountCache != NULL) {
        char **ptr = mountCache;
        for (ptr = mountCache; *ptr != NULL; ptr++) {
            free(*ptr);
        }
        free(mountCache);
        mountCache = NULL;
    }
    return 0;

_bindImgUDI_unclean:
    if (mountCache != NULL) {
        char **ptr = mountCache;
        for (ptr = mountCache; *ptr != NULL; ptr++) {
            free(*ptr);
        }
        free(mountCache);
        mountCache = NULL;
    }
    if (itemname != NULL) {
        free(itemname);
        itemname = NULL;
    }
    return 1;
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

    if (owner == INVALID_USER) owner = sourceStat.st_uid;
    if (group == INVALID_GROUP) group = sourceStat.st_gid;
    if (chown(dest, owner, group) != 0) {
        fprintf(stderr, "Failed to set ownership to %d:%d on %s\n", owner, group, dest);
        goto _copyFile_unclean;
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
    char **mountCache = NULL;
    char **mntPtr = NULL;
    char **fnamePtr = NULL;
    int ret = 0;
    size_t mountCache_cnt = 0;
    struct stat statData;

    char *mandatorySiteEtcFiles[4] = {
        "passwd", "group", "nsswitch.conf", NULL
    };
    char *copyLocalEtcFiles[3] = {
        "hosts", "resolv.conf", NULL
    };

    snprintf(udiRoot, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;
    if (chdir(udiRoot) != 0) {
        fprintf(stderr, "FAILED to chdir to %s. Exiting.\n", udiRoot);
        return 1;
    }

    /* get list of current mounts for this namespace */
    mountCache = parseMounts(&mountCache_cnt);
    if (mountCache == NULL) {
        fprintf(stderr, "FAILED to get list of current mount points\n");
        return 1;
    }
    qsort(mountCache, mountCache_cnt, sizeof(char*), _sortFsForward);

    /* create all the directories needed for initial setup */
#define _MKDIR(dir, perm) if (mkdir(dir, perm) != 0) { \
    fprintf(stderr, "FAILED to mkdir %s. Exiting.\n", dir); \
    ret = 1; \
    goto _prepSiteMod_unclean; \
}
#define _BINDMOUNT(mountCache, from, to, ro) if (_bindMount(mountCache, from, to, ro) != 0) { \
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
        char *args[3] = {
            "/bin/sh", udiConfig->sitePreMountHook, NULL
        };
        int ret = forkAndExecvp(args);
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
        _BINDMOUNT(mountCache, *volPtr, to_buffer, 0);
    }

    /* run site-defined post-mount procedure */
    if (strlen(udiConfig->sitePostMountHook) > 0) {
        char *args[3] = {
            "/bin/sh", udiConfig->sitePostMountHook, NULL
        };
        int ret = forkAndExecvp(args);
        if (ret != 0) {
            fprintf(stderr, "Site premount hook failed. Exiting.\n");
            ret = 1;
            goto _prepSiteMod_unclean;
        }
    }

    /* setup site needs for /etc */
    snprintf(mntBuffer, PATH_MAX, "%s/etc/local", udiRoot);
    mntBuffer[PATH_MAX-1] = 0;
    _BINDMOUNT(mountCache, "/etc", mntBuffer, 0);

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
            filename = _filterString(entry->d_name);
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
                char *args[5] = { "cp", "-p", srcBuffer, mntBuffer, NULL };
                if (forkAndExecvp(args) != 0) {
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
        char fromGroupFile[PATH_MAX];
        char toGroupFile[PATH_MAX];
        snprintf(fromGroupFile, PATH_MAX, "%s/etc/group", udiRoot);
        snprintf(toGroupFile, PATH_MAX, "%s/etc/group.orig", udiRoot);
        fromGroupFile[PATH_MAX - 1] = 0;
        toGroupFile[PATH_MAX - 1] = 0;
        mvArgs[0] = "mv";
        mvArgs[1] = fromGroupFile;
        mvArgs[2] = toGroupFile;
        mvArgs[3] = NULL;
        if (forkAndExecvp(mvArgs) != 0) {
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
            char *args[5] = {"cp", "-rp", srcBuffer, mntBuffer, NULL };
            char *chmodArgs[5] = {"chmod", "-R", "a+rX", mntBuffer, NULL};
            if (forkAndExecvp(args) != 0) {
                fprintf(stderr, "FAILED to copy %s to %s.\n", srcBuffer, mntBuffer);
                goto _prepSiteMod_unclean;
            }
            if (forkAndExecvp(chmodArgs) != 0) {
                fprintf(stderr, "FAILED to fix permissions on %s.\n", mntBuffer);
                goto _prepSiteMod_unclean;
            }
        }
    }

    /* setup hostlist for current allocation 
       format of minNodeSpec is "host1/16 host2/16" for 16 copies each of host1 and host2 */
    if (minNodeSpec) {
        char *minNode = strdup(minNodeSpec);
        char *search = minNode;
        char *token = NULL;
        FILE *fp = NULL;
        snprintf(mntBuffer, PATH_MAX, "%s/var/hostsfile", udiRoot);
        mntBuffer[PATH_MAX-1] = 0;
        fp = fopen(mntBuffer, "w");
        if (fp == NULL) {
            fprintf(stderr, "FAILED to open hostsfile for writing: %s\n", mntBuffer);
            goto _prepSiteMod_unclean;
        }
        while ((token = strtok(search, "/ ")) != NULL) {
            char *hostname = token;
            int count = 0;
            int i = 0;
            search = NULL;
            token = strtok(NULL, "/ ");
            if (token == NULL || ((count = atoi(token)) == 0)) {
                fprintf(stderr, "FAILED to parse minNodeSpec: %s\n", minNodeSpec);
                goto _prepSiteMod_unclean;
            }

            for (i = 0; i < count; i++) {
                fprintf(fp, "%s\n", hostname);
            }
        }
        fclose(fp);
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
    _BINDMOUNT(mountCache, "/sys", mntBuffer, 0);

    /* mount /dev */
    snprintf(mntBuffer, PATH_MAX, "%s/dev", udiRoot);
    mntBuffer[PATH_MAX-1] = 0;
    _BINDMOUNT(mountCache, "/dev", mntBuffer, 0);

    /* mount /tmp */
    snprintf(mntBuffer, PATH_MAX, "%s/tmp", udiRoot);
    mntBuffer[PATH_MAX-1] = 0;
    _BINDMOUNT(mountCache, "/tmp", mntBuffer, 0);

    /* mount any mount points under /dev */
    for (mntPtr = mountCache; *mntPtr != NULL; mntPtr++) {
        if (strncmp(*mntPtr, "/dev/", 4) == 0) {
            snprintf(mntBuffer, PATH_MAX, "%s/%s", udiRoot, *mntPtr);
            mntBuffer[PATH_MAX-1] = 0;
            _BINDMOUNT(mountCache, *mntPtr, mntBuffer, 0);
        }
    }

#undef _MKDIR
#undef _BINDMOUNT

    return 0;
_prepSiteMod_unclean:
    if (mountCache != NULL) {
        char **ptr = NULL;
        for (ptr = mountCache; *ptr != NULL; ptr++) {
            free(*ptr);
        }
        free(mountCache);
        mountCache = NULL;
    }
    destructUDI(udiConfig);
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
    if (bindImageIntoUDI(subtree, img, udiConfig, copyFlag) != 0) { \
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
    char *args[] = {mountExec, "-o", "loop,autoclear,ro,nosuid,nodev", from, to, NULL}; \
    if (forkAndExecvp(args) != 0) { \
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
    snprintf(mountExec, PATH_MAX, "%s/sbin/mount", udiConfig->udiRootPath);

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

int setupUserMounts(ImageData *imageData, char **volumeMapFrom, char **volumeMapTo, char **volumeMapFlags, UdiRootConfig *udiConfig) {
    char **from = NULL;
    char **to = NULL;
    char **flags = NULL;
    char *filtered_from = NULL;
    char *filtered_to = NULL;
    char from_buffer[PATH_MAX];
    char to_buffer[PATH_MAX];
    char udiRoot[PATH_MAX];
    struct stat statData;
    char **mountCache = NULL;
    size_t n_mountCache = 0;
    int ro = 0;

    if (imageData == NULL || udiConfig == NULL) {
        return 1;
    }
    if (volumeMapFrom == NULL) {
        return 0;
    }

#define _BINDMOUNT(mountCache, from, to, ro) if (_bindMount(mountCache, from, to, ro) != 0) { \
    fprintf(stderr, "BIND MOUNT FAILED from %s to %s\n", from, to); \
    goto _setupUserMounts_unclean; \
}

    mountCache = parseMounts(&n_mountCache);
    if (mountCache == NULL) {
        return 1;
    }

    snprintf(udiRoot, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;

    from = volumeMapFrom;
    to = volumeMapTo;
    flags = volumeMapFlags;
    for (; *from && *to; from++, to++, flags++) {
        filtered_from = _filterString(*from);
        filtered_to =   _filterString(*to);
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
        if (userMountFilter(udiRoot, filtered_from, filtered_to, *flags) != 0) {
            fprintf(stderr, "FAILED illegal user-requested mount: %s\n", filtered_to);
            goto _setupUserMounts_unclean;
        }
        ro = 0;
        if (flags && strcmp(*flags, "ro") == 0) {
            ro = 1;
        }
        _BINDMOUNT(mountCache, filtered_from, filtered_to, ro);
    }

#undef _BINDMOUNT
    if (mountCache != NULL) {
        char **ptr = mountCache;
        for (ptr = mountCache; *ptr != NULL; ptr++) {
            free(*ptr);
        }
        free(mountCache);
        mountCache = NULL;
    }
    return 0;

_setupUserMounts_unclean:
    if (filtered_from != NULL) {
        free(filtered_from);
    }
    if (filtered_to != NULL) {
        free(filtered_to);
    }
    if (mountCache != NULL) {
        char **ptr = mountCache;
        for (ptr = mountCache; *ptr != NULL; ptr++) {
            free(*ptr);
        }
        free(mountCache);
        mountCache = NULL;
    }
    return 1;
}

int setupImageSsh(ImageData *imageData, char *sshPubKey, char *username, uid_t uid, UdiRootConfig *udiConfig) {
    struct stat statData;
    char udiImage[PATH_MAX];
    char sshdConfigPath[PATH_MAX];
    char sshdConfigPathNew[PATH_MAX];
    char *keyType[5] = {"dsa", "ecdsa", "rsa","ed25519", NULL};
    char **keyPtr = NULL;
    char *lineBuf = NULL;
    size_t lineBuf_size = 0;

    FILE *inputFile = NULL;
    FILE *outputFile = NULL;

    size_t mountCache_cnt = 0;
    char **mountCache = parseMounts(&mountCache_cnt);

#define _BINDMOUNT(mountCache, from, to, ro) if (_bindMount(mountCache, from, to, ro) != 0) { \
    fprintf(stderr, "BIND MOUNT FAILED from %s to %s\n", from, to); \
    goto _setupImageSsh_unclean; \
}

    if (mountCache == NULL) {
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

        snprintf(keygenExec, PATH_MAX, "%s/bin/ssh-keygen", udiImage);
        keygenExec[PATH_MAX-1] = 0;
        snprintf(keyFileName, PATH_MAX, "%s/etc/ssh_host_%s_key", udiImage, *keyPtr);
        args[0] = keygenExec;
        args[1] = "-t";
        args[2] = *keyPtr;
        args[3] = "-f";
        args[4] = keyFileName;
        args[5] = "-N";
        args[6] = "";
        args[7] = NULL;
        if (forkAndExecv(args) != 0) {
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
        char *moveCmd[4] = { "mv", sshdConfigPathNew, sshdConfigPath, NULL };
        if (forkAndExecvp(moveCmd) != 0) {
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
            _BINDMOUNT(mountCache, from, to, 1);
        }
        snprintf(from, PATH_MAX, "%s/bin/ssh", udiImage);
        snprintf(to, PATH_MAX, "%s%s/bin/ssh", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
        from[PATH_MAX - 1] = 0;
        to[PATH_MAX - 1] = 0;
        if (stat(to, &statData) == 0) {
            _BINDMOUNT(mountCache, from, to, 1);
        }
        snprintf(from, PATH_MAX, "%s/etc/ssh_config", udiImage);
        snprintf(to, PATH_MAX, "%s%s/etc/ssh/ssh_config", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
        if (stat(to, &statData) == 0) {
            char *args[5] = {"cp", "-p", from, to, NULL};
            if (forkAndExecvp(args) != 0) {
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
    char udiRootPath[PATH_MAX];
    pid_t pid = 0;

    snprintf(udiRootPath, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    udiRootPath[PATH_MAX - 1] = 0;

    if (chdir(udiRootPath) != 0) {
        fprintf(stderr, "FAILED to chdir to %s while attempted to start sshd\n", udiRootPath);
        goto _startSshd_unclean;
    }

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "FAILED to fork while attempting to start sshd\n");
        goto _startSshd_unclean;
    }
    if (pid == 0) {
        char *args[2] = {"/opt/udiImage/sbin/sshd", NULL};
        if (chroot(udiRootPath) != 0) {
            fprintf(stderr, "FAILED to chroot to %s while attempting to start sshd\n", udiRootPath);
            goto _startSshd_unclean;
        }
        execv(args[0], args);
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

int forkAndExecvp(char **args) {
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
int forkAndExecv(char **args) {
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

static int _bindMount(char **mountCache, const char *from, const char *to, int ro) {
    int ret = 0;
    char **ptr = mountCache;
    char *to_real = NULL;
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

    /* not interesting in mounting over existing mounts, prevents 
       things from getting into a weird state later. */
    if (mountCache != NULL) {
        for (ptr = mountCache; *ptr; ptr++) {
            if (strcmp(*ptr, to_real) == 0) {
                fprintf(stderr, "%s is already mounted. Refuse to remount.  Fail.\n", to_real);
                ret = 1;
                goto _bindMount_exit;
            }
        }
    }

    /* perform the initial bind-mount */
    ret = mount(from, to, "bind", MS_BIND, NULL);
    if (ret != 0) {
        goto _bindMount_unclean;
    }

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
        ret = umount(to_real);
        free(to_real);
        to_real = NULL;
    } else {
        ret = umount(to);
    }
    if (ret != 0) {
        fprintf(stderr, "ERROR: unclean exit from bind-mount routine. %s may still be mounted.\n", to);
    }
    return 1;
}

static char *_filterString(char *input) {
    ssize_t len = 0;
    char *ret = NULL;
    char *rptr = NULL;
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
        rptr++;
    }
    *wptr = 0;
    return ret;
}

char **parseMounts(size_t *n_mounts) {
    pid_t pid = getpid();
    char fname_buffer[PATH_MAX];
    FILE *fp = NULL;
    char *lineBuffer = NULL;
    size_t lineBuffer_size = 0;
    ssize_t nRead = 0;

    char **ret = NULL;
    char **ret_ptr = NULL;
    size_t ret_capacity = 0;

    snprintf(fname_buffer, PATH_MAX, "/proc/%d/mounts", pid);
    fp = fopen(fname_buffer, "r");
    if (fp == NULL) {
        fprintf(stderr, "FAILED to open %s\n", fname_buffer);
        *n_mounts = 0;
        return NULL;
    }
    while (!feof(fp) && !ferror(fp)) {
        char *ptr = NULL;
        nRead = getline(&lineBuffer, &lineBuffer_size, fp);
        if (nRead == 0 || feof(fp) || ferror(fp)) {
            break;
        }
        /* want second space-seperated column */
        ptr = strtok(lineBuffer, " ");
        ptr = strtok(NULL, " ");

        if (ptr != NULL) {
            size_t curr_count = ret_ptr - ret;
            if (ret == NULL || (curr_count + 2) >= ret_capacity) {
                char **tRet = realloc(ret, (ret_capacity + MOUNT_ALLOC_BLOCK) * sizeof(char*));
                if (tRet == NULL) {
                    fprintf(stderr, "FAILED to allocate enough memory for mounts listing\n");
                    goto _parseMounts_errClean;
                }
                ret = tRet;
                ret_capacity += MOUNT_ALLOC_BLOCK;
                ret_ptr = tRet + curr_count;
            }
            *ret_ptr = strdup(ptr);
            ret_ptr++;
            *ret_ptr = NULL;
        }
    }
    fclose(fp);
    if (lineBuffer != NULL) {
        free(lineBuffer);
    }
    *n_mounts = ret_ptr - ret;
    return ret;
_parseMounts_errClean:
    if (lineBuffer != NULL) {
        free(lineBuffer);
    }
    if (ret != NULL) {
        for (ret_ptr = ret; *ret_ptr; ret_ptr++) {
            free(*ret_ptr);
        }
        free(ret);
    }
    *n_mounts = 0;
    return NULL;
}

static int _sortFsForward(const void *ta, const void *tb) {
    const char **a = (const char **) ta;
    const char **b = (const char **) tb;

    return strcmp(*a, *b);
}

static int _sortFsReverse(const void *ta, const void *tb) {
    const char **a = (const char **) ta;
    const char **b = (const char **) tb;

    return -1 * strcmp(*a, *b);
}

int userMountFilter(char *udiRoot, char *filtered_from, char *filtered_to, char *flags) {
    const char *disallowedPaths[] = {"/etc", "/opt", "/opt/udiImage", "/var", NULL};
    const char **strPtr = NULL;
    char mntBuffer[PATH_MAX];

    char *to_real = NULL;
    char *req_real = NULL;

    if (filtered_from == NULL || filtered_to == NULL) {
        return 1;
    }
    if (flags != NULL) {
        if (strcmp(flags, "ro") != 0) {
            return 1;
        }
    }
    req_real = realpath(filtered_to, NULL);
    if (req_real == NULL) {
        fprintf(stderr, "ERROR: unable to determine real path for %s\n", filtered_to);
        goto _userMntFilter_unclean;
    }
    for (strPtr = disallowedPaths; *strPtr; strPtr++) {
        int len = 0;
        snprintf(mntBuffer, PATH_MAX, "%s%s", udiRoot, *strPtr);
        to_real = realpath(mntBuffer, NULL);
        if (to_real == NULL) {
            fprintf(stderr, "ERROR: unable to determine real path for %s\n", mntBuffer);
            goto _userMntFilter_unclean;
        }
        len = strlen(to_real);
        if (strncmp(to_real, req_real, len) == 0) {
            fprintf(stderr, "ERROR: user requested mount matches illegal pattern: %s matches %s\n", req_real, to_real);
            goto _userMntFilter_unclean;
        }
        free(to_real);
        to_real = NULL;
    }
    if (req_real != NULL) {
        free(req_real);
    }

    return 0;
_userMntFilter_unclean:
    if (to_real != NULL) {
        free(to_real);
    }
    if (req_real != NULL) {
        free(req_real);
    }
    return 1;
}

int loadKernelModule(const char *name, const char *path, UdiRootConfig *udiConfig) {
    char kmodPath[PATH_MAX];
    FILE *fp = NULL;
    char *lineBuffer = NULL;
    size_t lineSize = 0;
    ssize_t nread = 0;
    struct stat statData;
    int loaded = 0;

    if (name == NULL || strlen(name) == 0 || path == NULL || strlen(path) == 0 || udiConfig == NULL) {
        return 1;
    }

    fp = fopen("/proc/modules", "r");
    if (fp == NULL) {
        return 1;
    }
    while (!feof(fp) && !ferror(fp)) {
        char *ptr = NULL;
        nread = getline(&lineBuffer, &lineSize, fp);
        if (nread == 0 || feof(fp) || ferror(fp)) {
            break;
        }
        ptr = strtok(lineBuffer, " ");
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
        char *insmodArgs[] = {"insmod", kmodPath, NULL};
        if (forkAndExecvp(insmodArgs) != 0) {
            fprintf(stderr, "FAILED to load kernel module %s (%s)\n", name, kmodPath);
            goto _loadKrnlMod_unclean;
        }
    } else {
        fprintf(stderr, "FAILED to find kernel modules %s (%s)\n", name, kmodPath);
        goto _loadKrnlMod_unclean;
    }
    return 0;
_loadKrnlMod_unclean:
    if (lineBuffer != NULL) {
        free(lineBuffer);
    }
    return 1;
}

/** filterEtcGroup
 *  many implementations of initgroups() do not deal with huge /etc/group 
 *  files due to a variety of limitations (like per-line limits).  this 
 *  function reads a given etcgroup file and filters the content to only
 *  include the specified user
 *
 *  Parameters:
 *  group_dest_fname - filename of filtered group file
 *  group_source_fname - filename of to-be-filtered group file
 *  username - user to identify
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

        char *token = NULL;
        gid_t gid = 0;
        size_t counter = 0;
        int foundUsername = 0;
        if (nread == 0) break;
        ptr = shifter_trim(linePtr);
        for (token = strtok(ptr, ":,");
             token != NULL;
             token = strtok(NULL, ":,")) {

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

int killSshd() {
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

int destructUDI(UdiRootConfig *udiConfig) {
    char udiRoot[PATH_MAX];
    char loopMount[PATH_MAX];
    size_t mountCache_cnt = 0;
    char **mountCache = parseMounts(&mountCache_cnt);
    size_t idx = 0;
    snprintf(udiRoot, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;
    snprintf(loopMount, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->loopMountPoint);
    loopMount[PATH_MAX-1] = 0;
    for (idx = 0; idx < 10; idx++) {
        killSshd();
        if (mountCache != NULL && *mountCache != NULL) {
            size_t udiRoot_len = strlen(udiRoot);
            size_t loopMount_len = strlen(loopMount);
            char **ptr = NULL;

            if (idx > 0) usleep(300);

            qsort(mountCache, mountCache_cnt, sizeof(char*), _sortFsReverse);
            for (ptr = mountCache; *ptr != NULL; ptr++) {
                if (strncmp(*ptr, udiRoot, udiRoot_len) == 0) {
                    char *args[] = {"umount", *ptr, NULL};
                    int ret = forkAndExecvp(args);
                    printf("%d umounted: %s\n", ret, *ptr);
                } else if (strncmp(*ptr, loopMount, loopMount_len) == 0) {
                    char *args[] = {"umount", *ptr, NULL};
                    int ret = forkAndExecvp(args);
                    printf("%d loop umounted: %s\n", ret, *ptr);
                }
                free(*ptr);
            }
            free(mountCache);
            mountCache = NULL;
        } else {
            break;
        }
    }
    return 0;
}
