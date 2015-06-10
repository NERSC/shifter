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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>

#include "ImageData.h"
#include "UdiRootConfig.h"
#include "shifter_core.h"


static int _bindMount(char **mountCache, const char *from, const char *to, int ro);
static int _sortFsForward(const void *, const void *);
static int _sortFsReverse(const void *, const void *);
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

/*! Setup all required files/paths for site mods to the image */
/*!
  Setup all required files/paths for site mods to the image.  This should be
  called before performing any bindmounts or other discovery of the user image.
  Any paths created here should then be ignored by all other setup function-
  ality; i.e., no bind-mounts over these locations/paths.

  The site-configured bind-mounts will also be performed here.

  \param minNodeSpec nodelist specification
  \param udiConfig global configuration for udiRoot
  \return 0 for succss, 1 otherwise
*/
int prepareSiteModifications(const char *minNodeSpec, UdiRootConfig *udiConfig) {

    /* construct path to "live" copy of the image. */
    char udiRoot[PATH_MAX];
    char mntBuffer[PATH_MAX];
    char srcBuffer[PATH_MAX];
    char **volPtr = NULL;
    char **mountCache = NULL;
    char **mntPtr = NULL;
    int ret = 0;
    size_t mountCache_cnt = 0;
    struct stat statData;

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

    /* recursively copy /opt/udiImage (to allow modifications) */
    if (udiConfig->optUdiImage != NULL) {
        snprintf(srcBuffer, PATH_MAX, "%s/%s", udiConfig->nodeContextPrefix, udiConfig->optUdiImage);
        srcBuffer[PATH_MAX-1] = 0;
        if (stat(srcBuffer, &statData) != 0) {
            fprintf(stderr, "FAILED to stat udiImage source directory: %s\n", srcBuffer);
            goto _prepSiteMod_unclean;
        }
        snprintf(mntBuffer, PATH_MAX, "%s/opt/udiImage", udiRoot);
        mntBuffer[PATH_MAX-1] = 0;

        if (stat(mntBuffer, &statData) != 0) {
            fprintf(stderr, "FAILED to stat udiImage target directory: %s\n", mntBuffer);
            goto _prepSiteMod_unclean;
        } else {
            char *args[5] = {"cp", "-rp", srcBuffer, mntBuffer, NULL };
            if (forkAndExecvp(args) != 0) {
                fprintf(stderr, "FAILED to copy %s to %s.\n", srcBuffer, mntBuffer);
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

int mountImageVFS(ImageData *imageData, const char *minNodeSpec, UdiRootConfig *udiConfig) {
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
    if (mount(NULL, udiRoot, "rootfs", MS_NOSUID|MS_NODEV, NULL) != 0) {
        fprintf(stderr, "FAILED to mount rootfs on %s\n", udiRoot);
        goto _mountImgVfs_unclean;
    }

    /* get our needs injected first */
    if (prepareSiteModifications(minNodeSpec, udiConfig) != 0) {
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

    return 0;

_mountImgVfs_unclean:
    /* do needed unmounts */
    return 1;
}

int mountImageLoop(ImageData *imageData, UdiRootConfig *udiConfig) {
    char loopMount[PATH_MAX];
    char imagePath[PATH_MAX];
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
#define LOOPMOUNT(from, to, imgtype, flags) { \
    char *args[] = {"mount", "-o", "loop,autoclear,ro,nosuid,nodev", from, to, NULL}; \
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

    LOADKMOD("loop", "drivers/block/loop.ko");
    if (imageData->format == FORMAT_EXT4) {
        LOADKMOD("mbcache", "fs/mbcache.ko");
        LOADKMOD("jbd2", "fs/jbd2/jbd2.ko");
        LOADKMOD("ext4", "fs/ext4/ext4.ko");
        LOOPMOUNT(imagePath, loopMount, "ext4", MS_NOSUID|MS_NODEV|MS_RDONLY);
    } else if (imageData->format == FORMAT_SQUASHFS) {
        LOADKMOD("squashfs", "fs/squashfs/squashfs.ko");
        LOOPMOUNT(imagePath, loopMount, "squashfs", MS_NOSUID|MS_NODEV|MS_RDONLY);
    } else if (imageData->format == FORMAT_CRAMFS) {
        LOADKMOD("cramfs", "fs/cramfs/cramfs.ko");
        LOOPMOUNT(imagePath, loopMount, "cramfs", MS_NOSUID|MS_NODEV|MS_RDONLY);
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

int destructUDI(UdiRootConfig *udiConfig) {
    char udiRoot[PATH_MAX];
    char loopMount[PATH_MAX];
    size_t mountCache_cnt = 0;
    char **mountCache = parseMounts(&mountCache_cnt);
    snprintf(udiRoot, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;
    snprintf(loopMount, PATH_MAX, "%s%s", udiConfig->nodeContextPrefix, udiConfig->loopMountPoint);
    loopMount[PATH_MAX-1] = 0;
    if (mountCache != NULL) {
        size_t udiRoot_len = strlen(udiRoot);
        size_t loopMount_len = strlen(loopMount);
        char **ptr = NULL;

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
    }
    return 0;
}
