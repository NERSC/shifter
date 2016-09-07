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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>

#include "UdiRootConfig.h"
#include "shifter_core.h"
#include "utility.h"

#include "shifterSpank.h"
#include "wrapper.h"

typedef enum {
    LOG_ERROR = 0,
    LOG_INFO,
    LOG_VERBOSE,
    LOG_DEBUG
} logLevel;

static void _log(logLevel level, const char *format, ...) {
    char buffer[1024];
    char *dbuffer = NULL;
    char *bufptr = buffer;
    int bytes = 0;
    va_list ap;
    
    va_start(ap, format);
    bytes = vsnprintf(buffer, 1024, format, ap);
    va_end(ap);

    if (bytes > 1024) {
        dbuffer = (char *) malloc(sizeof(char) * (bytes + 2));
        va_list ap;
        va_start(ap, format);
        vsnprintf(dbuffer, (bytes + 2), format, ap);
        va_end(ap);
        bufptr = dbuffer;
    }

    switch(level) {
        case LOG_ERROR:   wrap_spank_log_error(bufptr);   break;
        case LOG_INFO:    wrap_spank_log_info(bufptr);    break;
        case LOG_VERBOSE: wrap_spank_log_verbose(bufptr); break;
        case LOG_DEBUG:   wrap_spank_log_debug(bufptr);   break;
    }

    if (dbuffer != NULL) {
        free(dbuffer);
        dbuffer = NULL;
    }
}

shifterSpank_config *shifterSpank_init(
    void *spid, int argc, char **argv, int getoptValues)
{
    int idx = 0;
    char buffer[PATH_MAX];
    shifterSpank_config *ssconfig = NULL;

    ssconfig = (shifterSpank_config *) malloc(sizeof(shifterSpank_config));
    if (ssconfig == NULL) {
        _log(LOG_ERROR, "FAILED to allocate memory for shifterSpank_config");
        return NULL;
    }
    memset(ssconfig, 0, sizeof(shifterSpank_config));

    ssconfig->id = spid;

    for (idx = 0; idx < argc; ++idx) {
        if (strncmp("shifter_config=", argv[idx], 15) == 0) {
            char *ptr = argv[idx] + 15;
            snprintf(buffer, PATH_MAX, "%s", ptr);
            ptr = shifter_trim(buffer);
            ssconfig->shifter_config = strdup(ptr);
        } else if (strncasecmp("extern_setup=", argv[idx], 13) == 0) {
            char *ptr = argv[idx] + 13;
            snprintf(buffer, PATH_MAX, "%s", ptr);
            ptr = shifter_trim(buffer);
            ssconfig->extern_setup = strdup(ptr);
        } else if (strncasecmp("extern_cgroup=", argv[idx], 14) == 0) {
            char *ptr = argv[idx] + 14;
            ssconfig->extern_cgroup = (int) strtol(ptr, NULL, 10);
        } else if (strncasecmp("memory_cgroup=", argv[idx], 14) == 0) {
            char *ptr = argv[idx] + 14;
            snprintf(buffer, PATH_MAX, "%s", ptr);
            ptr = shifter_trim(buffer);
            ssconfig->memory_cgroup = strdup(ptr);
        } else if (strncasecmp("enable_ccm=", argv[idx], 11) == 0) {
            char *ptr = argv[idx] + 11;
            ssconfig->ccmEnabled = (int) strtol(ptr, NULL, 10);
        } else if (strncasecmp("enable_sshd=", argv[idx], 12) == 0) {
            char *ptr = argv[idx] + 12;
            ssconfig->sshdEnabled = (int) strtol(ptr, NULL, 10);
        }
    }

    if (ssconfig->shifter_config == NULL) {
        ssconfig->shifter_config = strdup(CONFIG_FILE);
    }
    if (ssconfig->shifter_config == NULL) {
        _log(LOG_ERROR, "shifterSlurm: failed to find config filename");
        goto error;
    }

    ssconfig->udiConfig = malloc(sizeof(UdiRootConfig));
    if (ssconfig->udiConfig == NULL) {
        _log(LOG_ERROR, "FAILED to allocate memory to read "
            "udiRoot configuration\n");
        goto error;
    }
    memset(ssconfig->udiConfig, 0, sizeof(UdiRootConfig));

    if (parse_UdiRootConfig(ssconfig->shifter_config, ssconfig->udiConfig, UDIROOT_VAL_ALL) != 0) {
        _log(LOG_ERROR, "FAILED to read udiRoot configuration file!\n");
        free(ssconfig->udiConfig);
        ssconfig->udiConfig = NULL;
        goto error;
    }

    if (getoptValues) {
        if (wrap_force_arg_parse(ssconfig) == SUCCESS) {
            shifterSpank_validate_input(ssconfig, 0);
        }
    }

    return ssconfig;
error:
    if (ssconfig != NULL) {
        shifterSpank_config_free(ssconfig);
        ssconfig = NULL;
    }
    return NULL;
}

void shifterSpank_config_free(shifterSpank_config *ssconfig) {
    if (ssconfig == NULL) return;
    if (ssconfig->image != NULL) {
        free(ssconfig->image);
        ssconfig->image = NULL;
    }
    if (ssconfig->imageType != NULL) {
        free(ssconfig->imageType);
        ssconfig->imageType = NULL;
    }
    if (ssconfig->volume != NULL) {
        free(ssconfig->volume);
        ssconfig->volume = NULL;
    }
    if (ssconfig->udiConfig != NULL) {
        free_UdiRootConfig(ssconfig->udiConfig, 1);
        ssconfig->udiConfig = NULL;
    }
    if (ssconfig->shifter_config != NULL) {
        free(ssconfig->shifter_config);
        ssconfig->shifter_config = NULL;
    }
    if (ssconfig->memory_cgroup != NULL) {
        free(ssconfig->memory_cgroup);
        ssconfig->memory_cgroup = NULL;
    }
    if (ssconfig->extern_setup != NULL) {
        free(ssconfig->extern_setup);
        ssconfig->extern_setup = NULL;
    }
    memset(ssconfig, 0, sizeof(shifterSpank_config));
    free(ssconfig);
}

