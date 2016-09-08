/** @file shifter_core.c
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
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/capability.h>

#include "ImageData.h"
#include "UdiRootConfig.h"
#include "shifter_core.h"
#include "utility.h"
#include "VolumeMap.h"
#include "MountList.h"
#include "config.h"

#ifndef BINDMOUNT_OVERWRITE_UNMOUNT_RETRY
#define BINDMOUNT_OVERWRITE_UNMOUNT_RETRY 3
#endif

#ifndef UMOUNT_NOFOLLOW
#define UMOUNT_NOFOLLOW 0x00000008 /* do not follow symlinks when unmounting */
#endif

int _shifterCore_bindMount(UdiRootConfig *confg, MountList *mounts,
        const char *from, const char *to, size_t flags, int overwrite);
int _shifterCore_copyFile(const char *cpPath, const char *source, const char *dest, int keepLink, uid_t owner, gid_t group, mode_t mode);

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
#define BINDMOUNT(mounts, from, to, ro, overwrite) if (_shifterCore_bindMount(udiConfig, mounts, from, to, ro, overwrite) != 0) { \
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
    snprintf(udiRoot, PATH_MAX, "%s", udiConfig->udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;

    if (imageData->useLoopMount) {
        snprintf(imgRoot, PATH_MAX, "%s", udiConfig->loopMountPoint);
        imgRoot[PATH_MAX-1] = 0;
    } else {
        snprintf(imgRoot, PATH_MAX, "%s", imageData->filename);
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
        if (strcmp(dirEntry->d_name, ".") == 0 ||
            strcmp(dirEntry->d_name, "..") == 0)
        {
            continue;
        }
        itemname = userInputPathFilter(dirEntry->d_name, 0);
        if (itemname == NULL) {
            fprintf(stderr, "FAILED to correctly filter entry: %s\n",
                dirEntry->d_name);
            rc = 2;
            goto _bindImgUDI_unclean;
        }
        if (strlen(itemname) == 0) {
            free(itemname);
            itemname = NULL;
            continue;
        }

        /* prevent the udiRoot from getting recursively mounted */
        snprintf(mntBuffer, PATH_MAX, "/%s/%s", relpath, itemname);
        mntBuffer[PATH_MAX-1] = 0;
        if (pathcmp(mntBuffer, udiConfig->udiMountPoint) == 0) {
            free(itemname);
            itemname = NULL;
            continue;
        }

        /* check to see if UDI version already exists */
        snprintf(mntBuffer, PATH_MAX, "%s/%s/%s", udiRoot, relpath, itemname);
        mntBuffer[PATH_MAX-1] = 0;
        if (lstat(mntBuffer, &statData) == 0) {
            /* exists in UDI, skip */
            free(itemname);
            itemname = NULL;
            continue;
        }

        /* after filtering, lstat path to get details */
        snprintf(srcBuffer, PATH_MAX, "%s/%s/%s", imgRoot, relpath, itemname);
        srcBuffer[PATH_MAX-1] = 0;
        if (lstat(srcBuffer, &statData) != 0) {
            /* path didn't exist, skip */
            free(itemname);
            itemname = NULL;
            continue;
        }

        /* if target is a symlink, copy it */
        if (S_ISLNK(statData.st_mode)) {
            char *args[] = { strdup(udiConfig->cpPath), strdup("-P"),
                strdup(srcBuffer), strdup(mntBuffer), NULL
            };
            char **argsPtr = NULL;
            int ret = forkAndExecv(args);
            for (argsPtr = args; *argsPtr != NULL; argsPtr++) {
                free(*argsPtr);
            }
            if (ret != 0) {
                fprintf(stderr, "Failed to copy %s to %s.\n", srcBuffer, mntBuffer);
                rc = 2;
                goto _bindImgUDI_unclean;
            }
            free(itemname);
            itemname = NULL;
            continue;
        }
        if (S_ISREG(statData.st_mode)) {
            if (statData.st_size < FILE_SIZE_LIMIT) {
                char *args[] = { strdup(udiConfig->cpPath), strdup("-p"),
                    strdup(srcBuffer), strdup(mntBuffer), NULL
                };
                char **argsPtr = NULL;
                int ret = forkAndExecv(args);
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
                if (fp != NULL) {
                    fclose(fp);
                }
                BINDMOUNT(&mountCache, srcBuffer, mntBuffer, 0, 0);
            }
            free(itemname);
            itemname = NULL;
            continue;
        }
        if (S_ISDIR(statData.st_mode)) {
            if (copyFlag == 0) {
                MKDIR(mntBuffer, 0755);
                BINDMOUNT(&mountCache, srcBuffer, mntBuffer, 0, 0);
            } else {
                char *args[] = { strdup(udiConfig->cpPath), strdup("-rp"),
                    strdup(srcBuffer), strdup(mntBuffer), NULL
                };
                char **argsPtr = NULL;
                int ret = forkAndExecv(args);
                for (argsPtr = args; *argsPtr != NULL; argsPtr++) {
                    free(*argsPtr);
                }
                if (ret != 0) {
                    fprintf(stderr, "Failed to copy %s to %s.\n", srcBuffer,
                            mntBuffer);
                    rc = 2;
                    goto _bindImgUDI_unclean;
                }
            }
            free(itemname);
            itemname = NULL;
            continue;
        }
        /* no other types are supported */
        free(itemname);
        itemname = NULL;
    }
    closedir(subtree);
    subtree = NULL;

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
    if (subtree != NULL) {
        closedir(subtree);
        subtree = NULL;
    }
    return rc;
}

