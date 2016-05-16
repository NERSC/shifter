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

#ifndef __SHIFTERSPANK_WRAPPER
#define __SHIFTERSPANK_WRAPPER

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <slurm/slurm.h>
#include <slurm/spank.h>

int wrap_spank_setenv(shifterSpank_config *, const char *envname,
                      const char *value, int overwrite);

int wrap_spank_job_control_setenv(shifterSpank_config *, const char *envname,
                                  const char *value, int overwrite);

int wrap_spank_get_jobid(shifterSpank_config *, uint32_t *job);
int wrap_spank_get_uid(shifterSpank_config *, uid_t *uid);
int wrap_spank_get_gid(shifterSpank_config *, gid_t *gid);
int wrap_spank_get_stepid(shifterSpank_config *, uint32_t *stepid);
int wrap_spank_get_supplementary_gids(shifterSpank_config *, gid_t **gids, int *ngids);
void wrap_spank_log_error(const char *msg);
void wrap_spank_log_info(const char *msg);
void wrap_spank_log_verbose(const char *msg);
void wrap_spank_log_debug(const char *msg);

int wrap_spank_stepd_connect(shifterSpank_config *ssconfig, char *dir,
    char *hostname, uint32_t jobid, uint32_t stepid, uint16_t *protocol);
int wrap_spank_stepd_add_extern_pid(shifterSpank_config *ssconfig,
    uint32_t stepd_fd, uint16_t protocol, pid_t pid);
int wrap_spank_extra_job_attributes(shifterSpank_config *ssconfig,
                                    uint32_t *jobid,
                                    char **nodelist,
                                    size_t *nnodes,
                                    size_t *tasksPerNode,
                                    uint16_t *shared);

int wrap_force_arg_parse(shifterSpank_config *ssconfig);

#ifdef __cplusplus
}
#endif

#endif 
