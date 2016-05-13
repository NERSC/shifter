#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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


int shifterSpank_init(int argc, char **argv, int getoptValues) {
    int idx = 0;
    char buffer[PATH_MAX];
    if (ssConfig != NULL) {
        return ERROR;
    }
    ssConfig = (shifterSpank_config *) malloc(sizeof(shifterSpank_config));
    if (ssConfig == NULL) {
        slurm_error("FAILED to allocate memory for shifterSpank_config");
        return ERROR;
    }
    memset(ssConfig, 0, sizeof(shifterSpank_config));

    for (idx = 0; idx < argc; ++idx) {
        if (strncmp("shifter_config=", argv[idx], 15) == 0) {
            char *ptr = argv[idx] + 15;
            snprintf(buffer, "%s", ptr);
            ptr = trim(buffer);
            ssConfig->shifter_config = strdup(ptr);
        } else if (strncasecmp("extern_setup=", argv[idx], 13) == 0) {
            char *ptr = argv[idx] + 13;
            snprintf(buffer, "%s", ptr);
            ptr = trim(buffer);
            ssConfig->extern_setup = strdup(ptr);
        } else if (strncasecmp("memory_cgroup=", argv[idx], 14) == 0) {
            char *ptr = argv[idx] + 14;
            snprintf(buffer, "%s", ptr);
            ptr = trim(buffer);
            ssConfig->memory_cgroup = strdup(ptr);
        } 
    }

    if (ssConfig->shifter_config == NULL) {
        ssConfig->shifter_config = strdup(CONFIG_FILE);
    }
    if (ssConfig->shifter_config == NULL) {
        lgplSpank_log_error("shifterSlurm: failed to find config filename");
        return ERROR;
    }

    ssConfig->udiConfig = malloc(sizeof(UdiRootConfig));
    if (ssConfig->udiConfig == NULL) {
        lgplSpank_log_error("FAILED to allocate memory to read "
            "udiRoot configuration\n");
        return ERROR;
    }
    memset(udiConfig, 0, sizeof(UdiRootConfig));

    if (parse_UdiRootConfig(config_filename, udiConfig, UDIROOT_VAL_ALL) != 0) {
        lgplSpank_log_error("FAILED to read udiRoot configuration file!\n");
        free(ssConfig->udiConfig);
        ssConfig->udiConfig = NULL;
        return NULL;
    }

    return SUCCESS;
}


int _opt_ccm(int val, const char *optarg, int remote) {
    if (strlen(image) == 0 && strlen(image_type) == 0) {
        snprintf(image, IMAGE_MAXLEN, "/");
        snprintf(image_type, IMAGE_MAXLEN, "local");
    }
    ccmMode = 1;
    return SUCCESS;
}

int _opt_image(int val, const char *optarg, int remote) {
    char *tmp = NULL;
    int rc = SUCCESS;
    if (optarg != NULL && strlen(optarg) > 0) {
        char *type = NULL;
        char *tag = NULL;
        tmp = strdup(optarg);
        if (parse_ImageDescriptor(tmp, &type, &tag, udiConfig) != 0) {
            slurm_error("Invalid image input: could not determine image type: "
                    "%s", optarg);
            rc = ERROR;
            goto _opt_image_exit;
        }
        snprintf(image_type, IMAGE_MAXLEN, "%s", type);
        snprintf(image, IMAGE_MAXLEN, "%s", tag);
        free(tmp);
        free(type);
        free(tag);
        tmp = NULL;
        type = NULL;
        tag = NULL;
        return SUCCESS;
    }
    slurm_error("Invalid image - must not be zero length");
_opt_image_exit:
    if (tmp != NULL) {
        free(tmp);
    }
    return rc;
}

int _opt_imagevolume(int val, const char *optarg, int remote) {
    if (optarg != NULL && strlen(optarg) > 0) {
        /* validate input */
        VolumeMap *vmap = (VolumeMap *) malloc(sizeof(VolumeMap));
        char *ptr = imagevolume + strlen(imagevolume);
        memset(vmap, 0, sizeof(VolumeMap));

        if (parseVolumeMap(optarg, vmap) != 0) {
            slurm_error("Failed to parse or invalid/disallowed volume map request: %s\n", optarg);
            free_VolumeMap(vmap, 1);
            exit(1);
        }
        free_VolumeMap(vmap, 1);

        if (ptr != imagevolume) {
            *ptr++ = ';';
        }
        if (strlen(optarg) > IMAGEVOLUME_MAXLEN - (ptr - imagevolume)) {
            slurm_error("Failed to store volume map request, too big: %s", optarg);
            exit(1);
        }

        snprintf(ptr, IMAGEVOLUME_MAXLEN - (ptr - imagevolume), "%s", optarg);
        return SUCCESS;
    }
    slurm_error("Invalid image volume options - if specified, must not be zero length");
    return ERROR;
}