int shifterSpank_process_option_ccm(
    shifterSpank_config *ssconfig, int val, const char *optarg, int remote)
{
    if (ssconfig == NULL) return ERROR;

    if (ssconfig->ccmEnabled) {
        ssconfig->ccmMode = 1;
    }
    if (ssconfig->image == NULL) ssconfig->image = strdup("/");
    if (ssconfig->imageType == NULL) ssconfig->imageType = strdup("local");
    return SUCCESS;
}

int shifterSpank_process_option_image(
    shifterSpank_config *ssconfig, int val, const char *optarg, int remote)
{
    char *tmp = NULL;
    int rc = SUCCESS;
    if (optarg != NULL && strlen(optarg) > 0) {
        char *type = NULL;
        char *tag = NULL;
        tmp = strdup(optarg);
        if (parse_ImageDescriptor(tmp, &type, &tag, ssconfig->udiConfig) != 0) {
            _log(LOG_ERROR, "Invalid image input: could not determine image " 
                    "type: %s", optarg);
            rc = ERROR;
            goto _opt_image_exit;
        }
        if (ssconfig->imageType != NULL) free(ssconfig->imageType);
        if (ssconfig->image != NULL) free(ssconfig->image);
        ssconfig->imageType = type;
        ssconfig->image = tag;
        free(tmp);
        tmp = NULL;
        return SUCCESS;
    }
    _log(LOG_ERROR, "Invalid image - must not be zero length");
_opt_image_exit:
    if (tmp != NULL) {
        free(tmp);
    }
    return rc;
}

int shifterSpank_process_option_volume(
    shifterSpank_config *ssconfig, int val, const char *optarg, int remote)
{
    if (optarg != NULL && strlen(optarg) > 0) {
        /* validate input */
        VolumeMap *vmap = (VolumeMap *) malloc(sizeof(VolumeMap));
        memset(vmap, 0, sizeof(VolumeMap));

        if (parseVolumeMap(optarg, vmap) != 0) {
            _log(LOG_ERROR, "Failed to parse or invalid/disallowed volume map request: %s\n", optarg);
            free_VolumeMap(vmap, 1);
            exit(1);
        }
        free_VolumeMap(vmap, 1);
        if (ssconfig->volume != NULL) {
            char *tmpvol = alloc_strgenf("%s;%s", ssconfig->volume, optarg);
            free(ssconfig->volume);
            ssconfig->volume = tmpvol;
        } else {
            ssconfig->volume = strdup(optarg);
        }

        return SUCCESS;
    }
    _log(LOG_ERROR, "Invalid image volume options - if specified, must not be zero length");
    return ERROR;
}

int forkAndExecvLogToSlurm(const char *appname, char **args) {
    int rc = 0;
    pid_t pid = 0;

    /* pipes for reading from setupRoot */
    int stdoutPipe[2];
    int stderrPipe[2];

    if (pipe(stdoutPipe) != 0) {
        _log(LOG_ERROR, "FAILED to open stdout pipe! %s", strerror(errno));
        rc = ERROR;
        goto endf;
    }
    if (pipe(stderrPipe) != 0) {
        _log(LOG_ERROR, "FAILED to open stderr pipe! %s", strerror(errno));
        rc = ERROR;
        goto endf;
    }
    pid = fork();
    if (pid < 0) {
        _log(LOG_ERROR, "FAILED to fork %s", appname);
        rc = ERROR;
        goto endf;
    } else if (pid > 0) {
        int status = 0;
        FILE *stdoutStream = NULL;
        FILE *stderrStream = NULL;
        char *lineBuffer = NULL;
        size_t lineBuffer_sz = 0;


        /* close the write end of both pipes */
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        stdoutStream = fdopen(stdoutPipe[0], "r");
        stderrStream = fdopen(stderrPipe[0], "r");

        for ( ; stdoutStream && stderrStream ; ) {
            if (stdoutStream) {
                ssize_t nBytes = getline(&lineBuffer, &lineBuffer_sz, stdoutStream);
                if (nBytes > 0) {
                    _log(LOG_ERROR, "%s stdout: %s", appname, lineBuffer);
                } else {
                    fclose(stdoutStream);
                    stdoutStream = NULL;
                }
            }
            if (stderrStream) {
                ssize_t nBytes = getline(&lineBuffer, &lineBuffer_sz, stderrStream);
                if (nBytes > 0) {
                    _log(LOG_ERROR, "%s stderr: %s", appname, lineBuffer);
                } else {
                    fclose(stderrStream);
                    stderrStream = NULL;
                }
            }
        }

        /* wait on the child */
        _log(LOG_ERROR, "waiting on %s\n", appname);
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
             rc = WEXITSTATUS(status);
        } else {
             rc = 1;
        }
        if (status != 0) {
            _log(LOG_ERROR, "FAILED to run %s", appname);
            rc = ERROR;
            goto endf;
        }
    } else {
        /* close the read end of both pipes */
        close(stdoutPipe[0]);
        close(stderrPipe[0]);

        /* make the pipe stdout/err */
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);


        execv(args[0], args);
        exit(127);
    }
endf:
    return rc;
}


/** generateSshKey
 *  checks to see if a udiRoot-specific ssh key exists and creates one if
 *  necessary.  Once an appropriate key exists, the public key is read in
 *  and set in the job_control environment for retrieval in the job prolog
 **/