/*! Copy a file or link as correctly as possible */
/*!
 * Copy file (or symlink) from source to dest.
 * \param cpPath path to cp exectutable
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
int _shifterCore_copyFile(const char *cpPath, const char *source,
        const char *dest, int keepLink, uid_t owner, gid_t group, mode_t mode)
{
    struct stat destStat;
    struct stat sourceStat;
    char *cmdArgs[5] = { NULL, NULL, NULL, NULL, NULL };
    char **ptr = NULL;
    size_t cmdArgs_idx = 0;
    int isLink = 0;
    mode_t tgtMode = mode;

    if (cpPath == NULL ||
            dest == NULL ||
            source == NULL ||
            strlen(dest) == 0 ||
            strlen(source) == 0)
    {
        fprintf(stderr, "Invalid arguments for _shifterCore_copyFile\n");
        goto _copyFile_unclean;
    }
    if (stat(dest, &destStat) == 0) {
        /* check if dest is a directory */
        if (!S_ISDIR(destStat.st_mode)) {
            fprintf(stderr, "Destination path %s exists and is not a directory."
                   " Will not copy\n", dest);
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

    cmdArgs[cmdArgs_idx++] = strdup(cpPath);
    if (isLink == 1 && keepLink == 1) {
        cmdArgs[cmdArgs_idx++] = strdup("-P");
    }
    cmdArgs[cmdArgs_idx++] = strdup(source);
    cmdArgs[cmdArgs_idx++] = strdup(dest);
    cmdArgs[cmdArgs_idx++] = NULL;

    /* perform the copy (and try a second time just in case the source file changes during copy) */
    if (forkAndExecv(cmdArgs) != 0) {
        if (forkAndExecv(cmdArgs) != 0) {
            fprintf(stderr, "Failed to copy %s to %s\n", source, dest);
            goto _copyFile_unclean;
        }
    }

    if (owner == INVALID_USER) owner = sourceStat.st_uid;
    if (group == INVALID_GROUP) group = sourceStat.st_gid;
    if (owner != INVALID_USER && group != INVALID_GROUP) {
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
int prepareSiteModifications(const char *username,
                             const char *minNodeSpec,
                             UdiRootConfig *udiConfig)
{
    /* construct path to "live" copy of the image. */
    char udiRoot[PATH_MAX];
    char mntBuffer[PATH_MAX];
    char srcBuffer[PATH_MAX];
    const char **fnamePtr = NULL;
    int ret = 0;
    struct stat statData;
    dev_t udiMountDev = 0;
    MountList mountCache;

    const char *mandatorySiteEtcFiles[4] = {
        "passwd", "group", "nsswitch.conf", NULL
    };
    const char *copyLocalEtcFiles[3] = {
        "hosts", "resolv.conf", NULL
    };

    memset(&mountCache, 0, sizeof(MountList));
    snprintf(udiRoot, PATH_MAX, "%s", udiConfig->udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;

    /* get udiMount device id */
    if (stat(udiRoot, &statData) != 0) {
        fprintf(stderr, "FAILED to stat udiRoot %s.\n", udiRoot);
        return 1;
    }
    udiMountDev = statData.st_dev;

    /* ensure any upcoming relative path operations are done with correct
       working directory */
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
    perror("   --- REASON: "); \
    ret = 1; \
    goto _prepSiteMod_unclean; \
}
#define _BINDMOUNT(mountCache, from, to, flags, overwrite) if (_shifterCore_bindMount(udiConfig, mountCache, from, to, flags, overwrite) != 0) { \
    fprintf(stderr, "BIND MOUNT FAILED from %s to %s\n", from, to); \
    perror("   --- REASON: "); \
    ret = 1; \
    goto _prepSiteMod_unclean; \
}

    _MKDIR("etc", 0755);
    _MKDIR("etc/udiImage", 0755);
    _MKDIR("opt", 0755);
    _MKDIR("opt/udiImage", 0755);
    _MKDIR("var", 0755);
    _MKDIR("var/spool", 0755);
    _MKDIR("var/run", 0755);
    _MKDIR("var/empty", 0700);
    _MKDIR("proc", 0755);
    _MKDIR("sys", 0755);
    _MKDIR("dev", 0755);
    _MKDIR("tmp", 0777);

    /* run site-defined pre-mount procedure */
    if (udiConfig->sitePreMountHook && strlen(udiConfig->sitePreMountHook) > 0) {
        char *args[] = {
            strdup("/bin/sh"), strdup(udiConfig->sitePreMountHook), NULL
        };
        char **argsPtr = NULL;
        ret = forkAndExecv(args);
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
    if (setupVolumeMapMounts(&mountCache, udiConfig->siteFs, 0, udiMountDev, udiConfig) != 0) {
        fprintf(stderr, "FAILED to mount siteFs volumes\n");
        goto _prepSiteMod_unclean;
    }

    /* run site-defined post-mount procedure */
    if (udiConfig->sitePostMountHook && strlen(udiConfig->sitePostMountHook) > 0) {
        char *args[] = {
            strdup("/bin/sh"), strdup(udiConfig->sitePostMountHook), NULL
        };
        char **argsPtr = NULL;
        ret = forkAndExecv(args);
        for (argsPtr = args; *argsPtr != NULL; argsPtr++) {
            free(*argsPtr);
        }
        if (ret != 0) {
            fprintf(stderr, "Site postmount hook failed. Exiting.\n");
            ret = 1;
            goto _prepSiteMod_unclean;
        }
    }

    /* copy needed local files */
    for (fnamePtr = copyLocalEtcFiles; *fnamePtr != NULL; fnamePtr++) {
        char source[PATH_MAX];
        char dest[PATH_MAX];
        snprintf(source, PATH_MAX, "/etc/%s", *fnamePtr);
        snprintf(dest, PATH_MAX, "%s/etc/%s", udiRoot, *fnamePtr);
        source[PATH_MAX - 1] = 0;
        dest[PATH_MAX - 1] = 0;
        if (_shifterCore_copyFile(udiConfig->cpPath, source, dest, 1, 0, 0, 0644) != 0) {
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

    if (udiConfig->populateEtcDynamically == 0) {
        /* --> loop over everything in site etc-files and copy into image etc */
        if (udiConfig->etcPath == NULL || strlen(udiConfig->etcPath) == 0) {
            fprintf(stderr, "UDI etcPath source directory not defined.\n");
            goto _prepSiteMod_unclean;
        }
        snprintf(srcBuffer, PATH_MAX, "%s", udiConfig->etcPath);
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
                    goto _fail_copy_etcPath;
                }
                snprintf(srcBuffer, PATH_MAX, "%s/%s", udiConfig->etcPath, filename);
                srcBuffer[PATH_MAX-1] = 0;
                snprintf(mntBuffer, PATH_MAX, "%s/etc/%s", udiRoot, filename);
                mntBuffer[PATH_MAX-1] = 0;
                free(filename);
                filename = NULL;

                if (lstat(srcBuffer, &statData) != 0) {
                    fprintf(stderr, "Couldn't find source file, check if there are illegal characters: %s\n", srcBuffer);
                    goto _fail_copy_etcPath;
                }

                if (lstat(mntBuffer, &statData) == 0) {
                    fprintf(stderr, "Couldn't copy %s because file already exists.\n", mntBuffer);
                    goto _fail_copy_etcPath;
                } else {
                    ret = _shifterCore_copyFile(udiConfig->cpPath, srcBuffer, mntBuffer, 0, 0, 0, 0644);
                    if (ret != 0) {
                        fprintf(stderr, "Failed to copy %s to %s.\n", srcBuffer, mntBuffer);
                        goto _fail_copy_etcPath;
                    }
                }
                continue;
_fail_copy_etcPath:
                if (filename != NULL) {
                    free(filename);
                    filename = NULL;
                }
                if (etcDir != NULL) {
                    closedir(etcDir);
                    etcDir = NULL;
                }
                goto _prepSiteMod_unclean;
            }
            closedir(etcDir);
        } else {
            fprintf(stderr, "Couldn't stat udiRoot etc dir: %s\n", srcBuffer);
            goto _prepSiteMod_unclean;
        }
    } else if (udiConfig->target_uid != 0 && udiConfig->target_gid != 0) {
        /* udiConfig->populateEtcDynamically == 1 */
        struct passwd *pwd = shifter_getpwuid(udiConfig->target_uid, udiConfig);
        struct group *grp = getgrgid(udiConfig->target_gid);
        FILE *fp = NULL;
        if (pwd == NULL) {
            fprintf(stderr, "Couldn't get user properties for uid %d\n", udiConfig->target_uid);
            goto _prepSiteMod_unclean;
        }
        if (grp == NULL) {
            fprintf(stderr, "Couldn't get group properties for gid %d\n", udiConfig->target_gid);
            goto _prepSiteMod_unclean;
        }

        /* write out container etc/passwd */
        snprintf(srcBuffer, PATH_MAX, "%s/etc/passwd", udiRoot);
        fp = fopen(srcBuffer, "w");
        if (fp == NULL) {
            fprintf(stderr, "Couldn't open passwd file for writing\n");
            goto _prepSiteMod_unclean;
        }
        fprintf(fp, "%s:x:%d:%d:%s:%s:%s\n", pwd->pw_name, pwd->pw_uid,
                pwd->pw_gid, pwd->pw_gecos, pwd->pw_dir, pwd->pw_shell);
        fclose(fp);
        fp = NULL;

        /* write out container etc/group */
        snprintf(srcBuffer, PATH_MAX, "%s/etc/group", udiRoot);
        fp = fopen(srcBuffer, "w");
        if (fp == NULL) {
            fprintf(stderr, "Couldn't open group file for writing\n");
            goto _prepSiteMod_unclean;
        }
        fprintf(fp, "%s:x:%d:\n", grp->gr_name, grp->gr_gid);
        fclose(fp);
        fp = NULL;

        /* write out container etc/nsswitch.conf */
        snprintf(srcBuffer, PATH_MAX, "%s/etc/nsswitch.conf", udiRoot);
        fp = fopen(srcBuffer, "w");
        if (fp == NULL) {
            fprintf(stderr, "Couldn't open nsswitch.conf for writing\n");
            goto _prepSiteMod_unclean;
        }
        fprintf(fp, "passwd: files\ngroup: files\nhosts: files dns\n"
                "networks:   files dns\nservices: files\nprotocols: files\n"
                "rpc: files\nethers: files\nnetmasks: files\nnetgroup: files\n"
                "publickey: files\nbootparams: files\nautomount: files\n"
                "aliases: files\n");
        fclose(fp);
        fp = NULL;
    } else {
        fprintf(stderr, "Unable to setup etc.\n");
        goto _prepSiteMod_unclean;
    }

    /* no valid reason for a user to provide their own /etc/shadow */
    /* populate /etc/shadow with an empty file */
    snprintf(srcBuffer, PATH_MAX, "%s/etc/shadow", udiRoot);
    FILE *fp = fopen(srcBuffer, "w");
    if (fp == NULL) {
        fprintf(stderr, "Couldn't open shadow file for writing\n");
        goto _prepSiteMod_unclean;
    }
    fclose(fp);
    fp = NULL;

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
        mvArgs[0] = strdup(udiConfig->mvPath);
        mvArgs[1] = strdup(fromGroupFile);
        mvArgs[2] = strdup(toGroupFile);
        mvArgs[3] = NULL;
        ret = forkAndExecv(mvArgs);
        for (argsPtr = mvArgs; *argsPtr != NULL; argsPtr++) {
            free(*argsPtr);
        }
        if (ret != 0) {
            fprintf(stderr, "Failed to rename %s to %s\n", fromGroupFile, toGroupFile);
            goto _prepSiteMod_unclean;
        }

        if (filterEtcGroup(fromGroupFile, toGroupFile, username, udiConfig->maxGroupCount) != 0) {
            fprintf(stderr, "Failed to filter group file %s\n", fromGroupFile);
            goto _prepSiteMod_unclean;
        }
    }

    /* recursively copy /opt/udiImage (to allow modifications) */
    if (udiConfig->optUdiImage != NULL) {
        char finalPath[PATH_MAX];
        snprintf(srcBuffer, PATH_MAX, "%s/", udiConfig->optUdiImage);
        srcBuffer[PATH_MAX-1] = 0;
        if (stat(srcBuffer, &statData) != 0) {
            fprintf(stderr, "FAILED to stat udiImage source directory: %s\n", srcBuffer);
            goto _prepSiteMod_unclean;
        }
        snprintf(mntBuffer, PATH_MAX, "%s/opt", udiRoot);
        mntBuffer[PATH_MAX-1] = 0;
        snprintf(finalPath, PATH_MAX, "%s/udiImage", mntBuffer);
        finalPath[PATH_MAX-1] = 0;

        if (stat(mntBuffer, &statData) != 0) {
            fprintf(stderr, "FAILED to stat udiImage target directory: %s\n", mntBuffer);
            goto _prepSiteMod_unclean;
        } else {
            char *args[] = {strdup(udiConfig->cpPath), strdup("-rp"),
                strdup(srcBuffer), strdup(mntBuffer), NULL
            };
            char *chmodArgs[] = {strdup(udiConfig->chmodPath), strdup("-R"),
                strdup("a+rX"), strdup(finalPath), NULL
            };
            ret = forkAndExecv(args);
            if (ret == 0) {
                ret = forkAndExecv(chmodArgs);
                if (ret != 0) {
                    fprintf(stderr, "FAILED to fix permissions on %s.\n", mntBuffer);
                }
            } else {
                fprintf(stderr, "FAILED to copy %s to %s.\n", srcBuffer, mntBuffer);
            }
            if (args[0]) free(args[0]);
            if (args[1]) free(args[1]);
            if (args[2]) free(args[2]);
            if (args[3]) free(args[3]);
            if (chmodArgs[0]) free(chmodArgs[0]);
            if (chmodArgs[1]) free(chmodArgs[1]);
            if (chmodArgs[2]) free(chmodArgs[2]);
            if (chmodArgs[3]) free(chmodArgs[3]);
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


#undef _MKDIR
#undef _BINDMOUNT

    free_MountList(&mountCache, 0);
    return 0;
_prepSiteMod_unclean:
    free_MountList(&mountCache, 0);
    destructUDI(udiConfig, 0);
    if (ret == 0) {
        return 1;
    }
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
    if (minNode == NULL) {
        goto _writeHostFile_error;
    }

    limit = minNode + strlen(minNode);

    snprintf(filename, PATH_MAX, "%s/var/hostsfile", udiConfig->udiMountPoint);
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
        count = (int) strtol(sptr, NULL, 10);
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
    char *sshPath = NULL;
    dev_t destRootDev = 0;
    dev_t srcRootDev = 0;
    dev_t tmpDev = 0;

    umask(022);

    if (imageData == NULL || username == NULL || udiConfig == NULL) {
        fprintf(stderr, "Invalid arguments to mountImageVFS(), error.\n");
        goto _mountImgVfs_unclean;
    }
    if (imageData->type != NULL && strcmp(imageData->type, "local") == 0 && udiConfig->allowLocalChroot == 0) {
        fprintf(stderr, "local chroot path requested, but this is disallowed by site policy, Fail.\n");
        goto _mountImgVfs_unclean;
    }

#define _MKDIR(dir, perm) if (mkdir(dir, perm) != 0) { \
    fprintf(stderr, "FAILED to mkdir %s. Exiting.\n", dir); \
    goto _mountImgVfs_unclean; \
}

    snprintf(udiRoot, PATH_MAX, "%s", udiConfig->udiMountPoint);

    if (lstat(udiRoot, &statData) != 0) {
        _MKDIR(udiRoot, 0755);
    }

#define BIND_IMAGE_INTO_UDI(subtree, img, udiConfig, copyFlag) \
    if (bindImageIntoUDI(subtree, img, udiConfig, copyFlag) > 1) { \
        fprintf(stderr, "FAILED To setup \"%s\" in %s\n", subtree, udiRoot); \
        goto _mountImgVfs_unclean; \
    }

    /* mount a new rootfs to work in */
    if (mount(NULL, udiRoot, udiConfig->rootfsType, MS_NOSUID|MS_NODEV, NULL) != 0) {
        fprintf(stderr, "FAILED to mount rootfs on %s\n", udiRoot);
        perror("   --- REASON: ");
        goto _mountImgVfs_unclean;
    }
    if (makeUdiMountPrivate(udiConfig) != 0) {
        fprintf(stderr, "FAILED to mark the udi as a private mount\n");
        goto _mountImgVfs_unclean;
    }

    if (chmod(udiRoot, 0755) != 0) {
        fprintf(stderr, "FAILED to chmod \"%s\" to 0755.\n", udiRoot);
        goto _mountImgVfs_unclean;
    }

    /* get destination device */
    if (lstat(udiRoot, &statData) != 0) {
        fprintf(stderr, "FAILED to stat %s\n", udiRoot);
        goto _mountImgVfs_unclean;
    }
    destRootDev = statData.st_dev;

    /* work out source device */
    if (imageData->useLoopMount) {
        if (lstat(udiConfig->loopMountPoint, &statData) != 0) {
            fprintf(stderr, "FAILED to stat loop mount point.\n");
            goto _mountImgVfs_unclean;
        }
    } else {
        if (lstat(imageData->filename, &statData) != 0) {
            fprintf(stderr, "FAILED to stat udi source.\n");
            goto _mountImgVfs_unclean;
        }
    }
    srcRootDev = statData.st_dev;

    if (lstat("/tmp", &statData) != 0) {
        fprintf(stderr, "FAILED to stat /tmp\n");
        goto _mountImgVfs_unclean;
    }
    tmpDev = statData.st_dev;

    /* authorize destRootDev, srcRootDev, and tmpDev  as the only allowed
     * volume mount targets */
    udiConfig->bindMountAllowedDevices = malloc(3 * sizeof(dev_t));
    if (udiConfig->bindMountAllowedDevices == NULL) {
        fprintf(stderr, "FAILED to allocate memory\n");
        goto _mountImgVfs_unclean;
    }
    udiConfig->bindMountAllowedDevices[0] = destRootDev;
    udiConfig->bindMountAllowedDevices[1] = srcRootDev;
    udiConfig->bindMountAllowedDevices[2] = tmpDev;
    udiConfig->bindMountAllowedDevices_sz = 3;


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
    sshPath = alloc_strgenf("%s/etc/ssh", udiRoot);
    if (sshPath != NULL) {
        _MKDIR(sshPath, 0755);
        free(sshPath);
        sshPath = NULL;
    }

    /* copy image /etc into place */
    BIND_IMAGE_INTO_UDI("/etc", imageData, udiConfig, 1);


#undef BIND_IMAGE_INTO_UDI
#undef _MKDIR

    return 0;

_mountImgVfs_unclean:
    if (sshPath != NULL) {
        free(sshPath);
    }
    return 1;
}

/** makeUdiMountPrivate
 *  Some Linux systems default their mounts to "shared" mounts, which means
 *  that mount option changes Shifter makes (or unmounts) can propagate back up
 *  to the original mount, which is not desirable.  This function remounts the
 *  base udiMount point as MS_PRIVATE - which means that no external mount 
 *  changes propagate into these mountpoints, nor do these go back up the 
 *  chain.  It may be desirable to allow sites to choose MS_SLAVE instead of
 *  MS_PRIVATE here, as that will allow site unmounts to propagate into shifter
 *  containers.
 */
int makeUdiMountPrivate(UdiRootConfig *udiConfig) {
    char buffer[PATH_MAX];
    snprintf(buffer, PATH_MAX, "%s", udiConfig->udiMountPoint);
    if (mount(NULL, buffer, NULL, MS_PRIVATE|MS_REC, NULL) != 0) {
        perror("Failed to remount non-shared.");
        return 1;
    }
    return 0;
}

int remountUdiRootReadonly(UdiRootConfig *udiConfig) {
    char udiRoot[PATH_MAX];

    if (udiConfig == NULL || udiConfig->udiMountPoint == NULL) {
        return 1;
    }
    snprintf(udiRoot, PATH_MAX, "%s", udiConfig->udiMountPoint);

    if (mount(udiRoot, udiRoot, udiConfig->rootfsType, MS_REMOUNT|MS_NOSUID|MS_NODEV|MS_RDONLY, NULL) != 0) {
        fprintf(stderr, "FAILED to remount rootfs readonly on %s\n", udiRoot);
        perror("   --- REASON: ");
        goto _remountUdiRootReadonly_unclean;
    }
    return 0;

_remountUdiRootReadonly_unclean:
    return 1;
}

int mountImageLoop(ImageData *imageData, UdiRootConfig *udiConfig) {
    char loopMountPath[PATH_MAX];
    char imagePath[PATH_MAX];
    if (imageData == NULL || udiConfig == NULL) {
        return 1;
    }
    if (imageData->useLoopMount == 0) {
        return 0;
    }
    if (udiConfig->loopMountPoint == NULL || strlen(udiConfig->loopMountPoint) == 0) {
        return 1;
    }
    if (mkdir(udiConfig->loopMountPoint, 0755) != 0) {
        if (errno != EEXIST) {
            fprintf(stderr, "FAILED to mkdir %s. Exiting.\n", udiConfig->loopMountPoint);
            goto _mountImageLoop_unclean;
        }
    }

    snprintf(loopMountPath, PATH_MAX, "%s", udiConfig->loopMountPoint);
    loopMountPath[PATH_MAX-1] = 0;
    snprintf(imagePath, PATH_MAX, "%s", imageData->filename);
    imagePath[PATH_MAX-1] = 0;
    if (loopMount(imagePath, loopMountPath, imageData->format, udiConfig, 1) != 0) {
        fprintf(stderr, "FAILED to loop mount image: %s\n", imagePath);
        goto _mountImageLoop_unclean;
    }
    return 0;
_mountImageLoop_unclean:
    return 1;
}

/**
 * _sortFsTypeForward
 * Utility function use for comparisons for sorting filesystems
 */
static int _sortFsTypeForward(const void *ta, const void *tb) {
    const char **a = (const char **) ta;
    const char **b = (const char **) tb;

    return strcmp(*a, *b);
}

/** getSupportedFilesystems
 *  Read /proc/filesystems and produce a list of filesystems supported by this
 *  kernel.
 *
 *  Returns NULL-terminated array of strings with all filesystem types
 */
char **getSupportedFilesystems() {
    char buffer[4096];
    char **ret = (char **) malloc(sizeof(char *) * 10);
    char **writePtr = NULL;
    size_t listExtent = 10;
    size_t listLen = 0;
    FILE *fp = NULL;
    
    if (ret == NULL) { // || buffer == NULL) {
        /* ran out of memory */
        return NULL;
    }

    fp = fopen("/proc/filesystems", "r");
    if (fp == NULL) {
        free(ret);
        ret = NULL;
        return NULL;
    }

    writePtr = ret;
    *writePtr = NULL;
    while (fgets(buffer, 4096, fp) != NULL) {
        char *ptr = strchr(buffer, '\t');

        if (ptr != NULL) {
            ptr = shifter_trim(ptr);
            if (strlen(ptr) == 0) continue;
            if (listLen == listExtent - 2) {
                char **tmp = (char **) realloc(ret, sizeof(char *) * (listExtent + 10));
                if (tmp == NULL) {
                    goto error;
                }
                writePtr = tmp + (writePtr - ret);
                ret = tmp;
                listExtent += 10;
            }
            *writePtr = strdup(ptr);
            writePtr++;
            *writePtr = NULL;
            listLen++;
        }
    }
    qsort(ret, listLen, sizeof(char *), _sortFsTypeForward);

    fclose(fp);
    fp = NULL;
    return ret;
error:
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
    return NULL;
}

int supportsFilesystem(char *const * fsTypes, const char *fsType) {
    char *const *ptr = fsTypes;

    if (fsTypes == NULL || fsType == NULL) {
        return -1;
    }
    for (ptr = fsTypes; ptr && *ptr; ptr++) {
        if (strcmp(fsType, *ptr) == 0) {
            return 0;
        }
    }
    return 1;
}

int loopMount(const char *imagePath, const char *loopMountPath, ImageFormat format, UdiRootConfig *udiConfig, int readOnly) {
    char mountExec[PATH_MAX];
    struct stat statData;
    int ready = 0;
    int useAutoclear = 0;
    const char *imgType = NULL;
    char **fstypes = getSupportedFilesystems();
#define LOADKMOD(name, path) if (loadKernelModule(name, path, udiConfig) != 0) { \
    fprintf(stderr, "FAILED to load %s kernel module.\n", name); \
    goto _loopMount_unclean; \
}

    snprintf(mountExec, PATH_MAX, "%s/mount", LIBEXECDIR);

    if (stat(mountExec, &statData) != 0) {
        fprintf(stderr, "udiRoot mount executable missing: %s\n", mountExec);
        goto _loopMount_unclean;
    } else if (statData.st_uid != 0 || statData.st_mode & S_IWGRP || statData.st_mode & S_IWOTH || !(statData.st_mode & S_IXUSR)) {
        fprintf(stderr, "udiRoot mount has incorrect ownership or permissions: %s\n", mountExec);
        goto _loopMount_unclean;
    }

    if (stat("/dev/loop0", &statData) != 0) {
        LOADKMOD("loop", "drivers/block/loop.ko");
    }
    if (format == FORMAT_EXT4) {
        if (supportsFilesystem(fstypes, "ext4") != 0) {
            LOADKMOD("mbcache", "fs/mbcache.ko");
            LOADKMOD("jbd2", "fs/jbd2/jbd2.ko");
            LOADKMOD("ext4", "fs/ext4/ext4.ko");
        }
        useAutoclear = 1;
        ready = 1;
        imgType = "ext4";
    } else if (format == FORMAT_SQUASHFS) {
        if (supportsFilesystem(fstypes, "squashfs") != 0) {
            LOADKMOD("squashfs", "fs/squashfs/squashfs.ko");
        }
        useAutoclear = 1;
        ready = 1;
        imgType = "squashfs";
    } else if (format == FORMAT_CRAMFS) {
        if (supportsFilesystem(fstypes, "cramfs") != 0) {
            LOADKMOD("cramfs", "fs/cramfs/cramfs.ko");
        }
        useAutoclear = 1;
        ready = 1;
        imgType = "cramfs";
    } else if (format == FORMAT_XFS) {
        if (supportsFilesystem(fstypes, "cramfs") != 0) {
            if (loadKernelModule("xfs", "fs/xfs/xfs.ko", udiConfig) != 0) {
                LOADKMOD("exportfs", "fs/exportfs/exportfs.ko");
                LOADKMOD("xfs", "fs/xfs/xfs.ko");
            }
        }
        useAutoclear = 0;
        ready = 1;
        imgType = "xfs";
    } else {
        fprintf(stderr, "ERROR: unknown image format.\n");
        goto _loopMount_unclean;
    }
    if (ready) {
        char *args[] = {
            strdup(mountExec),
            strdup("-n"),
            strdup("-o"),
            alloc_strgenf("loop,nosuid,nodev%s%s",
                    (readOnly ? ",ro" : ""),
                    (useAutoclear ? ",autoclear" : "")
            ),
            strdup("-t"),
            strdup(imgType),
            strdup(imagePath),
            strdup(loopMountPath),
            NULL
        };
        char **argsPtr = NULL;
        int ret = 0;
        for (argsPtr = args; argsPtr - args < 8; argsPtr++) {
            if (argsPtr == NULL || *argsPtr == NULL) {
                ret = 1;
            }
        }
        if (ret == 0) {
            ret = forkAndExecvSilent(args);
        }
        for (argsPtr = args; argsPtr && *argsPtr; argsPtr++) {
            free(*argsPtr);
        }
        if (ret != 0) {
            fprintf(stderr, "FAILED to mount image %s (%s) on %s\n", imagePath, imgType, loopMountPath);
            goto _loopMount_unclean;
        }
    }

    if (fstypes != NULL) {
        char **ptr = NULL;
        for (ptr = fstypes; ptr && *ptr; ptr++) {
            free(*ptr);
        }
        free(fstypes);
        fstypes = NULL;
    }
#undef LOADKMOD
    return 0;
_loopMount_unclean:

    if (fstypes != NULL) {
        char **ptr = NULL;
        for (ptr = fstypes; ptr && *ptr; ptr++) {
            free(*ptr);
        }
        free(fstypes);
        fstypes = NULL;
    }
    return 1;
}

