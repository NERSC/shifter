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

int wrap_spank_setenv(void *spid, const char *envname,
                      const char *value, int overwrite);

int wrap_spank_job_control_setenv(void *spid, const char *envname,
                                  const char *value, int overwrite);

int wrap_spank_get_jobid(void *spid, uint32_t *job);
int wrap_spank_get_uid(void *spid, uid_t *uid);
int wrap_spank_get_gid(void *spid, gid_t *gid);
int wrap_spank_get_stepid(void *spid, uint32_t *stepid);
int wrap_spank_get_supplementary_gids(void *spid, gid_t **gids, int *ngids);
void wrap_spank_log_error(const char *msg);
void wrap_spank_log_info(const char *msg);
void wrap_spank_log_verbose(const char *msg);
void wrap_spank_log_debug(const char *msg);

void wrap_spank_extra_job_attributes(uint32_t *jobid,
                                     char **nodelist,
                                     size_t *nnodes,
                                     size_t *tasksPerNode,
                                     uint16_t *shared);

#ifdef __cplusplus
}
#endif

#endif 