int generateSshKey(shifterSpank_config *ssconfig) {
    struct stat st_data;

    char filename[1024];
    char buffer[4096];
    struct passwd pwd;
    struct passwd *ptr = NULL;
    int generateKey = 0;
    int rc = 0;
    char *linePtr = NULL;
    size_t n_linePtr = 0;
    FILE *fp = NULL;

    getpwuid_r(getuid(), &pwd, buffer, 4096, &ptr);
    if (ptr == NULL) {
        _log(LOG_ERROR, "FAIL cannot lookup current_user");
        return 1;
    }
    snprintf(filename, 1024, "%s/.udiRoot/id_rsa.key", pwd.pw_dir);

    memset(&st_data, 0, sizeof(struct stat));
    if (stat(filename, &st_data) != 0) {
        generateKey = 1;
    }
    snprintf(filename, 1024, "%s/.udiRoot/id_rsa.key.pub", pwd.pw_dir);
    memset(&st_data, 0, sizeof(struct stat));
    if (stat(filename, &st_data) != 0) {
        generateKey = 1;
    }

    if (generateKey) {
        char cmd[1024];
        snprintf(filename, 1024, "%s/.udiRoot", pwd.pw_dir);
        mkdir(filename, 0700); // intentionally ignoring errors for this
        snprintf(filename, 1024, "%s/.udiRoot/id_rsa.key", pwd.pw_dir);
        snprintf(cmd, 1024, "ssh-keygen -t rsa -f %s -N '' >/dev/null 2>/dev/null", filename);
        rc = system(cmd);
    }
    if (rc == 0) {
        snprintf(filename, 1024, "%s/.udiRoot/id_rsa.key.pub", pwd.pw_dir);
        fp = fopen(filename, "r");
        if (fp == NULL) {
            _log(LOG_ERROR, "FAILED to open udiRoot pubkey: %s", filename);
            rc = 1;
            goto generateSshKey_exit;
        }
        if (!feof(fp) && !ferror(fp)) {
            size_t nread = getline(&linePtr, &n_linePtr, fp);
            if (nread > 0 && linePtr != NULL) {
                wrap_spank_job_control_setenv(ssconfig, "SHIFTER_SSH_PUBKEY", linePtr, 1);
                free(linePtr);
                linePtr = NULL;
            }
        }
        fclose(fp);
        fp = NULL;
    }
generateSshKey_exit:
    if (linePtr != NULL) {
        free(linePtr);
    }
    if (fp != NULL) {
        fclose(fp);
    }
    return rc;
}

int doExternStepTaskSetup(shifterSpank_config *ssconfig) {
    int rc = SUCCESS;
    struct stat statData;
    char buffer[PATH_MAX];

    if (ssconfig == NULL || ssconfig->udiConfig == NULL) return ERROR;

    /* check and see if there is an existing configuration */
    memset(&statData, 0, sizeof(struct stat));
    snprintf(buffer, 1024, "%s/var/shifterConfig.json", ssconfig->udiConfig->udiMountPoint);
    if (stat(buffer, &statData) != 0) {
        _log(LOG_ERROR, "Couldn't find shifterConfig.json, cannot do extern step processing.");
        return ERROR;
    }

    int stepd_fd = 0;
    char *dir = NULL;
    char *hostname = NULL;
    uint32_t jobid = 0;
    uid_t uid = 0;
    uint16_t protocol = 0;
    if (wrap_spank_get_jobid(ssconfig, &jobid) == ERROR) {
        _log(LOG_ERROR, "Couldn't get job id");
        return ERROR;
    }
    if (wrap_spank_get_uid(ssconfig, &uid) == ERROR) {
        _log(LOG_ERROR, "Couldn't get uid");
        return ERROR;
    }

    _log(LOG_INFO, "shifterSpank: about to do extern step setup: %d", ssconfig->extern_cgroup);
   
    if (ssconfig->extern_cgroup) {
        char *memory_cgroup_path = NULL;
        if (ssconfig->memory_cgroup) {
             memory_cgroup_path = setup_memory_cgroup(ssconfig, jobid, uid, NULL, NULL);
        }
        if (memory_cgroup_path) {
            char buffer[PATH_MAX];
            char *tasks = alloc_strgenf("%s/tasks", memory_cgroup_path);
            FILE *fp = fopen(tasks, "r");
            stepd_fd = wrap_spank_stepd_connect(ssconfig, dir, hostname, jobid, SLURM_EXTERN_CONT, &protocol);
            while (fgets(buffer, PATH_MAX, fp) != NULL) {
                pid_t pid = (pid_t) strtol(buffer, NULL, 10);
                if (pid == 0) continue;
                _log(LOG_INFO, "shifterSpank: moving pid %d to extern step via fd %d\n", pid, stepd_fd);
                wrap_spank_stepd_add_extern_pid(ssconfig, stepd_fd, protocol, pid);
            }
            free(tasks);
            fclose(fp);
        } else {
            /* move sshd into slurm proctrack */
            int sshd_pid = findSshd();
            if (sshd_pid > 0) {
                stepd_fd = wrap_spank_stepd_connect(ssconfig, dir, hostname, jobid, SLURM_EXTERN_CONT, &protocol);
                int ret = wrap_spank_stepd_add_extern_pid(ssconfig, stepd_fd, protocol, sshd_pid);
                _log(LOG_INFO, "shifterSpank: moved sshd (pid %d) into slurm controlled extern_step (ret: %d) via fd %d\n", sshd_pid, ret, stepd_fd);
            }
        }
    }

        /* see if an extern step is defined, if so, run it */
    if (ssconfig->extern_setup != NULL) {
        char *externScript[2];
        externScript[0] = ssconfig->extern_setup;
        externScript[1] = NULL;
        int status = forkAndExecvLogToSlurm("extern_setup", externScript);
        if (status == 0) rc = SUCCESS;
        else rc = ERROR;
    }
    _log(LOG_INFO, "shifterSpank: done with extern step setup");
    snprintf(buffer, PATH_MAX, "%s/var/shifterExtern.complete", ssconfig->udiConfig->udiMountPoint);
    int fd = open(buffer, O_CREAT|O_WRONLY|O_TRUNC, 0644);
    close(fd);
    return rc;
}

int shifterSpank_task_post_fork(void *id, int argc, char **argv) {
    uint32_t stepid = 0;
    int rc = SUCCESS;

    if (wrap_spank_get_stepid_noconfig(id, &stepid) == ERROR) {
        _log(LOG_ERROR, "FAILED to get stepid");
    }

    /* if this is the slurmstepd for prologflags=contain, then do the
     * proper setup to finalize shifter setup */
    if (stepid == SLURM_EXTERN_CONT) {
        shifterSpank_config *ssconfig = shifterSpank_init(id, argc, argv, 1);
        if (ssconfig->extern_cgroup || ssconfig->extern_setup) {
            rc = doExternStepTaskSetup(ssconfig);
        }
        shifterSpank_config_free(ssconfig);
        ssconfig = NULL;
    }

    return rc;
}