int setupUserMounts(VolumeMap *map, UdiRootConfig *udiConfig) {
    char udiRoot[PATH_MAX];
    MountList mountCache;
    struct stat statData;
    int ret = 0;
    dev_t udiMountDev = 0;

    memset(&mountCache, 0, sizeof(MountList));
    snprintf(udiRoot, PATH_MAX, "%s", udiConfig->udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;

    if (stat(udiRoot, &statData) != 0) {
        fprintf(stderr, "FAILED to stat udiRoot %s\n", udiRoot);
        return 1;
    }
    udiMountDev = statData.st_dev;

    /* get list of current mounts for this namespace */
    if (parse_MountList(&mountCache) != 0) {
        fprintf(stderr, "FAILED to get list of current mount points\n");
        return 1;
    }

    ret = setupVolumeMapMounts(&mountCache, map, 1, udiMountDev, udiConfig);
    free_MountList(&mountCache, 0);
    return ret;
}

int setupPerNodeCacheFilename(
        UdiRootConfig *udiConfig,
        VolMapPerNodeCacheConfig *cache,
        char *buffer,
        size_t buffer_len)
{
    char hostname_buf[128];
    size_t pos = 0;
    int fd = 0;
    int nbytes = 0;

    if (    cache == NULL ||
            cache->fstype == NULL  ||
            buffer == NULL ||
            buffer_len == 0 ||
            udiConfig == NULL)
    {
        return -1;
    }
    if (    udiConfig->perNodeCachePath == NULL ||
            strlen(udiConfig->perNodeCachePath) == 0)
    {
        fprintf(stderr, "udiRoot.conf does not have a valid perNodeCachePath."
                " Cannot generate perNodeCache backing stores.\n");
        return -1;
    }
    if (udiConfig->target_uid == 0 || udiConfig->target_gid == 0) {
        fprintf(stderr, "will not setup per-node cache with target uid or gid of 0\n");
        return -1;
    }
    pos = strlen(buffer);
    if (pos > buffer_len) {
        return -1;
    }
    if (gethostname(hostname_buf, 128) != 0) {
        return -1;
    }
    nbytes = snprintf(buffer, buffer_len, "%s/perNodeCache_uid%d_gid%d_%s.%s.XXXXXX",
            udiConfig->perNodeCachePath,
            udiConfig->target_uid,
            udiConfig->target_gid,
            hostname_buf,
            cache->fstype
    );
    if (nbytes >= buffer_len - 1) {
        fprintf(stderr, "perNodeCache filename too long to store in buffer.\n");
        return -1;
    }
    mode_t old_umask = umask(077);
    fd = mkstemp(buffer);
    if (fd < 0) {
        fprintf(stderr, "Failed to open perNodeCache backing store %s.\n",
                buffer);
        perror("Error: ");
        umask(old_umask);
        return -1;
    }
    umask(old_umask);
    return fd;
}

int setupPerNodeCacheBackingStore(VolMapPerNodeCacheConfig *cache, const char *buffer, UdiRootConfig *udiConfig) {
    if (udiConfig == NULL || cache == NULL || cache->fstype == NULL) {
        fprintf(stderr, "configuration is invalid (null), cannot setup per-node cache\n");
        return 1;
    }
    char *args[7];
    char **arg = NULL;
    int ret = 0;
    if (udiConfig->ddPath == NULL) {
        fprintf(stderr, "Must define ddPath in udiRoot configuration to use this feature\n");
        return 1;
    }
    args[0] = strdup(udiConfig->ddPath);
    args[1] = strdup("if=/dev/zero");
    args[2] = alloc_strgenf("of=%s", buffer);
    args[3] = strdup("bs=1");
    args[4] = strdup("count=0");
    args[5] = alloc_strgenf("seek=%lu", cache->cacheSize);
    args[6] = NULL;
    for (arg = args; arg - args < 6; arg++) {
        if (arg == NULL || *arg == NULL) {
            fprintf(stderr, "FAILED to allocate memory!\n");
            ret = 1;
        }
    }
    if (ret == 0) {
        ret = forkAndExecvSilent(args);
    }
    for (arg = args; *arg; arg++) {
        free(*arg);
    }

    if (ret != 0) {
        fprintf(stderr, "FAILED to dd backing store for cache on %s, %d\n", buffer, ret);
        return 1;
    }
    if (strcmp(cache->fstype, "xfs") == 0) {
        char **args = NULL;
        char **argPtr = NULL;
        int ret = 0;
        if (udiConfig->mkfsXfsPath == NULL) {
            fprintf(stderr, "Must define mkfsXfsPath in udiRoot configuration to use this feature\n");
            exit(1);
        }
        args = (char **) malloc(sizeof(char *) * 4);
        args[0] = strdup(udiConfig->mkfsXfsPath);
        args[1] = strdup("-d");
        args[2] = alloc_strgenf("name=%s,file=1,size=%lu", buffer, cache->cacheSize);
        args[3] = NULL;
        ret = forkAndExecvSilent(args);
        for (argPtr = args; argPtr && *argPtr; argPtr++) {
            free(*argPtr);
        }
        free(args);
        if (ret != 0) {
            fprintf(stderr, "FAILED to create the XFS cache filesystem on %s\n", buffer);
            return 1;
        }
    }
    return 0;
}

int setupVolumeMapMounts(
        MountList *mountCache,
        VolumeMap *map,
        int userRequested,
        dev_t createToDev,
        UdiRootConfig *udiConfig
) {
    char *filtered_from = NULL;
    char *filtered_to = NULL;
    char *to_real = NULL;
    char *from_real = NULL;
    VolumeMapFlag *flags = NULL;
    int (*_validate_fp)(const char *, const char *, VolumeMapFlag *);

    size_t mapIdx = 0;
    size_t udiMountLen = 0;

    char from_buffer[PATH_MAX];
    char to_buffer[PATH_MAX];
    struct stat statData;

    if (udiConfig == NULL) {
        return 1;
    }

    if (map == NULL || map->n == 0) {
        return 0;
    }

    if (userRequested == 0) {
        _validate_fp = validateVolumeMap_siteRequest;
    } else {
        _validate_fp = validateVolumeMap_userRequest;
    }

    udiMountLen = strlen(udiConfig->udiMountPoint);

    for (mapIdx = 0; mapIdx < map->n; mapIdx++) {
        size_t flagsInEffect = 0;
        size_t flagIdx = 0;
        int backingStoreExists = 0;
        filtered_from = userInputPathFilter(map->from[mapIdx], 1);
        filtered_to = userInputPathFilter(map->to[mapIdx], 1);
        flags = map->flags[mapIdx];

        if (filtered_from == NULL || filtered_to == NULL) {
            fprintf(stderr, "INVALID mount from %s to %s\n",
                    map->from[mapIdx], map->to[mapIdx]);
            goto _handleVolMountError;
        }
        snprintf(from_buffer, PATH_MAX, "%s/%s",
                (userRequested != 0 ? udiConfig->udiMountPoint : ""),
                filtered_from
        );
        from_buffer[PATH_MAX-1] = 0;
        snprintf(to_buffer, PATH_MAX, "%s/%s",
                udiConfig->udiMountPoint,
                filtered_to
        );
        to_buffer[PATH_MAX-1] = 0;

        free(filtered_from);
        filtered_from = NULL;
        free(filtered_to);
        filtered_to = NULL;

        /* check if this is a per-volume cache, and if so mangle the from name
         * and then create the volume backing store */
        for (flagIdx = 0; flags && flags[flagIdx].type != 0; flagIdx++) {
            flagsInEffect |= flags[flagIdx].type;
            if (flags[flagIdx].type == VOLMAP_FLAG_PERNODECACHE) {
                int ret = 0;
                int fd = 0;

                VolMapPerNodeCacheConfig *cache =
                        (VolMapPerNodeCacheConfig *) flags[flagIdx].value;
                fd = setupPerNodeCacheFilename(udiConfig, cache, from_buffer,
                        PATH_MAX);
                if (fd < 0) {
                    fprintf(stderr, "FAILED to set perNodeCache name\n");
                    goto _handleVolMountError;
                }

                /* intialize the backing store */
                ret = setupPerNodeCacheBackingStore(cache, from_buffer, udiConfig);
                if (ret != 0) {
                    fprintf(stderr, "FAILED to setup perNodeCache\n");
                    goto _handleVolMountError;
                }
                close(fd);
                backingStoreExists = 1;
            }
        }

        /* if this is not a per-node cache (i.e., is a standand volume mount),
         * then validate the user has permissions to view the content, by 
         * performing realpath() and lstat() as the user */
        if (!(flagsInEffect & VOLMAP_FLAG_PERNODECACHE)) {
            uid_t orig_euid = geteuid();
            gid_t orig_egid = getegid();
            gid_t *orig_auxgids = NULL;
            int norig_auxgids = 0;
            int switch_user_stage = 0;

            /* switch privileges if this is a user mount to ensure we only
             * grant access to resources the user can reach at time of 
             * invocation */
            if (userRequested != 0) {
                if (udiConfig->auxiliary_gids == NULL ||
                        udiConfig->nauxiliary_gids <= 0 ||
                        udiConfig->target_uid == 0 ||
                        udiConfig->target_gid == 0)
                {
                    fprintf(stderr, "Insufficient information about target "
                            "user to setup volume mount\n");
                    goto _fail_check_fromvol;
                }
                norig_auxgids = getgroups(0, NULL);
                if (norig_auxgids < 0) {
                    fprintf(stderr, "FAILED to getgroups.\n");
                    goto _fail_check_fromvol;
                } else if (norig_auxgids > 0) {
                    orig_auxgids = (gid_t *) malloc(sizeof(gid_t) * norig_auxgids);
                    if (orig_auxgids == NULL) {
                        fprintf(stderr, "FAILED to allocate memory for groups\n");
                        goto _fail_check_fromvol;
                    }
                    norig_auxgids = getgroups(norig_auxgids, orig_auxgids);
                    if (norig_auxgids <= 0) {
                        fprintf(stderr, "FAILED to getgroups().\n");
                        goto _fail_check_fromvol;
                    }
                }

                if (setgroups(udiConfig->nauxiliary_gids, udiConfig->auxiliary_gids) != 0) {
                    fprintf(stderr, "FAILED to assume user auxiliary gids\n");
                    goto _fail_check_fromvol;
                }
                switch_user_stage++;

                if (setegid(udiConfig->target_gid) != 0) {
                    fprintf(stderr, "FAILED to assume user gid\n");
                    goto _fail_check_fromvol;
                }
                switch_user_stage++;

                if (seteuid(udiConfig->target_uid) != 0) {
                    fprintf(stderr, "FAILED to assume user uid\n");
                    goto _fail_check_fromvol;
                }
                switch_user_stage++;
            }

            /* perform some introspection on the path to get it's real location
             * and vital attributes */
            from_real = realpath(from_buffer, NULL);
            if (from_real == NULL) {
                fprintf(stderr, "FAILED to find real path for volume "
                        "\"from\": %s\n", from_buffer);
                goto _fail_check_fromvol;
            }
            if (lstat(from_real, &statData) != 0) {
                fprintf(stderr, "FAILED to find volume \"from\": %s\n", from_buffer);
                goto _fail_check_fromvol;
            }
            if (!S_ISDIR(statData.st_mode)) {
                fprintf(stderr, "FAILED \"from\" location is not directory: "
                        "%s\n", from_real);
                goto _fail_check_fromvol;
            }

            /* switch back to original privileges */
            if (userRequested != 0) {
                if (seteuid(orig_euid) != 0) {
                    fprintf(stderr, "FAILED to assume original user effective uid\n");
                    goto _fail_check_fromvol;
                }
                switch_user_stage--;

                if (setegid(orig_egid) != 0) {
                    fprintf(stderr, "FAILED to assume original user effective gid\n");
                    goto _fail_check_fromvol;
                }
                switch_user_stage--;

                if (setgroups(norig_auxgids, orig_auxgids) != 0) {
                    fprintf(stderr, "FAILED to assume original user auxiliary gids\n");
                    goto _fail_check_fromvol;
                }
                switch_user_stage--;
            }
            /* skip over error handler */
            goto _pass_check_fromvol;

_fail_check_fromvol:
            if (switch_user_stage > 0) {
                /* TODO decide if we should re-assume original privileges for
                 * the crash */
            }
            if (orig_auxgids != NULL) {
                free(orig_auxgids);
                orig_auxgids = NULL;
            }
            goto _handleVolMountError;

_pass_check_fromvol:
            if (orig_auxgids != NULL) {
                free(orig_auxgids);
                orig_auxgids = NULL;
            }
        } else {
            from_real = realpath(from_buffer, NULL);
        }
        if (lstat(to_buffer, &statData) != 0) {
            if (createToDev) {
                int okToMkdir = 0;

                char *ptr = strrchr(to_buffer, '/');
                if (ptr) {
                    /* get parent path of intended dir */
                    *ptr = '\0';

                    /* if parent path is on the same device as is authorized by createToDev
                       then ok the mkdir operation */
                    if (lstat(to_buffer, &statData) == 0) {
                        if (statData.st_dev == createToDev) {
                            okToMkdir = 1;
                        }
                    }

                    /* reset to target path */
                    *ptr = '/';
                }

                if (okToMkdir) {
                    mkdir(to_buffer, 0755);
                    if (lstat(to_buffer, &statData) != 0) {
                        fprintf(stderr, "FAILED to find volume \"to\": %s\n",
                                to_buffer);
                        goto _handleVolMountError;
                    }
                } else {
                    fprintf(stderr, "FAILED to create volume \"to\": %s, cannot"
                            " create mount points in that location\n",
                            to_buffer);
                    goto _handleVolMountError;
                }
            } else {
                fprintf(stderr, "FAILED to find volume \"to\": %s\n", to_buffer);
                goto _handleVolMountError;
            }
        }
        if (!S_ISDIR(statData.st_mode)) {
            fprintf(stderr, "FAILED \"to\" location is not directory: %s\n", to_buffer);
            goto _handleVolMountError;
        }

        to_real = realpath(to_buffer, NULL);
        if (to_real == NULL) {
            fprintf(stderr, "Failed to get realpath for %s\n", to_buffer);
            goto _handleVolMountError;
        } else if (from_real == NULL) {
            fprintf(stderr, "Failed to get realpath for %s\n", from_buffer);
            goto _handleVolMountError;
        } else {
            size_t to_len = strlen(to_real);
            size_t from_len = strlen(from_real);
            size_t idx = 0;
            int volMountDevOk = 0;
            const char *container_to_real = NULL;
            const char *container_from_real = NULL;
            int ret = 0;
            struct stat toStat;

            /* validate that path starts with udiMountPoint */
            if (to_len <= udiMountLen ||
                strncmp(to_real, udiConfig->udiMountPoint, udiMountLen) != 0) {

                fprintf(stderr, "Invalid destination %s, not allowed, fail.\n", to_real);
                goto _handleVolMountError;
            }

            /* validate source mount point */
            if (userRequested != 0 && !(flagsInEffect & VOLMAP_FLAG_PERNODECACHE)) {
                if (from_len <= udiMountLen ||
                    strncmp(from_real, udiConfig->udiMountPoint, udiMountLen) != 0) {

                    fprintf(stderr, "Invalid source %s, not allowed, fail.\n", from_real);
                    goto _handleVolMountError;
                } else {
                    /* from_real is known to be longer than udiMountLen from 
                     * previous check (i.e., don't remove the check!) */ 
                    container_from_real = from_real + udiMountLen;
                }
            } else {
                container_from_real = from_real;
            }

            /* validate that the path is allowed */
            /* to_real is known to be longer than udiMountLen from previous
             * check (i.e., don't remove the check!) */
            container_to_real = to_real + udiMountLen;

            if ((ret = _validate_fp(container_from_real, container_to_real, flags)) != 0) {
                fprintf(stderr, "Invalid mount request, permission denied! "
                        "Cannot mount from %s to %s. Err Code %d\n",
                        container_from_real, container_to_real, ret);
                goto _handleVolMountError;
            }

            /* the destination mount must be either in the shifter root
             * filesystem, or on the orignal device providing the container
             * image.  Should not allow volume mounts onto other imported 
             * content */
            memset(&toStat, 0, sizeof(struct stat));
            if (lstat(to_real, &toStat) != 0) {
                fprintf(stderr, "Failed to stat destination mount point: %s\n",
                        strerror(errno));
                goto _handleVolMountError;
            }
            if (udiConfig->bindMountAllowedDevices == NULL) {
                fprintf(stderr, "No devices are authorized to accept "
                        "volume mounts.\n");
                goto _handleVolMountError;
            }

            volMountDevOk = 0;
            for (idx = 0; idx < udiConfig->bindMountAllowedDevices_sz; idx++) {
                if (toStat.st_dev == udiConfig->bindMountAllowedDevices[idx]) {
                    volMountDevOk = 1;
                    break;
                }
            }
            if (volMountDevOk == 0){
                fprintf(stderr, "Mount request path %s not on an approved "
                        "device for volume mounts.\n", to_real);
                goto _handleVolMountError;
            }
        }

        if (flagsInEffect & VOLMAP_FLAG_PERNODECACHE) {
            VolMapPerNodeCacheConfig *cacheConfig = NULL;
            /* do something to mount backing store */
            for (flagIdx = 0; flags && flags[flagIdx].type != 0; flagIdx++) {
                if (flags[flagIdx].type == VOLMAP_FLAG_PERNODECACHE) {
                    cacheConfig = (VolMapPerNodeCacheConfig *) flags[flagIdx].value;
                    break;
                }
            }
            if (cacheConfig == NULL) {
                fprintf(stderr, "FAILED to find per-node cache config, exiting.\n");
                goto _handleVolMountError;
            }
            ImageFormat format = FORMAT_INVALID;
            if (strcmp(cacheConfig->fstype, "xfs") == 0) {
                format = FORMAT_XFS;
            }
            if (strcmp(cacheConfig->method, "loop") == 0) {
                if (loopMount(from_buffer, to_real, format, udiConfig, 0) != 0) {
                    fprintf(stderr, "FAILED to mount per-node cache, exiting.\n");
                    goto _handleVolMountError;
                }
                insert_MountList(mountCache, to_real);
            } else {
                fprintf(stderr, "FAILED to understand per-node cache mounting method, exiting.\n");
                goto _handleVolMountError;
            }
            if (chown(to_real, udiConfig->target_uid, udiConfig->target_gid) != 0) {
                fprintf(stderr, "FAILED to chown per-node cache to user.\n");
                goto _handleVolMountError;
            }
            if (unlink(from_buffer) != 0) {
                fprintf(stderr, "FAILED to unlink backing file: %s!", from_buffer);
                perror("Error: ");
                goto _handleVolMountError;
            }

        } else {
            int allowOverwriteBind = 1;

            if (_shifterCore_bindMount(udiConfig, mountCache, from_buffer, to_real, flagsInEffect, allowOverwriteBind) != 0) {
                fprintf(stderr, "BIND MOUNT FAILED from %s to %s\n", from_buffer, to_real);
                goto _handleVolMountError;
            }
        }
        free(to_real);
        to_real = NULL;
        free(from_real);
        from_real = NULL;

        continue;
_handleVolMountError:
        if ((flagsInEffect & VOLMAP_FLAG_PERNODECACHE) && backingStoreExists == 1) {
            unlink(from_buffer);
        }
        goto _setupVolumeMapMounts_unclean;
    }

#undef _BINDMOUNT
    return 0;

_setupVolumeMapMounts_unclean:
    if (filtered_from != NULL) {
        free(filtered_from);
    }
    if (filtered_to != NULL) {
        free(filtered_to);
    }
    if (to_real != NULL) {
        free(to_real);
    }
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

    snprintf(saveFilename, PATH_MAX, "%s/var/shifterConfig.json",
            udiConfig->udiMountPoint);
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

    snprintf(configFilename, PATH_MAX, "%s/var/shifterConfig.json",
            udiConfig->udiMountPoint);

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
    if (configString != NULL) {
        free(configString);
    }
    if (buffer != NULL) {
        free(buffer);
    }
    return -1;
}

int setupImageSsh(char *sshPubKey, char *username, uid_t uid, gid_t gid, UdiRootConfig *udiConfig) {
    struct stat statData;
    char udiImage[PATH_MAX];
    char sshdConfigPath[PATH_MAX];
    char sshdConfigPathNew[PATH_MAX];
    const char *keyType[5] = {"dsa", "ecdsa", "rsa","ed25519", NULL};
    const char **keyPtr = NULL;
    char *lineBuf = NULL;
    size_t lineBuf_size = 0;
    uid_t ownerUid = 0;
    gid_t ownerGid = 0;

    FILE *inputFile = NULL;
    FILE *outputFile = NULL;

    MountList mountCache;
    memset(&mountCache, 0, sizeof(MountList));

    if (udiConfig->optionalSshdAsRoot == 0) {
        ownerUid = uid;
        ownerGid = gid;
    }

    if (udiConfig->optionalSshdAsRoot == 0 && (ownerUid == 0 || ownerGid == 0)) {
        fprintf(stderr, "FAILED to identify proper uid to run sshd\n");
        goto _setupImageSsh_unclean;
    }

#define _BINDMOUNT(mounts, from, to, flags, overwrite) if (_shifterCore_bindMount(udiConfig, mounts, from, to, flags, overwrite) != 0) { \
    fprintf(stderr, "BIND MOUNT FAILED from %s to %s\n", from, to); \
    goto _setupImageSsh_unclean; \
}

    if (parse_MountList(&mountCache) != 0) {
        fprintf(stderr, "FAILED to parse existing mounts\n");
        goto _setupImageSsh_unclean;
    }

    snprintf(udiImage, PATH_MAX, "%s/opt/udiImage", udiConfig->udiMountPoint);
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
        for (argPtr = args; argPtr - args < 7; argPtr++) {
            if (argPtr == NULL || *argPtr == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                ret = 1;
            }
        }
        if (ret == 0) {
            ret = forkAndExecv(args);
        }
        for (argPtr = args; *argPtr != NULL; argPtr++) {
            free(*argPtr);
        }

        if (ret != 0) {
            fprintf(stderr, "Failed to generate key of type %s\n", *keyPtr);
            goto _setupImageSsh_unclean;
        }

        /* chown files to user */
        if (chown(keyFileName, ownerUid, ownerGid) != 0) {
            fprintf(stderr, "Failed to chown ssh host key to user: %s\n",
                    keyFileName);
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
        char *moveCmd[] = { strdup(udiConfig->mvPath),
            strdup(sshdConfigPathNew),
            strdup(sshdConfigPath),
            NULL
        };
        char **argsPtr = NULL;
        int ret = forkAndExecv(moveCmd);
        for (argsPtr = moveCmd; *argsPtr != NULL; argsPtr++) {
            free(*argsPtr);
        }
        if (ret != 0) {
            fprintf(stderr, "FAILED to replace sshd_config with configured version.\n");
            goto _setupImageSsh_unclean;
        }
    }
    if (chown(sshdConfigPath, ownerUid, ownerGid) != 0) {
        fprintf(stderr, "FAILED to chown sshd config path %s\n", sshdConfigPath);
        perror("   errno: ");
        goto _setupImageSsh_unclean;
    }
    if (chmod(sshdConfigPath, S_IRUSR | S_IROTH) != 0) {
        fprintf(stderr, "FAILED to set sshd config permissions to 0600\n");
        perror("   errno: ");
        goto _setupImageSsh_unclean;
    }

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
        if (chown(buffer, uid, 0) != 0) {
            fprintf(stderr, "FAILED to chown ssh pub key to uid %d\n", uid);
            perror("   errno: ");
            goto _setupImageSsh_unclean;
        }
        if (chmod(buffer, S_IRUSR) != 0) {
            fprintf(stderr, "FAILED to set ssh pub key permissions to 0600\n");
            perror("   errno: ");
            goto _setupImageSsh_unclean;
        }
    }

    {
        char from[PATH_MAX];
        char to[PATH_MAX];

        snprintf(from, PATH_MAX, "%s/bin/ssh", udiImage);
        snprintf(to, PATH_MAX, "%s/usr/bin/ssh", udiConfig->udiMountPoint);
        from[PATH_MAX-1] = 0;
        to[PATH_MAX-1] = 0;
        if (stat(to, &statData) == 0) {
            _BINDMOUNT(&mountCache, from, to, VOLMAP_FLAG_READONLY, 1);
        }
        snprintf(from, PATH_MAX, "%s/bin/ssh", udiImage);
        snprintf(to, PATH_MAX, "%s/bin/ssh", udiConfig->udiMountPoint);
        from[PATH_MAX - 1] = 0;
        to[PATH_MAX - 1] = 0;
        if (stat(to, &statData) == 0) {
            _BINDMOUNT(&mountCache, from, to, VOLMAP_FLAG_READONLY, 1);
        }
        snprintf(from, PATH_MAX, "%s/etc/ssh_config", udiImage);
        snprintf(to, PATH_MAX, "%s/etc/ssh/ssh_config", udiConfig->udiMountPoint);
        if (_shifterCore_copyFile(udiConfig->cpPath, from, to, 0, 0, 0, 0) != 0) {
            fprintf(stderr, "FAILED to copy ssh_config to %s\n", to);
            goto _setupImageSsh_unclean;
        }
        /* rely on var/empty created earlier in the setup process */
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
    return 1;
}

