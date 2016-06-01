/* Shifter, Copyright (c) 2016, The Regents of the University of California,
 through Lawrence Berkeley National Laboratory (subject to receipt of any
 required approvals from the U.S. Dept. of Energy).  All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
  3. Neither the name of the University of California, Lawrence Berkeley
     National Laboratory, U.S. Dept. of Energy nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

See LICENSE for full text.
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "shifterSpank.h"

/******************************************************************************
* wrapper interfaces into shifterSpank
******************************************************************************/
int wrap_opt_ccm(int val, const char *optarg, int remote) {
    return SUCCESS;
}
int wrap_opt_image(int val, const char *optarg, int remote) {
    return SUCCESS;
}
int wrap_opt_volume(int val, const char *optarg, int remote) {
    return SUCCESS;
}

int wrap_force_arg_parse(shifterSpank_config *ssconfig) {
    return SUCCESS;
}

/******************************************************************************
* wrappers for SPANK functionality
******************************************************************************/
int wrap_spank_setenv(
    shifterSpank_config *ssconfig, const char *envname, const char *value, int overwrite)
{
    return SUCCESS;
}

int wrap_spank_job_control_setenv(
    shifterSpank_config *ssconfig,
    const char *envname,
    const char *value,
    int overwrite)
{
    return SUCCESS;
}

int wrap_spank_get_jobid(shifterSpank_config *ssconfig, uint32_t *job) {
    *job = 1;
    return SUCCESS;
}

int wrap_spank_get_uid(shifterSpank_config *ssconfig, uid_t *uid) {
    *uid = getuid();
    return SUCCESS;
}

int wrap_spank_get_gid(shifterSpank_config *ssconfig, gid_t *gid) {
    *gid = getgid();
    return SUCCESS;
}

int wrap_spank_get_stepid(shifterSpank_config *ssconfig, uint32_t *stepid) {
    return SUCCESS;
}

int wrap_spank_get_stepid_noconfig(void *sp, uint32_t *stepid) {
    return SUCCESS;
}

int wrap_spank_get_supplementary_gids(
    shifterSpank_config *ssconfig,
    gid_t **gids,
    int *ngids)
{
    return SUCCESS;
}

void wrap_spank_log_error(const char *msg) {
     fprintf(stderr, "MockLogError: %s\n", msg);
}

void wrap_spank_log_info(const char *msg) {
     fprintf(stderr, "MockLogInfo: %s\n", msg);
}

void wrap_spank_log_verbose(const char *msg) {
     fprintf(stderr, "MockLogVerbose: %s\n", msg);
}

void wrap_spank_log_debug(const char *msg) {
     fprintf(stderr, "MockLogDebug: %s\n", msg);
}

/******************************************************************************
* segment interfacing with libslurm beyond the spank interface
******************************************************************************/

int wrap_spank_stepd_connect(shifterSpank_config *ssconfig, char *dir,
    char *hostname, uint32_t jobid, uint32_t stepid, uint16_t *protocol)
{
    return 0;
}

int wrap_spank_stepd_add_extern_pid(shifterSpank_config *ssconfig,
    uint32_t stepd_fd, uint16_t protocol, pid_t pid)
{
    return SUCCESS;
}

int wrap_spank_extra_job_attributes(
    shifterSpank_config *ssconfig,
    uint32_t jobid,
    char **nodelist,
    size_t *nnodes,
    size_t *tasksPerNode,
    uint16_t *shared)
{
    /* get tasksPerNode */
    *tasksPerNode = 32;

    /* get shared-node status */
    *shared = 0;

    /* get number of nodes in allocation */
    *nnodes = 2;

    /* nid00[1-4] -> nid001,nid002,nid003,nid004 */
    *nodelist = strdup("nid00001,nid00002");

    return SUCCESS;
}