void shifterSpank_validate_input(shifterSpank_config *ssconfig, int allocator) {
    if (ssconfig == NULL) return;
    if (ssconfig->ccmMode == 1 &&
        ((ssconfig->imageType != NULL && strcmp(ssconfig->imageType, "local") != 0) ||
         (ssconfig->image != NULL && strcmp(ssconfig->image, "/") != 0))
       )
    {
        if (allocator) {
            _log(LOG_ERROR, "Cannot specify --ccm mode with --image, or in an allocation with a previously set image");
            exit(1);
        }
        ssconfig->ccmMode = 0;
    }
    if (ssconfig->volume != NULL && strlen(ssconfig->volume) > 0) {
        if (ssconfig->image == NULL || strlen(ssconfig->image) == 0) {
            if (allocator) {
                _log(LOG_ERROR, "Cannot specify shifter volumes without specifying the image first!");
                exit(-1);
            }
            free(ssconfig->volume);
            ssconfig->volume = NULL;
        }
    }
    if (ssconfig->image != NULL && strlen(ssconfig->image) == 0) {
        free(ssconfig->image);
        ssconfig->image = NULL;
    }
    ssconfig->args_parsed = 1;
}

void shifterSpank_init_allocator_setup(shifterSpank_config *ssconfig) {
    if (ssconfig == NULL ||
        ssconfig->imageType == NULL ||
        ssconfig->image == NULL)
    {
        return;
    }
    if (strcmp(ssconfig->imageType, "id") != 0 &&
        strcmp(ssconfig->imageType, "local") != 0)
    {
        char *image_id = NULL;
        image_id = lookup_ImageIdentifier(
            ssconfig->imageType, ssconfig->image, 0, ssconfig->udiConfig);
        if (image_id == NULL) {
            _log(LOG_ERROR, "Failed to lookup image.  Aborting.");
            exit(-1);
        }
        free(ssconfig->image);
        free(ssconfig->imageType);
        ssconfig->image = image_id;
        ssconfig->imageType = strdup("id");
    }
    if (ssconfig->image == NULL || strlen(ssconfig->image) == 0) {
        return;
    }

    /* for slurm native, generate ssh keys here */
    if (ssconfig->sshdEnabled) {
        generateSshKey(ssconfig);
    }
    wrap_spank_setenv(ssconfig, "SHIFTER_IMAGE", ssconfig->image, 1);
    wrap_spank_setenv(ssconfig, "SHIFTER_IMAGETYPE", ssconfig->imageType, 1);
    wrap_spank_job_control_setenv(ssconfig, "SHIFTER_IMAGE", ssconfig->image, 1);
    wrap_spank_job_control_setenv(ssconfig, "SHIFTER_IMAGETYPE", ssconfig->imageType, 1);

    /* change the cached value of the user supplied arg to match
     * the looked-up value */
    char *tmpval = alloc_strgenf("%s:%s", ssconfig->imageType, ssconfig->image);
    wrap_spank_setenv(ssconfig, "_SLURM_SPANK_OPTION_shifter_image", tmpval, 1);
    free(tmpval);
}


void shifterSpank_init_setup(shifterSpank_config *ssconfig) {
    if (ssconfig->image != NULL && strlen(ssconfig->image) == 0) {
        return;
    }

    if (ssconfig->volume != NULL && strlen(ssconfig->volume) > 0) {
        wrap_spank_setenv(ssconfig, "SHIFTER_VOLUME", ssconfig->volume, 1);
        wrap_spank_job_control_setenv(ssconfig, "SHIFTER_VOLUME", ssconfig->volume, 1);
    }
    if (getgid() != 0) {
        char buffer[128];
        snprintf(buffer, 128, "%d", getgid());
        wrap_spank_setenv(ssconfig, "SHIFTER_GID", buffer, 1);
        wrap_spank_job_control_setenv(ssconfig, "SHIFTER_GID", buffer, 1);
    }
    if (ssconfig->ccmMode != 0) {
        wrap_spank_setenv(ssconfig, "SHIFTER_CCM", "1", 1);
        wrap_spank_job_control_setenv(ssconfig, "SHIFTER_CCM", "1", 1);

        /* this is an irritating hack, but CCM needs to be propagated to all
         * sruns within the job allocation (even sruns within sruns) and this
         * achieves that */
        wrap_spank_setenv(ssconfig, "_SLURM_SPANK_OPTION_shifter_ccm", "", 1);
    }
    return;
}

/**
  read_data_from_job -- constructs nodelist with tasksPerNode encoded for use
      with setupRoot, as well as collect other data required
Params:
    spank_t sp: spank session identifier/structure
    char **nodelist:  pointer to nodelist string (for modification here)
    size_t *tasksPerNode: pointer to integer tasksPerNode (for modification here)
**/
int read_data_from_job(shifterSpank_config *ssconfig, uint32_t *jobid, char **nodelist, size_t *tasksPerNode, uint16_t *shared) {
    char *raw_host_string = NULL;
    size_t n_nodes = 0;

    /* load job data and fail if not possible */
    if (wrap_spank_get_jobid(ssconfig, jobid) == ERROR) {
        _log(LOG_ERROR, "Couldn't get job id");
        return ERROR;
    }

    if(wrap_spank_extra_job_attributes(ssconfig, *jobid, &raw_host_string, &n_nodes, tasksPerNode, shared) == ERROR) {
        _log(LOG_ERROR, "Failed to get job attributes");
    }

    /* convert exploded string to encode how many tasks per host */
    /* nid001/24,nid002/24,nid003/24,nid004/24 */
    if (raw_host_string != NULL) {
        char tmp[1024];
        size_t string_overhead_per_node = 2; // slash and comma
        size_t total_string_len = 0;
        size_t i = 0;
        char *r_ptr = NULL;
        char *w_ptr = NULL;
        char *e_ptr = NULL;
        char *limit_ptr = NULL;

        snprintf(tmp, 1024, "%lu", *tasksPerNode);
        string_overhead_per_node += strlen(tmp);

        // raw_host_string len - existing commas + string_overhead
        total_string_len = strlen(raw_host_string)
            - n_nodes  // ignore current commas
            + string_overhead_per_node * n_nodes // new overhead (including commas)
            + 1; // null byte
        *nodelist = malloc(sizeof(char) * total_string_len);

        r_ptr = raw_host_string;
        w_ptr = *nodelist;
        limit_ptr = *nodelist + total_string_len;
        for (i = 0; i < n_nodes; i++) {
            e_ptr = strchr(r_ptr, ',');
            if (e_ptr == NULL) e_ptr = r_ptr + strlen(r_ptr);
            *e_ptr = 0;
            w_ptr += snprintf(w_ptr, limit_ptr - w_ptr,
                    "%s/%s%c", r_ptr, tmp, (i + 1 == n_nodes ? '\0' : ' '));
            r_ptr = e_ptr + 1;
        }
        free(raw_host_string);
    }
    return SUCCESS;
}