int forkAndExecvLogToSlurm(const char *appname, char **args) {
    int rc = 0;
    pid_t pid = 0;

    /* pipes for reading from setupRoot */
    int stdoutPipe[2];
    int stderrPipe[2];

    pipe(stdoutPipe);
    pipe(stderrPipe);
    pid = fork();
    if (pid < 0) {
        slurm_error("FAILED to fork %s", appname);
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
                    slurm_error("%s stdout: %s", appname, lineBuffer);
                } else {
                    fclose(stdoutStream);
                    stdoutStream = NULL;
                }
            }
            if (stderrStream) {
                ssize_t nBytes = getline(&lineBuffer, &lineBuffer_sz, stderrStream);
                if (nBytes > 0) {
                    slurm_error("%s stderr: %s", appname, lineBuffer);
                } else {
                    fclose(stderrStream);
                    stderrStream = NULL;
                }
            }
        }


        /* wait on the child */
        slurm_error("waiting on %s\n", appname);
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
             rc = WEXITSTATUS(status);
        } else {
             rc = 1;
        }
        if (status != 0) {
            slurm_error("FAILED to run %s", appname);
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
        slurm_error("FAIL cannot lookup current_user");
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
            slurm_error("FAILED to open udiRoot pubkey: %s", filename);
            rc = 1;
            goto generateSshKey_exit;
        }
        if (!feof(fp) && !ferror(fp)) {
            size_t nread = getline(&linePtr, &n_linePtr, fp);
            if (nread > 0 && linePtr != NULL) {
                lgplSpank_job_control_setenv(ssconfig->id, "SHIFTER_SSH_PUBKEY", linePtr, 1);
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

int doForceArgParse(shifterSpank_config *ssconfig) {
    int i,j;
    int rc = SUCCESS;
    for (i = 0; spank_option_array[i].name != NULL; ++i) {
        char *optarg = NULL;
        j = spank_option_getopt(sp, &spank_option_array[i], &optarg);
        if (j != SUCCESS) {
            continue;
        }
        (spank_option_array[i].cb)(spank_option_array[i].val, optarg, 1);
    }
    return rc;
}

int doExternStepTaskSetup(shifterSpank_config *ssconfig) {
    int rc = SUCCESS;
    struct stat statData;
    char buffer[PATH_MAX];
    /* check and see if there is an existing configuration */
    memset(&statData, 0, sizeof(struct stat));
    snprintf(buffer, 1024, "%s/var/shifterConfig.json", udiConfig->udiMountPoint);
    if (stat(buffer, &statData) == 0) {
        int stepd_fd = 0;
        int i = 0;
        char *dir = NULL;
        char *hostname = NULL;
        uint32_t jobid = 0;
        uint16_t protocol = 0;
        if (lgplSpank_get_jobid(ssConfig->id, &jobid) == ERROR) {
            lgplSpank_log_error(ssConfig->id, "Couldn't get job id");
            return ERROR;
        }

        /* move sshd into slurm proctrack */
        int sshd_pid = findSshd();
        if (sshd_pid > 0) {
            stepd_fd = lgplSpank_stepd_connect(dir, hostname, jobid, SLURM_EXTERN_CONT, &protocol);
            int ret = lgplSpank_stepd_add_extern_pid(stepd_fd, protocol, sshd_pid);
            lgplSpank_log_info("moved sshd (pid %d) into slurm controlled extern_step (ret: %d) via fd %d\n", sshd_pid, ret, stepd_fd);
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
        snprintf(buffer, PATH_MAX, "%s/var/shifterExtern.complete", ssconfig->udiConfig->udiMountPoint);
        int fd = open(buffer, O_CREAT|O_WRONLY|O_TRUNC, 0644);
        close(fd);
    }
    return rc;
}

int shifterSpank_task_post_fork(unsigned int id, int argc, char **argv) {
    uint32_t stepid = 0;
    int rc = SUCCESS;

    if (wrapSpank_get_stepid(id, &stepid) == ERROR) {
        wrapSpank_log_error(id, "FAILED to get stepid");
    }

    /* if this is the slurmstepd for prologflags=contain, then do the
     * proper setup to finalize shifter setup */
    if (stepid == SLURM_EXTERN_CONT) {
        if (ssconfig == NULL) ssconfig = shifterSpank_init(argc, argv, 1);
        rc = doExternStepTaskSetup(ssconfig);
        shifterSpank_free(ssconfig);
        ssconfig = NULL;
    }

    return rc;
}

void shifterSpank_validate_input(int allocator) {
    if (ssconfig == NULL) return;
    if (ssconfig->ccmMode == 1 &&
        (strcmp(ssconfig->image_type, "local") != 0 ||
         strcmp(ssconfig->image, "/") != 0)
       )
    {
        if (allocator) {
            wrap_slurm_error("Cannot specify --ccm mode with --image, or in an allocation with a previously set image");
            exit(1);
        }
        ssconfig->ccmMode = 0;
    }
    if (ssconfig->imagevolume != NULL && strlen(ssconfig->imagevolume > 0)) {
        if (ssconfig->image == NULL || strlen(ssconfig->image) == 0) {
            if (allocator) {
                wrap_slurm_error("Cannot specify shifter volumes without specifying the image first!");
                exit(-1);
            }
            free(ssconfig->imagevolume);
            ssconfig->imagevolume = NULL;
        }
    }
    if (ssconfig->image != NULL && strlen(ssconfig->image) == 0) {
        free(ssconfig->image);
        ssconfig->image = NULL;
    }
}

void shifterSpank_init_allocator_setup() {
    if (ssconfig == NULL ||
        ssconfig->image_type == NULL ||
        ssconfig->image == NULL)
    {
        return;
    }
    if (strcmp(ssconfig->image_type, "id") != 0 &&
        strcmp(ssconfig->image_type, "local") != 0)
    {
        char *image_id = NULL;
        image_id = lookup_ImageIdentifier(
            ssconfig->image_type, ssconfig->image, 0, ssconfig->udiConfig);
        if (image_id == NULL) {
            wrap_slurm_error("Failed to lookup image.  Aborting.");
            exit(-1);
        }
        free(ssconfig->image);
        free(ssconfig->image_type);
        ssconfig->image = image_id;
        ssconfig->image_type = strdup("id");
    }
    if (strlen(ssconfig->image) == 0) {
        return;
    }

    if (ssconfig->nativeSlurm) {
        /* for slurm native, generate ssh keys here */
        generateSshKey();
    }
    wrap_spank_setenv("SHIFTER_IMAGE", image, 1);
    wrap_spank_setenv("SHIFTER_IMAGETYPE", image_type, 1);
    wrap_spank_job_control_setenv("SHIFTER_IMAGE", image, 1);
    wrap_spank_job_control_setenv("SHIFTER_IMAGETYPE", image_type, 1);

    /* change the cached value of the user supplied arg to match
     * the looked-up value */
    char *tmpval = alloc_strgenf("%s:%s", image_type, image);
    wrap_spank_setenv("_SLURM_SPANK_OPTION_shifter_image", tmpval, 1);
    free(tmpval);
}

int shifterSpank_init_setup() {
    if (ssconfig == NULL) return ERROR;
    if (ssconfig->image != NULL && strlen(ssconfig->image) == 0) {
        return SUCCESS;
    }

    verbose_lookup = 1;
    
    if (ssconfig->imagevolume != NULL && strlen(ssconfig->imagevolume) > 0) {
        wrap_spank_setenv("SHIFTER_VOLUME", ssconfig->imagevolume, 1);
        wrap_spank_job_control_setenv("SHIFTER_VOLUME", ssconfig->imagevolume, 1);
    }
    if (getgid() != 0) {
        char buffer[128];
        snprintf(buffer, 128, "%d", getgid());
        wrap_spank_setenv("SHIFTER_GID", buffer, 1);
        wrap_spank_job_control_setenv("SHIFTER_GID", buffer, 1);
    }
    if (ccmMode != 0) {
        wrap_spank_setenv("SHIFTER_CCM", "1", 1);
        wrap_spank_job_control_setenv("SHIFTER_CCM", "1", 1);

        /* this is an irritating hack, but CCM needs to be propagated to all
         * sruns within the job allocation (even sruns within sruns) and this
         * achieves that */
        wrap_spank_setenv("_SLURM_SPANK_OPTION_shifter_ccm", "", 1);
    }
    return rc;
}

/**
  read_data_from_job -- constructs nodelist with tasksPerNode encoded for use
      with setupRoot, as well as collect other data required
Params:
    spank_t sp: spank session identifier/structure
    char **nodelist:  pointer to nodelist string (for modification here)
    size_t *tasksPerNode: pointer to integer tasksPerNode (for modification here)
**/
int read_data_from_job(spank_t sp, uint32_t *jobid, char **nodelist, size_t *tasksPerNode, uint16_t *shared) {
    job_info_msg_t *job_buf = NULL;
    hostlist_t hl;
    char *raw_host_string = NULL;

    /* load job data and fail if not possible */
    if (lgplSpank_get_jobid(ssConfig->id, jobid) == ERROR) {
        lgplSpank_log_error(ssConfig->id, "Couldn't get job id");
        return ERROR;
    }
    if (slurm_load_job(&job_buf, *jobid, SHOW_ALL) != 0) {
        slurm_error("Couldn't load job data");
        return ERROR;
    }
    if (job_buf->record_count != 1) {
        slurm_error("Can't deal with this job!");
        slurm_free_job_info_msg(job_buf);
        return ERROR;
    }

    /* get tasksPerNode */
    *tasksPerNode = job_buf->job_array->num_cpus / job_buf->job_array->num_nodes;

    /* get shared-node status */
    *shared = job_buf->job_array->shared;

    /* obtain the hostlist for this job */
    hl = slurm_hostlist_create_dims(job_buf->job_array->nodes, 0);
    slurm_hostlist_uniq(hl);

    /* convert hostlist to exploded string */
    /* nid00[1-4] -> nid001,nid002,nid003,nid004 */
    raw_host_string = slurm_hostlist_deranged_string_malloc(hl);

    /* convert exploded string to encode how many tasks per host */
    /* nid001/24,nid002/24,nid003/24,nid004/24 */
    if (raw_host_string != NULL) {
        char tmp[1024];
        size_t string_overhead_per_node = 2; // slash and comma
        size_t total_string_len = 0;
        size_t n_nodes = job_buf->job_array->num_nodes;
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
    slurm_free_job_info_msg(job_buf);
    slurm_hostlist_destroy(hl);
    return SUCCESS;
}

int shifterSpank_job_prolog() {
    int rc = SUCCESS;

    char *ptr = NULL;
    int idx = 0;
    int i,j;
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
    char **volArgs_sv = NULL;
    size_t n_volArgs = 0;
    char *nodelist = NULL;
    char *username = NULL;
    char *uid_str = NULL;
    char *gid_str = NULL;
    char *sshPubKey = NULL;
    size_t tasksPerNode = 0;
    pid_t pid = 0;

#define PROLOG_ERROR(message, errCode) \
    slurm_error(message); \
    rc = errCode; \
    goto _prolog_exit_unclean;

    wrap_slurm_debug("shifter prolog, id after looking at args: %s:%s", ssconfig->image_type, ssconfig->image);

    /* if processing the user-specified options indicates no image, dump out */
    if (strlen(ssconfig->image) == 0 || strlen(ssconfig->image_type) == 0) {
        return rc;
    }

    int set_type = 0;
    ptr = getenv("SHIFTER_IMAGETYPE");
    if (ptr != NULL) {
        char *tmp = imageDesc_filterString(ptr, NULL);
        snprintf(image_type, IMAGE_MAXLEN, "%s", tmp);
        free(tmp);
        set_type = 1;
    }

    ptr = getenv("SHIFTER_IMAGE");
    if (ptr != NULL) {
        char *tmp = imageDesc_filterString(ptr, set_type ? image_type : NULL);
        snprintf(image, IMAGE_MAXLEN, "%s", tmp);
        free(tmp);
    }

    ptr = getenv("SHIFTER_VOLUME");
    if (ptr != NULL) {
        snprintf(imagevolume, IMAGEVOLUME_MAXLEN, "%s", ptr);
    }
    wrap_slurm_debug("shifter prolog, id after looking at env: %s:%s", image_type, image);

    /* check and see if there is an existing configuration */
    struct stat statData;
    memset(&statData, 0, sizeof(struct stat));
    snprintf(buffer, PATH_MAX, "%s/var/shifterConfig.json", udiConfig->udiMountPoint);
    if (stat(buffer, &statData) == 0) {
        /* oops, already something there -- do not run setupRoot
         * this is probably going to be an issue for the job, however the 
         * shifter executable can be relied upon to detect the mismatch and
         * deal with it appropriately */
        PROLOG_ERROR("shifterConfig.json already exists!", rc);
    }

    for (ptr = image_type; ptr - image_type < strlen(image_type); ptr++) {
        *ptr = tolower(*ptr);
    }

    rc = read_data_from_job(sp, &job, &nodelist, &tasksPerNode, &shared);
    if (rc != SUCCESS) {
        PROLOG_ERROR("FAILED to get job information.", ERROR);
    }

    /* this prolog should not be used for shared-node jobs */
    if (shared != 0) {
        slurm_debug("shifter prolog: job is shared, moving on");
        goto _prolog_exit_unclean;
    }

    /* try to recover ssh public key */
    sshPubKey = getenv("SHIFTER_SSH_PUBKEY");
    if (sshPubKey != NULL) {
        char *ptr = strdup(sshPubKey);
        sshPubKey = trim(ptr);
        sshPubKey = strdup(sshPubKey);
        free(ptr);
    }

    uid_str = getenv("SLURM_JOB_UID");
    if (uid_str != NULL) {
        uid = strtoul(uid_str, NULL, 10);
    } else {
        if (lgplSpank_get_uid(ssConfig->id, &uid) == ERROR) {
            PROLOG_ERROR("FAILED to get job uid!", ERROR);
        }
    }

    gid_str = getenv("SHIFTER_GID");
    if (gid_str != NULL) {
        gid = strtoul(gid_str, NULL, 10);
    } else {
        if (lgplSpank_get_gid(ssConfig->id, &gid) == ERROR) {
            PROLOG_ERROR("FAILED to get job gid!", ERROR);
        }
    }

    /* try to get username from environment first, then fallback to getpwuid */
    username = getenv("SLURM_JOB_USER");
    if (username != NULL && strcmp(username, "(null)") != 0) {
        username = strdup(username);
        slurm_debug("shifter prolog: got username from environment: %s", username);
    } else if (uid != 0) {
        /* getpwuid may not be optimal on cray compute node, but oh well */
        slurm_debug("shifter prolog: failed to get username from environment, trying getpwuid_r on %d", uid);
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
            slurm_debug("shifter prolog: got username from getpwuid_r: %s", username);
        }
    }

    /* setupRoot argument construction 
       /path/to/setupRoot <imageType> <imageIdentifier> -u <uid> -U <username>
            [-v volMap ...] -s <sshPubKey> -N <nodespec> NULL
     */
    if (strlen(imagevolume) > 0) {
        char *ptr = imagevolume;
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
    snprintf(setupRootPath, PATH_MAX, "%s/sbin/setupRoot", udiConfig->udiRootPath);
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
    strncpy_StringArray(image_type, strlen(image_type), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
    strncpy_StringArray(image, strlen(image), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);

    for (setupRootArgs_sv = setupRootArgs; setupRootArgs_sv && *setupRootArgs_sv; setupRootArgs_sv++) {
        wrap_slurm_error("setupRoot arg %d: %s", (int)(setupRootArgs_sv - setupRootArgs), *setupRootArgs_sv);
    }

    int status = forkAndExecvLogToSlurm("setupRoot", setupRootArgs);

    wrap_slurm_error("after setupRoot");

    snprintf(buffer, PATH_MAX, "%s/var/shifterSlurm.jobid", udiConfig->udiMountPoint);
    FILE *fp = fopen(buffer, "w");
    if (fp == NULL) {
        wrap_slurm_error("shifter_prolog: failed to open file %s\n", buffer);
    } else {
        fprintf(fp, "%d", job);
        fclose(fp);
    }

    pid = findSshd();
    wrap_slurm_debug("shifter_prolog: sshd on pid %d\n", pid);
    
_prolog_exit_unclean:
    if (setupRootArgs != NULL) {
        char **ptr = setupRootArgs;
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

int shifterSpank_job_epilog() {
    int rc = SUCCESS;
    spank_context_t context;
    char path[PATH_MAX];
    char *epilogueArgs[2];
    int i, j;
    pid_t pid = 0;
    char *lineBuffer = NULL;
    size_t lineBuffer_sz = 0;
    uid_t uid = 0;
    int job = 0;
    int retry = 0;

#define EPILOG_ERROR(message, errCode) \
    slurm_error(message); \
    rc = errCode; \
    goto _epilog_exit_unclean;

    if (udiConfig == NULL) {
        read_config(argc, argv);
    }
    if (udiConfig == NULL) {
        EPILOG_ERROR("Failed to read/parse shifter configuration.\n", rc);
    }

    context = spank_context();
    for (i = 0; spank_option_array[i].name != NULL; ++i) {
        char *optarg = NULL;
        j = spank_option_getopt(sp, &spank_option_array[i], &optarg);
        if (j != SUCCESS) {
            continue;
        }
        (spank_option_array[i].cb)(spank_option_array[i].val, optarg, 1);
    }
    if (strlen(image) == 0) {
        return rc;
    }

    if (lgplSpank_get_uid(ssConfig->id, &uid) == ERROR) {
        EPILOG_ERROR("FAILED to get job uid!", ERROR);
    }
    if (lgplSpank_get_jobid(ssConfig->id, &job) == ERROR) {
        EPILOG_ERROR("Couldnt get job id", ERROR);
    }

    snprintf(path, PATH_MAX, "%s/sbin/unsetupRoot", udiConfig->udiRootPath);
    epilogueArgs[0] = path;
    epilogueArgs[1] = NULL;
    int status = forkAndExecvLogToSlurm("unsetupRoot", epilogueArgs);
    if (status != 0) {
        rc = SLURM_ERROR;
    }

    slurm_debug("shifter_epilog: done with unsetupRoot");
    
_epilog_exit_unclean:
    return rc;
}

int slurm_spank_task_init_privileged(spank_t sp, int argc, char **argv) {
    int rc = SUCCESS;
    int i = 0, j = 0;
    ImageData imageData;

    uid_t job_uid = 0;
    uid_t curr_uid = geteuid();

    gid_t *existing_suppl_gids = NULL;
    int n_existing_suppl_gids = 0;
    gid_t existing_gid = getegid();
    uint32_t stepid = 0;

    memset(&imageData, 0, sizeof(ImageData));
    if (lgplSpank_get_stepid(ssConfig->id, &stepid) == ERROR) {
        slurm_error("FAILED to get stepid");
    }

#define TASKINITPRIV_ERROR(message, errCode) \
    slurm_error(message); \
    rc = errCode; \
    goto _taskInitPriv_exit_unclean;

    if (nativeSlurm == 0) return SUCCESS;
    if (udiConfig == NULL) {
        read_config(argc, argv);
    }
    if (udiConfig == NULL) {
        TASKINITPRIV_ERROR("Failed to load udiRoot config!", ERROR);
    }
    doForceArgParse(sp);

    if (strlen(image) == 0) {
        return rc;
    }
    if (strlen(image) == 0 || strlen(image_type) == 0) {
        return rc;
    }

    /* if this is the slurmstepd for prologflags=contain, then do the
     * proper setup to finalize shifter setup */
    if (stepid == SLURM_EXTERN_CONT) {
        return rc;
    } else {
#if 0
        /* need to ensure the extern step is setup */
        char buffer[PATH_MAX];
        struct stat statData;
        int tries = 0;
        snprintf(buffer, PATH_MAX, "%s/var/shifterExtern.complete", udiConfig->udiMountPoint);
        for (tries = 0; tries < 10; tries++) {
            if (stat(buffer, &statData) == 0) {
                break;
            } else {
                sleep(1);
            }
        }
#endif
    }

    if (ccmMode == 0) return rc;

    parse_ImageData(image_type, image, udiConfig, &imageData);

    if (lgplSpank_get_uid(ssConfig->id, &job_uid) == ERROR) {
        TASKINITPRIV_ERROR("FAILED to get job uid!", ERROR);
    }

    if (strlen(udiConfig->udiMountPoint) > 0) {
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
        snprintf(newcwd, PATH_MAX, "%s", udiConfig->udiMountPoint);
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

        if (chroot(udiConfig->udiMountPoint) != 0) {
            TASKINITPRIV_ERROR("FAILED to chroot to designated image", ERROR);
        }

        if (lgplSpank_get_supplementary_gids(ssConfig->id, &gids, &ngids) == ERROR) {
            TASKINITPRIV_ERROR("FAILED to obtain group ids", ERROR);
        }

        if (lgplSpank_get_gid(ssConfig->id, &gid) == ERROR) {
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
            slurm_error("FAILED to change directory to %s, going to /tmp instead.", currcwd);
            chdir("/tmp");
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

        if (shifter_setupenv(&environ, &imageData, udiConfig) != 0) {
            TASKINITPRIV_ERROR("FAILED to setup shifter environment", ERROR);
        }
    }
_taskInitPriv_exit_unclean:
    free_ImageData(&imageData, 0);
    return rc;
}
