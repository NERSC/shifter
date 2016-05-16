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

#include <slurm/spank.h>
#include <shifterSpank.h>

SPANK_PLUGIN(shifter, 1)

int wrap_opt_ccm(int val, const char *optarg, int remote);
int wrap_opt_image(int val, const char *optarg, int remote);
int wrap_opt_volume(int val, const char *optarg, int remote);

/* global variable used by spank to get plugin options */
struct spank_option spank_option_array[] = {
    { "image", "image", "shifter image to use", 1, 0,
      (spank_opt_cb_f) wrap_opt_image},
    { "volume", "volume", "shifter image bindings", 1, 0,
      (spank_opt_cb_f) wrap_opt_volume },
    { "ccm", "ccm", "ccm emulation mode", 0, 0,
      (spank_opt_cb_f) wrap_opt_ccm},
    SPANK_OPTIONS_TABLE_END
};

static shifterSpank_config *ssconfig = NULL;


/******************************************************************************
* SPANK interface functions to SLURM for shifter
* 
* slurm_spank_init:           used to initialize data structures
* slurm_spank_init_post_opt:  executed in some contexts after host program
*                             parses command line arguments
* slurm_spank_task_post_fork: executed after tasks are launched
* slurm_spank_job_prolog:     executed during node allocation of a job
* slurm_spank_job_epilog:     executed during job teardown
*******************************************************************************/

int slurm_spank_init(spank_t sp, int argc, char **argv) {
    spank_context_t context = spank_context();

    if (context == S_CTX_ALLOCATOR ||
        context == S_CTX_LOCAL ||
        context == S_CTX_REMOTE)
    {
        int idx = 0;

        /* need to initialize ssconfig for callbacks */
        if (ssconfig == NULL)
            ssconfig = shifterSpank_init((void *) sp, argc, argv, 0);

        /* register command line options */
        for (idx = 0; spank_option_array[idx] != SPANK_OPTIONS_TABLE_END; idx++) {
            int lrc = spank_option_register(sp, &(spank_option_array[idx]));
            if (lrc != ESPANK_SUCCESS) {
                rc = ESPANK_ERROR;
            }
        }
    }
    return rc;
}

int slurm_spank_init_post_opt(spank_t sp, int argc, char **argv) {
    spank_context_t context = spank_context();
    int rc = SUCCESS;

    shifterSpank_validate_input(
        context == S_CTX_ALLOCATOR | context == S_CTX_LOCAL
    );

    if (context == S_CTX_ALLOCATOR || context == S_CTX_LOCAL) {
        rc = shifterSpank_init_allocator_setup();
    }
    if (rc != SUCCESS) return ESPANK_ERROR;
    return ESPANK_SUCCESS;
}
    
int slurm_spank_task_post_fork(spank_t sp, int argc, char **argv) {
    return shifterSpank_task_post_fork((void *) sp, argc, argv);
}

int slurm_spank_job_prolog(spank_t sp, int argc, char **argv) {
    if (ssconfig == NULL)
         ssconfig = shifterSlurm_init((void *) sp, argc, argv, 1);
    if (ssconfig == NULL) return ESPANK_ERROR;
    if (ssconfig->parsedOptions == 0)
        if (_forceParseOptions() != SUCCESS)
            return ESPANK_ERROR;
    return shifterSpank_prolog(ssconfig);
    
}
int slurm_spank_job_epilog(spank_t sp, int argc, char **argv) {
    if (ssconfig == NULL)
         ssconfig = shifterSlurm_init((void *) sp, argc, argv, 1);
    if (ssconfig == NULL) return ESPANK_ERROR;
    if (ssconfig->parsedOptions == 0)
        if (wrap_forceParseOptions() != SUCCESS)
            return ESPANK_ERROR;
    return shifterSpank_epilog(ssconfig);
}

/******************************************************************************
* wrapper interfaces into shifterSpank
******************************************************************************/
int wrap_opt_ccm(int val, const char *optarg, int remote) {
    return shifterSpank_process_option_ccm(ssconfig, val, optarg, remote);
}
int wrap_opt_image(int val, const char *optarg, int remote) {
    return shifterSpank_process_option_image(ssconfig, val, optarg, remote);
}
int wrap_opt_volume(int val, const char *optarg, int remote) {
    return shifterSpank_process_option_volume(ssconfig, val, optarg, remote);
}

int wrap_force_arg_parse(shifterSpank_config *ssconfig) {
    int i,j;
    int rc = SUCCESS;
    for (i = 0; spank_option_array[i].name != NULL; ++i) {
        char *optarg = NULL;
        j = spank_option_getopt(sp, &spank_option_array[i], &optarg);
        if (j != ESPANK_SUCCESS) {
            continue;
        }
        rc = (spank_option_array[i].cb)(spank_option_array[i].val, optarg, 1);
        if (rc != ESPANK_SUCCESS) break;
    }
    if (rc == ESPANK_SUCCESS) return SUCCESS;
    return ERROR;
}

