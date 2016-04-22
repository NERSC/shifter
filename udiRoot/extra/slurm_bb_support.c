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

#include "shifter_core.h"

extern int _shifterCore_bindMount(UdiRootConfig *confg, MountList *mounts, const char *from, const char *to, size_t flags, int overwrite);

int main(int argc, char **argv) {
    UdiRootConfig config;
    MountList mounts;
    char dwMountPoint[PATH_MAX];
    memset(&config, 0, sizeof(UdiRootConfig));
    memset(&mounts, 0, sizeof(MountList));
    if (parse_MountList(&mounts) != 0) {
        /* error */
    }
    if (parse_UdiRootConfig(CONFIG_FILE, &config, UDIROOT_VAL_ALL) != 0) {
        fprintf(stderr, "FAILED to parse udiRoot configuration.\n");
        exit(1);
    }

    snprintf(dwMountPoint, PATH_MAX, "%s/var/opt/cray/dws", config.udiMountPoint);

    unmountTree(&mounts, dwMountPoint);

    _shifterCore_bindMount(&config, &mounts, "/var/opt/cray/dws", dwMountPoint, VOLMAP_FLAG_RECURSIVE, 1);

    return 0;
}