int create_cgroup_dir(shifterSpank_config *ssconfig, const char *path, void *data) {
   int ret = mkdir(path, 0755);
   if (ret != 0 && errno == EEXIST) return SUCCESS;
   if (ret == 0) return SUCCESS;
   return ERROR;
}

int cgroup_record_components(shifterSpank_config *ssconfig, const char *path, void *data) {
    char ***comp_ptr = (char ***) data;
    char **ptr = *comp_ptr;
    size_t sz = 0;
    size_t diff = 0;
    while (ptr && *ptr) ptr++;
    if (ptr != NULL) {
        diff = ptr - *comp_ptr;
        sz = diff;
    }
    sz += 2;
#if 0
    _log(LOG_DEBUG, "sz: %lu, diff: %lu, *comp_ptr=%lu, %s", sz, diff, *comp_ptr, path);
#endif 
    *comp_ptr = (char **) realloc(*comp_ptr, sizeof(char*) * sz);
    ptr = *comp_ptr + diff;
    *ptr = strdup(path);
    ptr++;
    *ptr = NULL;
    return 0;
}

char *setup_memory_cgroup(
    shifterSpank_config *ssconfig,
    uint32_t job,
    uid_t uid,
    int (*action)(shifterSpank_config *, const char *, void *),
    void *data)
{
    struct stat st;

    if (ssconfig == NULL || ssconfig->memory_cgroup == NULL) {
        return NULL;
    }

    if (stat(ssconfig->memory_cgroup, &st) != 0) {
        /* base cgroup does not exist */
        return NULL;
    }

    char *components[] = {
        strdup("shifter"),
        alloc_strgenf("uid_%d", uid),
        alloc_strgenf("job_%u", job),
        NULL
    };
    char *cgroup_path = NULL;
    size_t cgroup_path_sz = 0;
    size_t cgroup_path_cap = 0;
    char **cptr = NULL;

    cgroup_path = alloc_strcatf(cgroup_path, &cgroup_path_sz, &cgroup_path_cap, "%s", ssconfig->memory_cgroup);

    for (cptr = components; cptr && *cptr; cptr++) {
        cgroup_path = alloc_strcatf(cgroup_path, &cgroup_path_sz, &cgroup_path_cap, "/%s", *cptr);
        if (action != NULL) {
            action(ssconfig, cgroup_path, data);
        }
        free(*cptr);
    }
    return cgroup_path;
}