/******************************************************************************
* wrappers for SPANK functionality
******************************************************************************/
int wrap_spank_setenv(
    shifterSpank_config *ssconfig, const char *envname, const char *value, int overwrite)
{
    if (ssconfig == NULL || ssconfig->id == NULL) return ERROR;
    return spank_setenv((spank_t) ssconfig->id, envname, value, overwrite);
}

int wrap_spank_job_control_setenv(
    shifterSlurm_config *ssconfig,
    const char *envname,
    const char *value,
    int overwrite)
{
    if (ssconfig == NULL || ssconfig->id == NULL) return ERROR;
    return spank_job_control_setenv(
                (spank_t) ssconfig->id, envname, value, overwrite);
}

int wrap_spank_get_jobid(shifterSlurm_config *ssconfig, uint32_t *job) {
    if (ssconfig == NULL || ssconfig->id == NULL) return ERROR;
    if (spank_get_item((spank_t) ssconfig->id, S_JOB_ID, job)
            != ESPANK_SUCCESS)
    {
        return ERROR;
    }
    return SUCCESS;
}

int wrap_spank_get_uid(shifterSlurm_config *ssconfig, uid_t *uid) {
    if (ssconfig == NULL || ssconfig->id == NULL) return ERROR;
    if (spank_get_item((spank_t) ssconfig->id, S_JOB_UID, uid)
            != ESPANK_SUCCESS) {
       return ERROR;
   }
   return SUCCESS;
}

int wrap_spank_get_gid(shifterSlurm_config *ssconfig, gid_t *gid) {
    if (ssconfig == NULL || ssconfig->id == NULL) return ERROR;
    if (spank_get_item((spank_t) ssconfig->id, S_JOB_GID, gid)
            != ESPANK_SUCCESS) {
        return ERROR;
    }
    return SUCCESS;
}

int wrap_spank_get_stepid(shifterSlurm_config *ssconfig, uint32_t *stepid) {
    if (ssconfig == NULL || ssconfig->id == NULL) return ERROR;
    if (spank_get_item((spank_t) ssconfig->id, S_JOB_STEPID, stepid)
             != ESPANK_SUCCESS) {
        return ERROR;
    }
    return SUCCESS;
}

int wrap_spank_get_supplementary_gids(
    shifterSlurm_config *ssconfig,
    gid_t **gids,
    int *ngids)
{
    if (ssconfig == NULL || ssconfig->id == NULL) return ERROR;
    if (spank_get_item((spank_t) spid, S_JOB_SUPPLEMENTARY_GIDS, gids, ngids)
            != ESPANK_SUCCESS) {
        return ERROR;
    }
    return SUCCESS;
}

void wrap_spank_log_error(const char *msg) {
     slurm_error("%s", msg);
}

void wrap_spank_log_info(const char *msg) {
     slurm_info("%s", msg);
}

void wrap_spank_log_verbose(const char *msg) {
     slurm_verbose("%s", msg);
}

void wrap_spank_log_debug(const char *msg) {
     slurm_debug("%s", msg);
}

/******************************************************************************
* segment interfacing with libslurm beyond the spank interface
******************************************************************************/

int wrap_spank_stepd_connect(shifterSpank_config *ssconfig, char *dir,
    char *hostname, uint32_t jobid, uint32_t stepid, uint16_t *protocol)
{
    /* TODO dlopen libslurm here */
    return slurm_stepd_connect(dir, hostname, jobid, stepid, protocol);
}

int wrap_spank_stepd_add_extern_pid(shifterSpank_config *ssconfig,
    uint32_t stepd_fd, uint16_t protocol, pid_t pid)
{
    /* TODO dlopen libslurm here */
    return slurm_stepd_add_extern_pid(stepd_fd, protocol, pid);
}

int wrap_spank_extra_job_attributes(
    shifterSpank_config *ssconfig,
    uint32_t jobid,
    char **nodelist,
    size_t *nnodes,
    size_t *tasksPerNode,
    uint16_t *shared)
{
    job_info_msg_t *job_buf = NULL;
    hostlist_t hl;
    char *raw_host_string = NULL;

    /* TODO dlopen libslurm here */
    if (slurm_load_job(&job_buf, *jobid, SHOW_ALL) != 0) {
        slurm_error("%s", "Couldn't load job data");
        return ERROR;
    }
    if (job_buf->record_count != 1) {
        slurm_error("%s", "Can't deal with this job!");
        slurm_free_job_info_msg(job_buf);
        return ERROR;
    }

    /* get tasksPerNode */
    *tasksPerNode = job_buf->job_array->num_cpus / job_buf->job_array->num_nodes;

    /* get shared-node status */
    *shared = job_buf->job_array->shared;

    /* get number of nodes in allocation */
    *nnodes = job_buf->job_array->num_nodes;

    /* obtain the hostlist for this job */
    hl = slurm_hostlist_create_dims(job_buf->job_array->nodes, 0);
    slurm_hostlist_uniq(hl);

    /* convert hostlist to exploded string */
    /* nid00[1-4] -> nid001,nid002,nid003,nid004 */
    *nodelist = slurm_hostlist_deranged_string_malloc(hl);

    slurm_free_job_info_msg(job_buf);
    slurm_hostlist_destroy(hl);

    return SUCCESS;
}