gid_t *shifter_getgrouplist(const char *user, gid_t group, int *ngroups) {
    int ret = 0;
    int idx = 0;
    gid_t *ret_groups = NULL;
    int nret_groups = 0;

    if (user == NULL || group == 0 || ngroups == NULL) {
        goto _getgrlist_err;
    }
    if (strcmp(user, "root") == 0) {
        fprintf(stderr, "FAILED: refuse to lookup groups for root\n");
        goto _getgrlist_err;
    }

    ret = getgrouplist(user, group, NULL, &nret_groups);
    if (ret < 0 && nret_groups > 0) {
        if (nret_groups > 512) {
            fprintf(stderr, "FAILED to get groups, seriously 512 groups is enough!\n");
            goto _getgrlist_err;
        }

        /* allocate and initialize memory to be populated by getgrouplist */
        ret_groups = (gid_t *) malloc(sizeof(gid_t) * (nret_groups + 1));
        if (ret_groups == NULL) {
            fprintf(stderr, "FAILED to reallocate memory for group list\n");
            goto _getgrlist_err;
        }
        memset(ret_groups, 0, sizeof(gid_t) * (nret_groups + 1));

        ret = getgrouplist(user, group, ret_groups, &nret_groups);
        if (ret < 0) {
            fprintf(stderr, "FAILED to get groups correctly\n");
            goto _getgrlist_err;
        }
    }

    /* set default group list if none are found */
    if (nret_groups <= 0) {
        if (ret_groups != NULL) {
            free(ret_groups);
            ret_groups = NULL;
        }
        ret_groups = (gid_t *) malloc(sizeof(gid_t) * 2);
        if (ret_groups == NULL) {
            fprintf(stderr, "FAILED to allocate memory for default group list\n");
            goto _getgrlist_err;
        }
        ret_groups[0] = group;
        ret_groups[1] = 0;
        nret_groups = 1;
    }
    if (ret_groups == NULL) {
        fprintf(stderr, "FAILED: no auxilliary groups found!\n");
        goto _getgrlist_err;
    }

    /* just make sure no zeros snuck in */
    for (idx = 0; idx < nret_groups; idx++) {
        if (ret_groups[idx] == 0) {
            ret_groups[idx] = group;
        }
    }
    *ngroups = nret_groups;
    return ret_groups;

_getgrlist_err:
    if (ret_groups != NULL) {
        free(ret_groups);
        ret_groups = NULL;
    }
    if (ngroups != NULL) {
        *ngroups = 0;
    }
    return NULL;
}