int shifterSpank_job_prolog(shifterSpank_config *ssconfig) {
    int rc = SUCCESS;

    char *ptr = NULL;
    int idx = 0;
    uint32_t job;
    uid_t uid = 0;
    gid_t gid = 0;
    uint16_t shared = 0;

    char buffer[PATH_MAX];
    char setupRootPath[PATH_MAX];
    char **setupRootArgs = NULL;
    char **setupRootArgs_sv = NULL;
    size_t n_setupRootArgs = 0;
    char **volArgs = NULL;
    size_t n_volArgs = 0;
    char *nodelist = NULL;
    char *username = NULL;
    char *uid_str = NULL;
    char *gid_str = NULL;
    char *sshPubKey = NULL;
    size_t tasksPerNode = 0;
    pid_t pid = 0;

#define PROLOG_ERROR(message, errCode) \
    _log(LOG_ERROR, "%s", message); \
    rc = errCode; \
    goto _prolog_exit_unclean;

    _log(LOG_DEBUG, "shifter prolog, id after looking at args: %s:%s", ssconfig->imageType, ssconfig->image);

    /* if processing the user-specified options indicates no image, dump out */
    if (ssconfig->image == NULL || strlen(ssconfig->image) == 0 ||
        ssconfig->imageType == NULL || strlen(ssconfig->imageType) == 0) {
        return rc;
    }

#if 0
    extern char **environ;
    char **envPtr = NULL;
    for (envPtr = environ; envPtr && *envPtr; envPtr++) {
        slurm_error("env: %s\n", *envPtr);
    }
#endif

    int set_type = 0;
    ptr = getenv("SHIFTER_IMAGETYPE");
    if (ptr != NULL) {
        char *tmp = imageDesc_filterString(ptr, NULL);
        if (ssconfig->imageType != NULL) {
            free(ssconfig->imageType);
        }
        ssconfig->imageType = tmp;
        set_type = 1;
    }

    _log(LOG_ERROR, "about to lookup image in prolog env");
    ptr = getenv("SHIFTER_IMAGE");
    if (ptr != NULL) {
        char *tmp = imageDesc_filterString(ptr, set_type ? ssconfig->imageType : NULL);
        if (ssconfig->image != NULL) {
            free(ssconfig->image);
        }
        ssconfig->image = tmp;
    }

    ptr = getenv("SHIFTER_VOLUME");
    if (ptr != NULL) {
        if (ssconfig->volume != NULL) {
            free(ssconfig->volume);
        }
        ssconfig->volume = strdup(ptr);
    }
    _log(LOG_DEBUG, "shifter prolog, id after looking at env: %s:%s", ssconfig->imageType, ssconfig->image);

    /* check and see if there is an existing configuration */
    struct stat statData;
    memset(&statData, 0, sizeof(struct stat));
    snprintf(buffer, PATH_MAX, "%s/var/shifterConfig.json", ssconfig->udiConfig->udiMountPoint);
    if (stat(buffer, &statData) == 0) {
        /* oops, already something there -- do not run setupRoot
         * this is probably going to be an issue for the job, however the 
         * shifter executable can be relied upon to detect the mismatch and
         * deal with it appropriately */
        PROLOG_ERROR("shifterConfig.json already exists!", rc);
    }

    for (ptr = ssconfig->imageType; *ptr != 0; ptr++) {
        *ptr = tolower(*ptr);
    }

    rc = read_data_from_job(ssconfig, &job, &nodelist, &tasksPerNode, &shared);
    if (rc != SUCCESS) {
        PROLOG_ERROR("FAILED to get job information.", ERROR);
    }

    /* this prolog should not be used for shared-node jobs */
    if (shared != 0) {
        _log(LOG_DEBUG, "shifter prolog: job is shared, moving on");
        goto _prolog_exit_unclean;
    }

    /* try to recover ssh public key */
    sshPubKey = getenv("SHIFTER_SSH_PUBKEY");
    if (sshPubKey != NULL && ssconfig->sshdEnabled) {
        char *ptr = strdup(sshPubKey);
        sshPubKey = shifter_trim(ptr);
        sshPubKey = strdup(sshPubKey);
        free(ptr);
    }

    uid_str = getenv("SLURM_JOB_UID");
    if (uid_str != NULL) {
        uid = strtoul(uid_str, NULL, 10);
    } else {
        if (wrap_spank_get_uid(ssconfig, &uid) == ERROR) {
            PROLOG_ERROR("FAILED to get job uid!", ERROR);
        }
    }

    gid_str = getenv("SHIFTER_GID");
    if (gid_str != NULL) {
        gid = strtoul(gid_str, NULL, 10);
    } else {
        if (wrap_spank_get_gid(ssconfig, &gid) == ERROR) {
            _log(LOG_DEBUG, "shifter prolog: failed to get gid from environment, trying getpwuid_r on %d", uid);
            char buffer[4096];
            struct passwd pw, *result;
            while (1) {
                rc = getpwuid_r(uid, &pw, buffer, 4096, &result);
                if (rc == EINTR) continue;
                if (rc != 0) result = NULL;
                break;
            }
            if (result != NULL) {
                gid = result->pw_gid;
                _log(LOG_DEBUG, "shifter prolog: got gid from getpwuid_r: %s", username);
            } else {
                PROLOG_ERROR("FAILED to get job gid!", ERROR);
            }
        }
    }

    /* try to get username from environment first, then fallback to getpwuid */
    username = getenv("SLURM_JOB_USER");
    if (username != NULL && strcmp(username, "(null)") != 0) {
        username = strdup(username);
        _log(LOG_DEBUG, "shifter prolog: got username from environment: %s", username);
    } else if (uid != 0) {
        /* getpwuid may not be optimal on cray compute node, but oh well */
        _log(LOG_DEBUG, "shifter prolog: failed to get username from environment, trying getpwuid_r on %d", uid);
        char buffer[4096];
        struct passwd pw, *result;
        while (1) {
            rc = getpwuid_r(uid, &pw, buffer, 4096, &result);
            if (rc == EINTR) continue;
            if (rc != 0) result = NULL;
            break;
        }
        if (result != NULL) {
            username = strdup(result->pw_name);
            _log(LOG_DEBUG, "shifter prolog: got username from getpwuid_r: %s", username);
        }
    }

    /* setupRoot argument construction 
       /path/to/setupRoot <imageType> <imageIdentifier> -u <uid> -U <username>
            [-v volMap ...] -s <sshPubKey> -N <nodespec> NULL
     */
    if (ssconfig->volume != NULL && strlen(ssconfig->volume) > 0) {
        char *ptr = ssconfig->volume;
        for ( ; ; ) {
            char *limit = strchr(ptr, ';');
            volArgs = (char **) realloc(volArgs,sizeof(char *) * (n_volArgs + 2));
            if (limit != NULL) *limit = 0;
            volArgs[n_volArgs++] = strdup(ptr);
            volArgs[n_volArgs] = NULL;


            if (limit == NULL) {
                break;
            }
            ptr = limit + 1;
        }
    }
    snprintf(setupRootPath, PATH_MAX, "%s/sbin/setupRoot", ssconfig->udiConfig->udiRootPath);
    strncpy_StringArray(setupRootPath, strlen(setupRootPath), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
    if (uid != 0) {
        snprintf(buffer, PATH_MAX, "%u", uid);
        strncpy_StringArray("-U", 3, &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
        strncpy_StringArray(buffer, strlen(buffer), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
    }
    if (gid != 0) {
        snprintf(buffer, PATH_MAX, "%u", gid);
        strncpy_StringArray("-G", 3, &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
        strncpy_StringArray(buffer, strlen(buffer), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
    }
    if (username != NULL) {
        strncpy_StringArray("-u", 3, &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
        strncpy_StringArray(username, strlen(username), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
    }
    if (sshPubKey != NULL) {
        strncpy_StringArray("-s", 3, &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
        strncpy_StringArray(sshPubKey, strlen(sshPubKey), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
    }
    if (nodelist != NULL) {
        strncpy_StringArray("-N", 3, &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
        strncpy_StringArray(nodelist, strlen(nodelist), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
    }
    for (idx = 0; idx < n_volArgs; idx++) {
        strncpy_StringArray("-v", 3, &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
        strncpy_StringArray(volArgs[idx], strlen(volArgs[idx]), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
    }
    strncpy_StringArray(ssconfig->imageType, strlen(ssconfig->imageType), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
    strncpy_StringArray(ssconfig->image, strlen(ssconfig->image), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);

    for (setupRootArgs_sv = setupRootArgs; setupRootArgs_sv && *setupRootArgs_sv; setupRootArgs_sv++) {
        _log(LOG_ERROR, "setupRoot arg %d: %s", (int)(setupRootArgs_sv - setupRootArgs), *setupRootArgs_sv);
    }

    /* setup memory cgroup if necessary */
    char *memory_cgroup_path = NULL;
    if (ssconfig->memory_cgroup) {
        memory_cgroup_path = setup_memory_cgroup(ssconfig, job, uid, create_cgroup_dir, NULL);
    }

    /* if the cgroup exists, put this process onto it (which will result in
       setupRoot and any sshd process it execs being in the cgroup */
    if (memory_cgroup_path != NULL) {
        char *tasks = alloc_strgenf("%s/tasks", memory_cgroup_path);
        FILE *fp = fopen(tasks, "w");
        if (fp != NULL) {
            fprintf(fp, "%d\n", getpid());
            fclose(fp);
        }
        free(tasks);
        free(memory_cgroup_path);
    }

    int status = forkAndExecvLogToSlurm("setupRoot", setupRootArgs);

    _log(LOG_ERROR, "after setupRoot, exit code: %d", status);


    if (status == 0) {
        snprintf(buffer, PATH_MAX, "%s/var/shifterSlurm.jobid", ssconfig->udiConfig->udiMountPoint);
        FILE *fp = fopen(buffer, "w");
        if (fp == NULL) {
            _log(LOG_ERROR, "shifter_prolog: failed to open file %s\n", buffer);
        } else {
            fprintf(fp, "%d", job);
            fclose(fp);
        }
    }

    pid = findSshd();
    _log(LOG_DEBUG, "shifter_prolog: sshd on pid %d\n", pid);
    
_prolog_exit_unclean:
    if (setupRootArgs != NULL) {
        char **ptr = NULL;
        for (ptr = setupRootArgs; ptr && *ptr; ptr++) {
            free(*ptr);
        }
        free(setupRootArgs);
    }
    if (volArgs != NULL) {
        char **ptr = volArgs;
        while (*ptr != NULL) free(*ptr++);
        free(volArgs);
    }
    if (nodelist != NULL) free(nodelist);
    if (username != NULL) free(username);
    if (sshPubKey != NULL) free(sshPubKey);

    return rc;
} 

int shifterSpank_job_epilog(shifterSpank_config *ssconfig) {
    int rc = SUCCESS;
    char path[PATH_MAX];
    char *epilogueArgs[2];
    uid_t uid = 0;
    uint32_t job = 0;

#define EPILOG_ERROR(message, errCode) \
    _log(LOG_ERROR, "%s", message); \
    rc = errCode; \
    goto _epilog_exit_unclean;

    if (ssconfig->image == NULL || strlen(ssconfig->image) == 0) {
        return rc;
    }

    if (wrap_spank_get_uid(ssconfig, &uid) == ERROR) {
        EPILOG_ERROR("FAILED to get job uid!", ERROR);
    }
    if (wrap_spank_get_jobid(ssconfig, &job) == ERROR) {
        EPILOG_ERROR("Couldnt get job id", ERROR);
    }

    /* see if memory cgroup exists */
    char *memory_cgroup_path = NULL;
    char **cgroup_components = NULL;
    if (ssconfig->memory_cgroup) {
        memory_cgroup_path = setup_memory_cgroup(ssconfig, job, uid, cgroup_record_components, (void *) &cgroup_components);
        if (memory_cgroup_path != NULL && cgroup_components != NULL) {
            char **ptr = NULL;
            char *line = NULL;
            size_t line_sz = 0;
            ssize_t bytes = 0;
            char *tasks = alloc_strgenf("%s/tasks", memory_cgroup_path);
            FILE *fp = NULL;

            /* kill all the processes in the cgroup tasks */
            fp = fopen(tasks, "r");
            if (fp != NULL) {
                while ((bytes = getline(&line, &line_sz, fp)) >= 0) {
                    pid_t pid = (pid_t) strtol(line, NULL, 10);
                    if (pid == 0) continue;
                    kill(pid, 9); 
                }
                fclose(fp);
                fp = NULL;
            }
            free(tasks);
            tasks = NULL;

            /* remove the empty cgroups */
            for (ptr = cgroup_components; ptr && *ptr; ptr++) { }
            if (ptr > cgroup_components) {
                for ( ; ptr && *ptr && (ptr > cgroup_components); ptr--) {
                    rmdir(*ptr);
                    free(*ptr);
                    *ptr = NULL;
                }
            }
            free(cgroup_components);
            cgroup_components = NULL;
        }
        if (memory_cgroup_path != NULL) {
            free(memory_cgroup_path);
            memory_cgroup_path = NULL;
        }
        if (cgroup_components != NULL) {
            char **ptr = NULL;
            for (ptr = cgroup_components; ptr && *ptr; ptr++) {
                free(*ptr);
                *ptr = NULL;
            }
            free(cgroup_components);
        }
    }

    snprintf(path, PATH_MAX, "%s/sbin/unsetupRoot", ssconfig->udiConfig->udiRootPath);
    epilogueArgs[0] = path;
    epilogueArgs[1] = NULL;
    int status = forkAndExecvLogToSlurm("unsetupRoot", epilogueArgs);
    if (status != 0) {
        rc = SLURM_ERROR;
    }

    _log(LOG_DEBUG, "shifter_epilog: done with unsetupRoot");

_epilog_exit_unclean:
    return rc;
}

int shifterSpank_task_init_privileged(shifterSpank_config *ssconfig) {
    int rc = SUCCESS;
    ImageData imageData;

    uid_t job_uid = 0;
    uid_t curr_uid = geteuid();

    gid_t *existing_suppl_gids = NULL;
    int n_existing_suppl_gids = 0;
    gid_t existing_gid = getegid();
    uint32_t stepid = 0;

    memset(&imageData, 0, sizeof(ImageData));
    if (wrap_spank_get_stepid(ssconfig, &stepid) == ERROR) {
        _log(LOG_ERROR, "FAILED to get stepid");
    }

#define TASKINITPRIV_ERROR(message, errCode) \
    _log(LOG_ERROR, "%s", message); \
    rc = errCode; \
    goto _taskInitPriv_exit_unclean;

    if (ssconfig->udiConfig == NULL) {
        TASKINITPRIV_ERROR("Failed to load udiRoot config!", ERROR);
    }

    if (ssconfig->image == NULL || strlen(ssconfig->image) == 0) {
        return rc;
    }
    if (ssconfig->imageType == NULL || strlen(ssconfig->imageType) == 0) {
        return rc;
    }

    /* if this is the slurmstepd for prologflags=contain, then do the
     * proper setup to finalize shifter setup */
    if (stepid == SLURM_EXTERN_CONT) {
        return rc;
    } else if (ssconfig->extern_setup != NULL || ssconfig->extern_cgroup) {
        /* need to ensure the extern step is setup */
        char buffer[PATH_MAX];
        struct stat statData;
        int tries = 0;
        snprintf(buffer, PATH_MAX, "%s/var/shifterExtern.complete", ssconfig->udiConfig->udiMountPoint);
        for (tries = 0; tries < 10; tries++) {
            if (stat(buffer, &statData) == 0) {
                break;
            } else {
                sleep(1);
            }
        }
    }

    if (ssconfig->ccmMode == 0) return rc;

    parse_ImageData(ssconfig->imageType, ssconfig->image, ssconfig->udiConfig, &imageData);

    if (wrap_spank_get_uid(ssconfig, &job_uid) == ERROR) {
        TASKINITPRIV_ERROR("FAILED to get job uid!", ERROR);
    }

    if (strlen(ssconfig->udiConfig->udiMountPoint) > 0) {
        char currcwd[PATH_MAX];
        char newcwd[PATH_MAX];
        gid_t *gids = NULL;
        gid_t gid = 0;
        int ngids = 0;
        struct stat st_data;

        if (getcwd(currcwd, PATH_MAX) == NULL) {
            TASKINITPRIV_ERROR("FAILED to determine working directory", ERROR);
        }

        // check to see if newcwd exists, if not, just chdir to the new chroot base
        snprintf(newcwd, PATH_MAX, "%s", ssconfig->udiConfig->udiMountPoint);
        if (stat(newcwd, &st_data) != 0) {
            char error[PATH_MAX];
            snprintf(error, PATH_MAX, "FAILED to stat UDI directory: %s", newcwd);
            TASKINITPRIV_ERROR(error, ERROR);
        }
        if (chdir(newcwd) != 0) {
            char error[PATH_MAX];
            snprintf(error, PATH_MAX, "FAILED to change directory to %s", newcwd);
            TASKINITPRIV_ERROR(error, ERROR);
        }

        if (chroot(ssconfig->udiConfig->udiMountPoint) != 0) {
            TASKINITPRIV_ERROR("FAILED to chroot to designated image", ERROR);
        }
        if (chdir("/") != 0) {
            TASKINITPRIV_ERROR("FAILED to chdir to new chroot", ERROR);
        }

        if (wrap_spank_get_supplementary_gids(ssconfig, &gids, &ngids) == ERROR) {
            TASKINITPRIV_ERROR("FAILED to obtain group ids", ERROR);
        }

        if (wrap_spank_get_gid(ssconfig, &gid) == ERROR) {
            TASKINITPRIV_ERROR("FAILED to obtain job group id", ERROR);
        }

        n_existing_suppl_gids = getgroups(0, NULL);
        if (n_existing_suppl_gids > 0) {
            existing_suppl_gids = (gid_t *) malloc(sizeof(gid_t) * n_existing_suppl_gids);
            if (existing_suppl_gids == NULL) {
                TASKINITPRIV_ERROR("FAILED to allocate memory to store current suppl gids", ERROR);
            }
            if (getgroups(n_existing_suppl_gids, existing_suppl_gids) < 0) {
                TASKINITPRIV_ERROR("FAILED to get current suppl gids", ERROR);
            }
        }

        if (setgroups(ngids, gids) != 0) {
            TASKINITPRIV_ERROR("FAILED to set supplmentary group ids", ERROR);
        }

        if (setegid(gid) != 0) {
            TASKINITPRIV_ERROR("FAILED to set job group id", ERROR);
        }

        /* briefly assume job uid prior to chdir into target path
           this is necessary in the case that we are chdir'ing into a privilege
           restricted path on a root-squashed filesystem */
        if (seteuid(job_uid) < 0) {
            char error[1024];
            snprintf(error, 1024, "FAILED to set effective uid to %d", job_uid);
            TASKINITPRIV_ERROR(error, ERROR);
        }

        if (chdir(currcwd) != 0) {
            char error[PATH_MAX];
            snprintf(error, PATH_MAX, "FAILED to change directory to: %s", currcwd);
            _log(LOG_ERROR, "FAILED to change directory to %s, going to /tmp instead.", currcwd);
            if (chdir("/tmp") != 0) {
                snprintf(error, 1024, "Failed to chdir to /tmp, fatal error");
                TASKINITPRIV_ERROR(error, ERROR);
            }
        }

        /* go back to our original effective uid */
        if (seteuid(curr_uid) < 0) {
            char error[1024];
            snprintf(error, 1024, "FAILED to return effective uid to %d", curr_uid);
            TASKINITPRIV_ERROR(error, ERROR);
        }

        if (setegid(existing_gid) != 0) {
            TASKINITPRIV_ERROR("FAILED to return effective gid", ERROR);
        }
        if (setgroups(n_existing_suppl_gids, existing_suppl_gids) != 0) {
            TASKINITPRIV_ERROR("FAILED to drop supplementary gids", ERROR);
        }
        if (existing_suppl_gids != NULL) {
            free(existing_suppl_gids);
            existing_suppl_gids = NULL;
        }
    }
_taskInitPriv_exit_unclean:
    free_ImageData(&imageData, 0);
    if (existing_suppl_gids != NULL) {
        free(existing_suppl_gids);
        existing_suppl_gids = NULL;
    }
    return rc;
}