/**
  * startSshd
  * chroots into image and runs the secured sshd
  */
int startSshd(const char *user, UdiRootConfig *udiConfig) {
    char chrootPath[PATH_MAX];
    pid_t pid = 0;

    snprintf(chrootPath, PATH_MAX, "%s", udiConfig->udiMountPoint);
    chrootPath[PATH_MAX - 1] = 0;

    if (chdir(chrootPath) != 0) {
        fprintf(stderr, "FAILED to chdir to %s while attempted to start sshd\n", chrootPath);
        goto _startSshd_unclean;
    }
    if (udiConfig->optionalSshdAsRoot == 0 && (udiConfig->target_uid == 0 ||
                udiConfig->target_gid == 0)) {
        fprintf(stderr, "FAILED to start sshd, will not start as root\n");
        goto _startSshd_unclean;
    }

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "FAILED to fork while attempting to start sshd\n");
        goto _startSshd_unclean;
    }
    if (pid == 0) {
        /* get grouplist in the external environment */
        gid_t *gidList = NULL;
        int nGroups = 0;
        if (udiConfig->optionalSshdAsRoot == 0) {
            gidList = shifter_getgrouplist(user, udiConfig->target_gid, &nGroups);
            if (gidList == NULL) {
                fprintf(stderr, "FAILED to correctly get grouplist for sshd\n");
                exit(1);
            }
        }

        if (chdir(chrootPath) != 0) {
            fprintf(stderr, "FAILED to chdir to %s while attempting to start sshd\n", chrootPath);
            exit(1);
        }
        if (chroot(chrootPath) != 0) {
            fprintf(stderr, "FAILED to chroot to %s while attempting to start sshd\n", chrootPath);
            exit(1);
        }
        if (chdir("/") != 0) {
            fprintf(stderr, "FAILED to chdir following chroot\n");
            exit(1);
        }
        if (udiConfig->optionalSshdAsRoot == 0) {
            if (gidList == NULL) {
                fprintf(stderr, "FAILED to get groupllist for sshd, exiting!\n");
                exit(1);
            }
            if (shifter_set_capability_boundingset_null() != 0) {
                fprintf(stderr, "FAILED to restrict future capabilities\n");
                exit(1);
            }
            if (setgroups(nGroups, gidList) != 0) {
                fprintf(stderr, "FAILED to setgroups(): %s\n", strerror(errno));
                exit(1);
            }
            if (setresgid(udiConfig->target_gid, udiConfig->target_gid,
                        udiConfig->target_gid) != 0) {
                fprintf(stderr, "FAILED to setresgid(): %s\n", strerror(errno));
                exit(1);
            }
            if (setresuid(udiConfig->target_uid, udiConfig->target_uid,
                        udiConfig->target_uid) != 0) {
                fprintf(stderr, "FAILED to setresuid(): %s\n", strerror(errno));
                exit(1);
            }
#if HAVE_DECL_PR_SET_NO_NEW_PRIVS == 1
            /* ensure this process and its heirs cannot gain privilege */
            /* see https://www.kernel.org/doc/Documentation/prctl/no_new_privs.txt */
            if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
                fprintf(stderr, "Failed to fully drop privileges: %s",
                        strerror(errno));
                exit(1);
            }
#endif
        }
        char **sshdArgs = (char **) malloc(sizeof(char *) * 2);
        if (sshdArgs == NULL) {
            fprintf(stderr, "FAILED to exec sshd!\n");
            exit(1);
        }

        sshdArgs[0] = strdup("/opt/udiImage/sbin/sshd");
        sshdArgs[1] = NULL;

        if (sshdArgs[0] == NULL) {
            fprintf(stderr, "FAILED to exec sshd!\n");
            exit(1);
        }
        execv(sshdArgs[0], sshdArgs);
        fprintf(stderr, "FAILED to exec sshd!\n");

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

int _forkAndExecv(char *const *args, int silent) {
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
    if (silent) {
        int devNull = open("/dev/null", O_WRONLY);
        if (devNull < 0) {
            fprintf(stderr, "FAILED to open /dev/null: %s", strerror(errno));
            exit(1);
        }
        dup2(devNull, STDOUT_FILENO);
        dup2(devNull, STDERR_FILENO);
        close(devNull);
    }
    execv(args[0], args);
    fprintf(stderr, "FAILED to execvp! Exiting.\n");
    exit(127);
}

int forkAndExecv(char *const *args) {
    return _forkAndExecv(args, 0);
}

int forkAndExecvSilent(char *const *args) {
    return _forkAndExecv(args, 1);
}

int _shifterCore_bindMount(UdiRootConfig *udiConfig, MountList *mountCache,
        const char *from, const char *to, size_t flags, int overwriteMounts)
{
    int ret = 0;
    char **ptr = NULL;
    char *to_real = NULL;
    unsigned long mountFlags = MS_BIND;
    unsigned long remountFlags = MS_REMOUNT|MS_BIND|MS_NOSUID;
    unsigned long privateRemountFlags = 0;

    if (udiConfig == NULL) {
        fprintf(stderr, "FAILED to provide udiConfig!\n");
        return 1;
    }

    privateRemountFlags = 
        udiConfig->mountPropagationStyle == VOLMAP_FLAG_SLAVE ?
        MS_SLAVE : MS_PRIVATE;

    if (flags & VOLMAP_FLAG_SLAVE) {
        privateRemountFlags = MS_SLAVE;
    } else if (flags & VOLMAP_FLAG_PRIVATE) {
        privateRemountFlags = MS_PRIVATE;
    }

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
            int retry = 0;
            for (retry = 0; retry < BINDMOUNT_OVERWRITE_UNMOUNT_RETRY; retry++) {
                if (unmountTree(mountCache, to_real) != 0) {
                    fprintf(stderr, "%s was already mounted, failed to unmount existing, fail.\n", to_real);
                    ret = 1;
                    goto _bindMount_exit;
                }
                if (validateUnmounted(to_real, 0) == 0) {
                    break;
                }
                usleep(300000); /* sleep for 0.3s */
            }
        } else {
            fprintf(stderr, "%s was already mounted, not allowed to unmount existing, fail.\n", to_real);
            ret = 1;
            goto _bindMount_exit;
        }
    }

    if (strcmp(from, "/dev") == 0 || (flags & VOLMAP_FLAG_RECURSIVE)) {
        mountFlags |= MS_REC;
        remountFlags |= MS_REC;
        privateRemountFlags = MS_PRIVATE|MS_REC;
    }

    /* perform the initial bind-mount */
    ret = mount(from, to_real, "bind", mountFlags, NULL);
    if (ret != 0) {
        goto _bindMount_unclean;
    }
    insert_MountList(mountCache, to_real);

    /* if the source is exactly /dev or starts with /dev/ then
       ALLOW device entires, otherwise remount with noDev */
    if (strcmp(from, "/dev") != 0 && strncmp(from, "/dev/", 5) != 0) {
        remountFlags |= MS_NODEV;
    }

    if (flags & VOLMAP_FLAG_READONLY) {
        remountFlags |= MS_RDONLY;
    }

    /* remount the bind-mount to get the needed mount flags */
    ret = mount(from, to_real, "bind", remountFlags, NULL);
    if (ret != 0) {
        goto _bindMount_unclean;
    }
    if (mount(NULL, to_real, NULL, privateRemountFlags, NULL) != 0) {
        perror("Failed to remount non-shared: ");
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
    }
    if (ret != 0) {
        fprintf(stderr, "ERROR: unclean exit from bind-mount routine. %s may still be mounted.\n", to);
    }
    return 1;
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

    if (fp == NULL) {
        goto _err_valid_args;
    }
    while (!feof(fp) && !ferror(fp)) {
        char *ptr = NULL;
        char *svptr = NULL;
        size_t n = getline(&lineBuffer, &lineBuffer_size, fp);
        if (n == 0 || feof(fp) || ferror(fp) || lineBuffer == NULL) {
            break;
        }
        ptr = strtok_r(lineBuffer, " ", &svptr);
        if (ptr == NULL) goto _err_valid_args;
        ptr = strtok_r(NULL, " ", &svptr);
        if (ptr == NULL) goto _err_valid_args;
        ptr = strtok_r(NULL, " ", &svptr);
        if (ptr == NULL) goto _err_valid_args;
        ptr = strtok_r(NULL, " ", &svptr);
        if (ptr == NULL) goto _err_valid_args;
        ptr = strtok_r(NULL, " ", &svptr);
        if (ptr == NULL) goto _err_valid_args;

        if (strcmp(ptr, mountPoint) == 0) {
            ptr = strtok_r(NULL, "\0", &svptr); /* get rest of line */
            if (strstr(ptr, " shared:") != NULL) {
                rc = 1;
            }
            break;
        }
    }
    fclose(fp);
    fp = NULL;
    if (lineBuffer != NULL) {
        free(lineBuffer);
    }

    return rc;
_err_valid_args:
    if (lineBuffer != NULL) {
        free(lineBuffer);
        lineBuffer = NULL;
    }
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
    return -1;
}

/*! Check if a kernel module is loaded
 *
 * \param name name of kernel module
 *
 * Returns 1 if loaded
 * Returns 0 if not
 * Returns -1 upon error
 */
int isKernelModuleLoaded(const char *name) {
    FILE *fp = NULL;
    char *lineBuffer = NULL;
    size_t lineSize = 0;
    ssize_t nread = 0;
    int loaded = 0;
    if (name == NULL || strlen(name) == 0) {
        return -1;
    }

    fp = fopen("/proc/modules", "r");
    if (fp == NULL) {
        return 1;
    }
    while (!feof(fp) && !ferror(fp)) {
        char *ptr = NULL;
        char *svptr = NULL;
        nread = getline(&lineBuffer, &lineSize, fp);
        if (nread == 0 || feof(fp) || ferror(fp) || lineBuffer == NULL) {
            break;
        }
        ptr = strtok_r(lineBuffer, " ", &svptr);
        if (ptr == NULL) {
            continue;
        }
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
    return loaded;
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
    struct stat statData;
    int ret = 0;

    if (name == NULL || strlen(name) == 0 || path == NULL || strlen(path) == 0 || udiConfig == NULL) {
        return -1;
    }

    if (isKernelModuleLoaded(name)) {
        return 0;
    } else if (udiConfig->autoLoadKernelModule) {
        /* try to load kernel module from system cache */
        char *args[] = {
            strdup(udiConfig->modprobePath),
            strdup(name),
            NULL
        };
        char **argPtr = NULL;
        ret = forkAndExecvSilent(args);
        for (argPtr = args; argPtr && *argPtr; argPtr++) {
            free(*argPtr);
        }
        if (isKernelModuleLoaded(name)) {
            return 0;
        }
    }

    if (udiConfig->kmodPath == NULL
            || strlen(udiConfig->kmodPath) == 0
            || !udiConfig->autoLoadKernelModule)
    {
        return -1;
    }

    /* construct path to kernel modulefile */
    snprintf(kmodPath, PATH_MAX, "%s/%s", udiConfig->kmodPath, path);
    kmodPath[PATH_MAX-1] = 0;

    if (stat(kmodPath, &statData) == 0) {
        char *insmodArgs[] = {
            strdup(udiConfig->insmodPath),
            strdup(kmodPath),
            NULL
        };
        char **argPtr = NULL;

        /* run insmod and clean up */
        ret = forkAndExecvSilent(insmodArgs);
        for (argPtr = insmodArgs; *argPtr != NULL; argPtr++) {
            free(*argPtr);
        }

        if (ret != 0) {
            fprintf(stderr, "FAILED to load kernel module %s (%s); insmod exit status: %d\n", name, kmodPath, ret);
            goto _loadKrnlMod_unclean;
        }
    } else {
        fprintf(stderr, "FAILED to find kernel modules %s (%s)\n", name, kmodPath);
        goto _loadKrnlMod_unclean;
    }
    if (isKernelModuleLoaded(name)) return 0;
    return 1;
_loadKrnlMod_unclean:
    return ret;
}

/** shifter_getpwuid
 *  Lookup user information based on information in shifter passwd cache.
 *  This is useful to avoid making remote getpwuid() calls on Cray systems
 *  or other systems where access to LDAP may be slow, difficult, or impossible
 *  on compute nodes.
 *
 *  If UdiRootConfig parameter allowLibcPwdCalls is set to 1, this function
 *  will simply call the regular getpwuid
 *
 */
struct passwd *shifter_getpwuid(uid_t tgtuid, UdiRootConfig *config) {
    FILE *input = NULL;
    char buffer[PATH_MAX];
    struct passwd *pw = NULL;
    int found = 0;

    if (config == NULL) {
        return NULL;
    }

    if (config->allowLibcPwdCalls == 1) {
        return getpwuid(tgtuid);
    }

    snprintf(buffer, PATH_MAX, "%s/passwd", config->etcPath);
    input = fopen(buffer, "r");

    if (input == NULL) {
        fprintf(stderr, "FAILED to find shifter passwd file at %s", buffer);
        goto _shifter_getpwuid_unclean;
    }
    while ((pw = fgetpwent(input)) != NULL) {
        if (pw->pw_uid == tgtuid) {
            found = 1;
            break;
        }
    }
    fclose(input);
    input = NULL;

    if (found) return pw;
    return NULL;

_shifter_getpwuid_unclean:
    return NULL;
}

struct passwd *shifter_getpwnam(const char *tgtnam, UdiRootConfig *config) {
    FILE *input = NULL;
    char buffer[PATH_MAX];
    struct passwd *pw = NULL;
    int found = 0;

    if (config == NULL) {
        return NULL;
    }
    if (config->allowLibcPwdCalls == 1) {
        return getpwnam(tgtnam);
    }

    snprintf(buffer, PATH_MAX, "%s/passwd", config->etcPath);
    input = fopen(buffer, "r");

    if (input == NULL) {
        fprintf(stderr, "FAILED to find shifter passwd file at %s", buffer);
        goto _shifter_getpwnam_unclean;
    }
    while ((pw = fgetpwent(input)) != NULL) {
        if (strcmp(pw->pw_name, tgtnam) == 0) {
            found = 1;
            break;
        }
    }
    fclose(input);
    input = NULL;

    if (found) return pw;
    return NULL;

_shifter_getpwnam_unclean:
    return NULL;
}

/** filterEtcGroup
 *  many implementations of initgroups() do not deal with huge /etc/group
 *  files due to a variety of limitations (like per-line limits).  this
 *  function reads a given etcgroup file and filters the content to only
 *  include the specified user.  Additionally some services are unwilling
 *  to allow more than, for example, 31 groups from a group file, thus an
 *  upperbound to explicitly include users in is included.  Stub entries
 *  for all remaining groups will be added to provide useful group metadata
 *
 *  \param group_dest_fname filename of filtered group file
 *  \param group_source_fname filename of to-be-filtered group file
 *  \param username user to identify
 *  \param maxGroups maximum number of groups to write out for user, 0 for unlimited
 */
int filterEtcGroup(const char *group_dest_fname, const char *group_source_fname, const char *username, size_t maxGroups) {
    FILE *input = NULL;
    FILE *output = NULL;
    char *linePtr = NULL;
    size_t linePtr_size = 0;
    size_t foundGroups = 0;

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
        char *group_name = NULL;

        char *token = NULL;
        gid_t gid = 0;
        size_t counter = 0;
        int foundUsername = 0;
        if (nread == 0 || linePtr == NULL) break;
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
        if (group_name != NULL && gid != 0) {
            if (foundUsername == 1 && foundGroups < maxGroups) {
                fprintf(output, "%s:x:%d:%s\n", group_name, gid, username);
                foundGroups++;
            } else {
                fprintf(output, "%s:x:%d:\n", group_name, gid);
            }
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
    return 1;
}

pid_t findSshd(void) {
    return shifter_find_process_by_cmdline("/opt/udiImage/sbin/sshd");
}

pid_t shifter_find_process_by_cmdline(const char *command) {
    DIR *proc = NULL;
    struct dirent *dirEntry = NULL;
    char buffer[1024];
    pid_t found = 0;

    if (command == NULL || strlen(command) == 0) {
        return -1;
    }

    proc = opendir("/proc");

    if (proc == NULL) {
        return -1;
    }
    while ((dirEntry = readdir(proc)) != NULL) {
        size_t nread = 0;
        FILE *cmdlineFile = NULL;
        char *filename = NULL;
        pid_t pid = (pid_t) strtol(dirEntry->d_name, NULL, 10);
        if (pid == 0) {
            continue;
        }
        filename = alloc_strgenf("/proc/%d/cmdline", pid);
        if (filename != NULL) {
            cmdlineFile = fopen(filename, "r");
            free(filename);
            filename = NULL;
        }
        if (cmdlineFile == NULL) {
            continue;
        } else if (feof(cmdlineFile) || ferror(cmdlineFile)) {
            fclose(cmdlineFile);
            cmdlineFile = NULL;
            continue;
        }
        nread = fread(buffer, sizeof(char), 1024, cmdlineFile);
        fclose(cmdlineFile);
        cmdlineFile = NULL;

        if (nread > 0) {
            buffer[nread-1] = 0;
            if (strcmp(buffer, command) == 0) {
                found = pid;
                break;
            }
        }
    }
    closedir(proc);
    proc = NULL;
    return found;
}

int killSshd(void) {
    pid_t sshdPid = findSshd();
    if (sshdPid > 2) {
        kill(sshdPid, SIGTERM);
        return 0;
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

    memset(&mounts, 0, sizeof(MountList));
    if (parse_MountList(&mounts) != 0) {
        /*error*/
    }

    snprintf(udiRoot, PATH_MAX, "%s", udiConfig->udiMountPoint);
    udiRoot[PATH_MAX-1] = 0;
    snprintf(loopMount, PATH_MAX, "%s", udiConfig->loopMountPoint);
    loopMount[PATH_MAX-1] = 0;
    for (idx = 0; idx < 10; idx++) {

        if (idx > 0) {
            usleep(300000);
        }
        if (killSsh == 1) {
            killSshd();
        }

        if (unmountTree(&mounts, udiRoot) != 0) {
            continue;
        }
        if (validateUnmounted(udiRoot, 1) != 0) {
            continue;
        }
        if (unmountTree(&mounts, loopMount) != 0) {
            continue;
        }
        if (validateUnmounted(loopMount, 0) != 0) {
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

/*! validate that in this namespace the named path is unmounted */
/*! Constructs a fresh MountList and searches for the specified path; if it is
 * not found, return 0 (success), otherwise return 1 (failure), -1 for error
 */
int validateUnmounted(const char *path, int subtree) {
    MountList mounts;
    int rc = 0;
    memset(&mounts, 0, sizeof(MountList));
    if (parse_MountList(&mounts) != 0) {
        goto _validateUnmounted_error;
    }
    if (subtree) {
        if (findstartswith_MountList(&mounts, path) != NULL) {
            rc = 1;
        }
    } else {
        if (find_MountList(&mounts, path) != NULL) {
            rc = 1;
        }
    }
    free_MountList(&mounts, 0);
    return rc;
_validateUnmounted_error:
    free_MountList(&mounts, 0);
    return -1;
}

static char **_shifter_findenv(char ***env, char *var, size_t n, size_t *nElement) {
    char **ptr = NULL;
    char **ret = NULL;
    if (env == NULL || *env == NULL || var == NULL || n == 0) {
        return NULL;
    }
    if (nElement != NULL) {
        *nElement = 0;
    }
    for (ptr = *env; ptr && *ptr; ptr++) {
        if (strncmp(*ptr, var, n) == 0) {
            ret = ptr;
        }
        if (nElement != NULL) {
            (*nElement)++;
        }
    }
    return ret;
}

static int _shifter_unsetenv(char ***env, char *var) {
    size_t namelen = 0;
    size_t envsize = 0;
    char **pptr = NULL;
    if (env == NULL || *env == NULL || var == NULL) {
        return 1;
    }

    namelen = strlen(var);

    /* find the needed environment variable */
    pptr = _shifter_findenv(env, var, namelen, &envsize);

    /* if it is found, remove the entry by shifting the array */
    /* this relies on the env array being NULL terminated */
    /* TODO decide if the original *pptr should be freed */
    if (pptr != NULL) {
        char **nptr = NULL;
        for (nptr = pptr + 1; pptr && *pptr; nptr++, pptr++) {
            *pptr = *nptr;
        }
    }
    return 0;
}

static int _shifter_putenv(char ***env, char *var, int mode) {
    size_t namelen = 0;
    size_t envsize = 0;
    char *ptr = NULL;
    char **pptr = NULL;
    if (env == NULL || *env == NULL || var == NULL) {
        return 1;
    }
    ptr = strchr(var, '=');
    if (ptr == NULL) {
        return 1;
    }
    namelen = ptr - var;
    pptr = _shifter_findenv(env, var, namelen, &envsize);
    if (pptr != NULL) {
        char *value = strchr(*pptr, '=');
        if (value != NULL) {
            value++;
            if (*value == 0) {
                value = NULL;
            }
        }
        if (mode == 0) {
            /* replace */
            *pptr = strdup(var);
            return 0;
        } else if (mode == 1) {
            /* prepend */
            char *newptr = NULL;
            if (value == NULL) {
                *pptr = strdup(var);
                return 0;
            }

            newptr = alloc_strgenf("%s:%s", var, value);
            *pptr = newptr;
            return 0;
        } else if (mode == 2) {
            /* append */
            char *newptr = NULL;

            if (value == NULL) {
                *pptr = strdup(var);
                return 0;
            }
            newptr = alloc_strgenf("%s:%s", *pptr, (var + namelen + 1));
            *pptr = newptr;
            return 0;
        } else {
            return 1;
        }
    }
    char **tmp = realloc(*env, sizeof(char *) * (envsize + 2));
    if (tmp != NULL) {
        *env = tmp;
    } else {
        return 1;
    }
    tmp[envsize] = strdup(var);
    tmp[envsize+1] = NULL;
    return 0;
}

extern char **environ;
char **shifter_copyenv(void) {
    char **outenv = NULL;
    char **ptr = NULL;
    char **wptr = NULL;

    if (environ == NULL) {
        return NULL;
    }

    for (ptr = environ; *ptr != NULL; ++ptr) {
    }
    outenv = (char **) malloc(sizeof(char*) * ((ptr - environ) + 1));
    ptr = environ;
    wptr = outenv;
    for ( ; *ptr != NULL; ptr++) {
        *wptr++ = strdup(*ptr);
    }
    *wptr = NULL;
    return outenv;
}

int shifter_putenv(char ***env, char *var) {
    return _shifter_putenv(env, var, 0);
}

int shifter_appendenv(char ***env, char *var) {
    return _shifter_putenv(env, var, 2);
}

int shifter_prependenv(char ***env, char *var) {
    return _shifter_putenv(env, var, 1);
}

int shifter_unsetenv(char ***env, char *var) {
    return _shifter_unsetenv(env, var);
}

int shifter_setupenv(char ***env, ImageData *image, UdiRootConfig *udiConfig) {
    char **envPtr = NULL;
    if (env == NULL || *env == NULL || image == NULL || udiConfig == NULL) {
        return 1;
    }
    for (envPtr = image->env; envPtr && *envPtr; envPtr++) {
        shifter_putenv(env, *envPtr);
    }
    for (envPtr = udiConfig->siteEnv; envPtr && *envPtr; envPtr++) {
        shifter_putenv(env, *envPtr);
    }
    for (envPtr = udiConfig->siteEnvAppend; envPtr && *envPtr; envPtr++) {
        shifter_appendenv(env, *envPtr);
    }
    for (envPtr = udiConfig->siteEnvPrepend; envPtr && *envPtr; envPtr++) {
        shifter_prependenv(env, *envPtr);
    }
    for (envPtr = udiConfig->siteEnvUnset; envPtr && *envPtr; envPtr++) {
        shifter_unsetenv(env, *envPtr);
    }
    return 0;
}

int _shifter_get_max_capability(unsigned long *_maxCap) {
    unsigned long maxCap = CAP_LAST_CAP;
    unsigned long idxCap = 0;

    if (_maxCap == NULL) {
        return 1;
    }

    /* starting in Linux 3.2 this file will proclaim the "last" capability
     * read it to see if the current kernel has more capabilities than shifter
     * was compiled with */
    if (access("/proc/sys/kernel/cap_last_cap", R_OK) == 0) {
        FILE *fp = fopen("/proc/sys/kernel/cap_last_cap", "r");
        char buffer[1024];
        if (fp == NULL) {
            fprintf(stderr, "FAILED to determine capability max val\n");
            return 1;
        }
        size_t bytes = fread(buffer, sizeof(char), 1024, fp);
        if (bytes > 0 && bytes < 100) {
            unsigned long tmp = strtoul(buffer, NULL, 10);
            if (tmp > CAP_LAST_CAP) {
                maxCap = tmp;
            }
        } else {
            fprintf(stderr, "FAILED to determine capability max val\n");
            fclose(fp);
            return 1;
        }
        fclose(fp);
        *_maxCap = maxCap;
        return 0;
    }

    /* if we couldn't read it from kernel, try to find maximum processively */
    for (idxCap = 0; idxCap < 100; idxCap++) {
        int ret = prctl(PR_CAPBSET_READ, idxCap, 0, 0, 0);
        if (ret < 0) {
            if (idxCap > 0 && errno == EINVAL) {
                /* use idxCap - 1 because idxCap refers to an invalid
                   capability */
                *_maxCap = idxCap - 1;
                return 0;
            }
            return 1;
        }
    }

    return 1;
}


int shifter_set_capability_boundingset_null() {
    unsigned long maxCap = CAP_LAST_CAP;
    unsigned long idx = 0;
    int ret = 0;
    unsigned long possibleMaxCap = 0;

    if (_shifter_get_max_capability(&possibleMaxCap) != 0) {
        fprintf(stderr, "FAILED to determine capability max val\n");
        return 1;
    }

    if (possibleMaxCap > maxCap) {
        maxCap = possibleMaxCap;
    }

    if (maxCap >= 100) {
        fprintf(stderr, "FAILED: max cap seems too high (%lu)\n", maxCap);
        return 1;
    }

    /* calling PR_CAPBSET_DROP for all possible capabilities is intended to
     * prevent any mechanism from allowing processes running within shifter
     * from gaining privileges; in particular if a file has capabilities that
     * it can offer */
    for (idx = 0; idx <= maxCap; idx++) {
        if (prctl(PR_CAPBSET_DROP, idx, 0, 0, 0) != 0) {
            fprintf(stderr, "Failed to drop capability %lu\n", idx);
            ret = 1;
        }
    }
    return ret;
}
