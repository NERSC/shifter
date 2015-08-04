#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>

#include <slurm/spank.h>
#include <slurm/slurm.h> // for job prolog where job data structure is loaded

#include "UdiRootConfig.h"
#include "shifter_core.h"

SPANK_PLUGIN(shifter, 1)

#ifndef IS_NATIVE_SLURM
#define IS_NATIVE_SLURM 0
#endif

#define IMAGE_MAXLEN 1024
#define IMAGEVOLUME_MAXLEN 2048
static char image[IMAGE_MAXLEN] = "";
static char image_type[IMAGE_MAXLEN] = "";
static char imagevolume[IMAGEVOLUME_MAXLEN] = "";
static int nativeSlurm = IS_NATIVE_SLURM;

static int _opt_image(int val, const char *optarg, int remote);
static int _opt_imagevolume(int val, const char *optarg, int remote);
static int _opt_ccm(int val, const char *optarg, int remote);

struct spank_option spank_option_array[] = {
    { "image", "image", "shifter image to use", 1, 0, (spank_opt_cb_f) _opt_image},
    { "imagevolume", "imagevolume", "shifter image bindings", 1, 0, (spank_opt_cb_f) _opt_imagevolume },
    { "ccm", "ccm", "ccm emulation mode", 0, 0, (spank_opt_cb_f) _opt_ccm},
    SPANK_OPTIONS_TABLE_END
};

char *trim(char *string) {
    char *ptr = string;
    size_t len = 0;
    while(isspace(*ptr) && *ptr != 0) {
        ptr++;
    }
    len = strlen(ptr);
    while (--len > 0 && isspace(*(ptr+len))) {
        *(ptr + len) = 0;
    }
    return ptr;
}

int _opt_ccm(int val, const char *optarg, int remote) {
    snprintf(image, IMAGE_MAXLEN, "dsl");
    snprintf(image_type, IMAGE_MAXLEN, "local");
    return ESPANK_SUCCESS;
}

int _opt_image(int val, const char *optarg, int remote) {
    if (optarg != NULL && strlen(optarg) > 0) {
        char *tmp = strdup(optarg);
        char *p = strchr(tmp, ':');
        if (p == NULL) {
            slurm_error("Invalid image input: must specify image type: %s", optarg);
            return ESPANK_ERROR;
        }
        *p++ = 0;
        snprintf(image_type, IMAGE_MAXLEN, "%s", tmp);
        snprintf(image, IMAGE_MAXLEN, "%s", p);
        free(tmp);
        p = trim(image);
        if (p != image) memmove(image, p, strlen(p) + 1);
        p = trim(image_type);
        if (p != image_type) memmove(image_type, p, strlen(p) + 1);

        for (p = image_type; *p != 0 && p-image_type < IMAGE_MAXLEN; ++p) {
            if (!isalpha(*p)) {
                slurm_error("Invalid image type - alphabetic characters only");
                return ESPANK_ERROR;
            }
        }
        for (p = image; *p != 0 && p-image < IMAGE_MAXLEN; ++p) {
            if (!isalnum(*p) && (*p!=':') && (*p!='_') && (*p!='-') && (*p!='.') && (*p!='/')) {
                slurm_error("Invalid image type - A-Za-z:-_./ characters only");
                return ESPANK_ERROR;
            }
        }
        return ESPANK_SUCCESS;
    }
    slurm_error("Invalid image - must not be zero length");
    return ESPANK_ERROR;
}
int _opt_imagevolume(int val, const char *optarg, int remote) {
    if (optarg != NULL && strlen(optarg) > 0) {
        /* validate input */
        snprintf(imagevolume, IMAGEVOLUME_MAXLEN, optarg);
        return ESPANK_SUCCESS;
    }
    slurm_error("Invalid image volume options - if specified, must not be zero length");
    return ESPANK_ERROR;
}


/** generateSshKey
 *  checks to see if a udiRoot-specific ssh key exists and creates one if
 *  necessary.  Once an appropriate key exists, the public key is read in
 *  and set in the job_control environment for retrieval in the job prolog
 **/
int generateSshKey(spank_t sp) {
    struct stat st_data;

    char filename[1024];
    char buffer[4096];
    struct passwd pwd;
    struct passwd *ptr = NULL;
    int generateKey = 0;
    int rc = 0;
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
        FILE *fp = NULL;
        char *linePtr = NULL;
        size_t n_linePtr = 0;
        snprintf(filename, 1024, "%s/.udiRoot/id_rsa.key.pub", pwd.pw_dir);
        fp = fopen(filename, "r");
        if (fp != NULL && !feof(fp) && !ferror(fp)) {
            getline(&linePtr, &n_linePtr, fp);
            fclose(fp);
            if (linePtr != NULL) {
                spank_job_control_setenv(sp, "SHIFTER_SSH_PUBKEY", linePtr, 1);
            }
        }
    }
    return 0;
}

UdiRootConfig *read_config(int argc, char **argv) {
    int idx = 0;
    UdiRootConfig *udiConfig = NULL;
    char config_filename[PATH_MAX];

    for (idx = 0; idx < argc; ++idx) {
        if (strncmp("shifter_config=", argv[idx], 15) == 0) {
            char *ptr = strchr(argv[idx], '=');
            snprintf(config_filename, 1024, "%s", ptr + 1);
            ptr = trim(config_filename);
            if (ptr != config_filename) memmove(config_filename, ptr, strlen(ptr) + 1);
        }
    }

    udiConfig = malloc(sizeof(UdiRootConfig));
    if (udiConfig == NULL) {
        fprintf(stderr, "FAILED to allocate memory to read udiRoot configuration\n");
        return NULL;
    }
    memset(udiConfig, 0, sizeof(UdiRootConfig));

    if (parse_UdiRootConfig(config_filename, udiConfig, 0) != 0) {
        fprintf(stderr, "FAILED to read udiRoot configuration file!\n");
        free(udiConfig);
        return NULL;
    }

    return udiConfig;
}

int lookup_image(UdiRootConfig *config, int verbose, const char *mode, char **image_id) {
    int rc = 0;
    char buffer[4096];
    char buff1[1024];
    char buff2[1024];
    FILE *fp = NULL;
    ssize_t nread = 0;
    char *ptr = NULL;
    char *linePtr = NULL;
    size_t linePtrSize = 0;
    //verbose = 0;
    /* perform image lookup */
    snprintf(buffer, 4096, "%s%s/bin/getDockerImage.pl %s%s %s", config->nodeContextPrefix, config->udiRootPath, (verbose == 0 ? "-quiet " : ""), mode, image);
    printf("LOOKUP COMMAND: %s\n", buffer);
    fp = popen(buffer, "r");
    while ((nread = getline(&linePtr, &linePtrSize, fp)) > 0) {
        linePtr[nread] = 0;
        printf("RAW OUTPUT: %s\n", linePtr);
        ptr = trim(linePtr);
        printf("TRIMMED OUTPUT: %s\n", linePtr);
        if (verbose) {
            printf("%s\n", linePtr);
            int val = sscanf(linePtr, "Retrieved docker image %1024s resolving to ID %1024s", buff1, buff2);
            if (val != 2) {
                continue;
            }
            if (strcmp(buff1, image) != 0) {
                slurm_error("Got wrong image back!");
                return ESPANK_ERROR;
            }
            *image_id = (char *) realloc(*image_id, sizeof(char) * (strlen(buff2) + 1));
            snprintf(*image_id, strlen(buff2)+1, "%s", buff2);
        } else {
            if (strncmp(ptr, "ENV:", 4) == 0) {
                ptr += 4;
                ptr = trim(ptr);
            } else if (strncmp(ptr, "ENTRY:", 6) == 0) {
                ptr += 6;
                ptr = trim(ptr);
            } else {
                /* this is the image id */
                size_t nbytes = 0;
                *image_id = (char *) realloc(*image_id, sizeof(char) * (strlen(ptr) + 2));
                nbytes = snprintf(*image_id, strlen(ptr) + 1, "%s", ptr);
                *image_id[nbytes] = 0;
                printf("FOUND IMAGE: %s, %s\n", ptr, *image_id);
            }
        }

    }
    if (linePtr != NULL) {
        free(linePtr);
        linePtr = NULL;
        linePtrSize = 0;
    }
    if ((rc = pclose(fp)) != 0) {
        slurm_error("Image lookup process failed, exit status: %d", rc);
        exit(-1);
    }
    return 0;
}

int slurm_spank_init(spank_t sp, int argc, char **argv) {
    spank_context_t context;
    int rc = ESPANK_SUCCESS;
    int i, j;

    context = spank_context();

    image[0] = 0;
    imagevolume[0] = 0;

    //if (context == S_CTX_ALLOCATOR || context == S_CTX_LOCAL) {
        for (i = 0; spank_option_array[i].name != NULL; ++i) {
            j = spank_option_register(sp, &spank_option_array[i]);
            if (j != ESPANK_SUCCESS) {
                slurm_error("Could not register spank option %s", spank_option_array[i].name);
                rc = j;
            }
        }
    //}
    return rc;
}

int slurm_spank_init_post_opt(spank_t sp, int argc, char **argv) {
    spank_context_t context;
    int rc = ESPANK_SUCCESS;
    int verbose_lookup = 0;
    UdiRootConfig *udiConfig = NULL;

    // only perform this validation at submit time
    context = spank_context();
    if (context != S_CTX_ALLOCATOR) {
        return ESPANK_SUCCESS;
    }

    udiConfig = read_config(argc, argv);
    if (udiConfig == NULL) {
        slurm_error("Failed to parse shifter config. Cannot use shifter.");
        return rc;
    }

    verbose_lookup = 1;
    if (imagevolume != NULL && image == NULL) {
        slurm_error("Cannot specify shifter volumes without specifying the image first!");
        exit(-1);
    }
    
    if (strncmp(image_type, "docker", IMAGE_MAXLEN) == 0) {
        char *image_id = NULL;
        lookup_image(udiConfig, verbose_lookup, "lookup", &image_id);
        if (image_id == NULL) {
            slurm_error("Failed to lookup image.  Aborting.");
            exit(-1);
        }
        snprintf(image, IMAGE_MAXLEN, "%s", image_id);
        free(image_id);
    }
    if (strlen(image) == 0) {
        return rc;
    }

    if (nativeSlurm) {
        /* for slurm native, generate ssh keys here */
        generateSshKey(sp);
    }
    
    spank_setenv(sp, "CRAY_ROOTFS", "UDI", 1);
    spank_setenv(sp, "SHIFTER_IMAGE", image, 1);
    spank_setenv(sp, "SHIFTER_IMAGETYPE", image_type, 1);
    spank_job_control_setenv(sp, "SHIFTER_IMAGE", image, 1);
    spank_job_control_setenv(sp, "SHIFTER_IMAGETYPE", image_type, 1);
    
    if (imagevolume != NULL) {
         spank_setenv(sp, "SHIFTER_VOLUME", imagevolume, 1);
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
int read_data_from_job(spank_t sp, uint32_t *jobid, char **nodelist, size_t *tasksPerNode) {
    job_info_msg_t *job_buf = NULL;
    hostlist_t hl;
    char *raw_host_string = NULL;

    /* load job data and fail if not possible */
    if (spank_get_item(sp, S_JOB_ID, jobid) != ESPANK_SUCCESS) {
        slurm_error("Couldnt get job id");
        return ESPANK_ERROR;
    }
    if (slurm_load_job(&job_buf, *jobid, SHOW_ALL) != 0) {
        slurm_error("Couldn't load job data");
        return ESPANK_ERROR;
    }
    if (job_buf->record_count != 1) {
        slurm_error("Can't deal with this job!");
        slurm_free_job_info_msg(job_buf);
        return ESPANK_ERROR;
    }

    /* get tasksPerNode */
    *tasksPerNode = job_buf->job_array->num_cpus / job_buf->job_array->num_nodes;

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
                    "%s/%s%c", r_ptr, tmp, (i + 1 == n_nodes ? '\0' : ','));
            r_ptr = e_ptr + 1;
        }
        free(raw_host_string);
    }
    slurm_free_job_info_msg(job_buf);
    slurm_hostlist_destroy(hl);
    return ESPANK_SUCCESS;
}

int slurm_spank_job_prolog(spank_t sp, int argc, char **argv) {
    int rc = ESPANK_SUCCESS;

    char *ptr = NULL;
    int idx = 0;
    int i,j;
    uint32_t job;
    uid_t uid = 0;

    char buffer[1024];
    char setupRootPath[PATH_MAX];
    char **setupRootArgs = NULL;
    size_t n_setupRootArgs = 0;
    char **volArgs = NULL;
    size_t n_volArgs = 0;
    char *nodelist = NULL;
    char *username = NULL;
    char *uid_str = NULL;
    char *sshPubKey = NULL;
    size_t tasksPerNode = 0;
    UdiRootConfig *udiConfig = NULL;
    pid_t pid = 0;

#define PROLOG_ERROR(message, errCode) \
    slurm_error(message); \
    rc = errCode; \
    goto _prolog_exit_unclean;

    for (i = 0; spank_option_array[i].name != NULL; ++i) {
        char *optarg = NULL;
        j = spank_option_getopt(sp, &spank_option_array[i], &optarg);
        if (j != ESPANK_SUCCESS) {
            continue;
        }
        (spank_option_array[i].cb)(spank_option_array[i].val, optarg, 1);
    }

    /* if processing the user-specified options indicates no image, dump out */
    if (strlen(image) == 0 || strlen(image_type) == 0) {
        return rc;
    }

    slurm_error("DMJ: FOUND IMAGE ARGS, starting shifter prolog");

    ptr = getenv("SHIFTER_IMAGETYPE");
    if (ptr != NULL) {
        snprintf(image_type, IMAGE_MAXLEN, "%s", ptr);
    }

    ptr = getenv("SHIFTER_IMAGE");
    if (ptr != NULL) {
        slurm_error("DMJ: got SHIFTER_IMAGE: %s\n", ptr);
        snprintf(image, IMAGE_MAXLEN, "%s", ptr);
    }

    ptr = getenv("SHIFTER_VOLUME");
    if (ptr != NULL) {
        snprintf(imagevolume, IMAGEVOLUME_MAXLEN, "%s", ptr);
    }

    slurm_error("DMJ: SHIFTER, about to read configation");
    /* parse udi configuration */
    udiConfig = read_config(argc, argv);
    if (udiConfig == NULL) {
        PROLOG_ERROR("Failed to read/parse shifter configuration.\n", rc);
    }

    for (ptr = image_type; ptr - image_type < strlen(image_type); ptr++) {
        *ptr = toupper(*ptr);
    }

    slurm_error("DMJ: SHIFTER, about to read job data");
    rc = read_data_from_job(sp, &job, &nodelist, &tasksPerNode);
    if (rc != ESPANK_SUCCESS) {
        PROLOG_ERROR("FAILED to get job information.", ESPANK_ERROR);
    }

    slurm_error("DMJ: SHIFTER, about to read ssh pub key");
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
        if (spank_get_item(sp, S_JOB_UID, &uid) != ESPANK_SUCCESS) {
            PROLOG_ERROR("FAILED to get job uid!", ESPANK_ERROR);
        }
    }

    /* try to get username from environment first, then fallback to getpwuid */
    username = getenv("SLURM_JOB_USER");
    if (username != NULL) {
        username = strdup(username);
    } else if (uid != 0) {
        /* getpwuid may not be optimal on cray compute node, but oh well */
        char buffer[4096];
        struct passwd pw, *result;
        while (1) {
            rc = getpwuid_r(uid, &pw, buffer, 4096, &result);
            if (rc == EINTR) continue;
            if (rc != 0) result = NULL;
            break;
        }
        if (result != NULL) 
            username = strdup(result->pw_name);
    }

    uid_str = getenv("SLURM_JOB_UID");
    if (uid_str != NULL) {
        uid = strtoul(uid_str, NULL, 10);
    }

    /* setupRoot argument construction 
       /path/to/setupRoot <imageType> <imageIdentifier> -u <uid> -U <username>
            [-v volMap ...] -s <sshPubKey> -N <nodespec> NULL
     */
    n_setupRootArgs = 4;
    if (sshPubKey != NULL && username != NULL && uid != 0) {
        n_setupRootArgs += 6;
    }
    if (strlen(imagevolume) > 0) {
        char *ptr = imagevolume;
        for ( ; ; ) {
            char *limit = strchr(ptr, ',');
            volArgs = (char **) realloc(volArgs,sizeof(char *) * (n_volArgs + 1));
            if (limit != NULL) *limit = 0;
            volArgs[n_volArgs++] = strdup(ptr);


            if (limit == NULL) {
                break;
            }
            ptr = limit + 1;
        }
        n_setupRootArgs += 2 * n_volArgs;
    }
    snprintf(setupRootPath, PATH_MAX, "%s%s/sbin/setupRoot", udiConfig->nodeContextPrefix, udiConfig->udiRootPath);
    setupRootArgs = malloc(sizeof(char*) * n_setupRootArgs);
    idx = 0;
    setupRootArgs[idx++] = strdup(setupRootPath);
    /*
    if (uid != 0) {
        snprintf(buffer, 1024, "%u", uid);
        setupRootArgs[idx++] = strdup("-u");
        setupRootArgs[idx++] = strdup(buffer);
    }
    if (username != NULL) {
        setupRootArgs[idx++] = strdup("-U");
        setupRootArgs[idx++] = strdup(username);
    }
    if (sshPubKey != NULL) {
        setupRootArgs[idx++] = strdup("-s");
        setupRootArgs[idx++] = strdup(sshPubKey);
    }
    if (nodelist != NULL) {
        setupRootArgs[idx++] = strdup("-N");
        setupRootArgs[idx++] = strdup(nodelist);
    }
    */
    setupRootArgs[idx++] = strdup(image_type);
    setupRootArgs[idx++] = strdup(image);
    setupRootArgs[idx++] = NULL;

    for (i = 0; i < idx && setupRootArgs[i] != NULL; i++) {
        slurm_error("setupRoot arg %d: %s", i, setupRootArgs[i]);
    }

    /* return success because we don't want bad input to mark node in
       error state -- would be nice to do something to inform the job
       of this issue */
    pid = fork();
    if (pid < 0) {
        PROLOG_ERROR("FAILED to fork setupRoot", ESPANK_ERROR);
    } else if (pid > 0) {
        /* this is the parent */
        int status = 0;
        slurm_error("waiting on child\n");
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
             status = WEXITSTATUS(status);
        } else {
             status = 1;
        }
        if (status != 0) PROLOG_ERROR("FAILED to run setupRoot", ESPANK_ERROR);
    } else {
        execv(setupRootArgs[0], setupRootArgs);
    }
    
_prolog_exit_unclean:
    if (udiConfig != NULL) free_UdiRootConfig(udiConfig);
    if (setupRootArgs != NULL) {
        char **ptr = setupRootArgs;
        while (*ptr != NULL) free(*ptr++);
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

int slurm_spank_job_epilog(spank_t sp, int argc, char **argv) {
    int rc = ESPANK_SUCCESS;
    spank_context_t context;
    UdiRootConfig *udiConfig = NULL;
    char path[PATH_MAX];
    char *epilogueArgs[2];
    int i, j;
    pid_t pid = 0;

#define EPILOG_ERROR(message, errCode) \
    slurm_error(message); \
    rc = errCode; \
    goto _epilog_exit_unclean;

    context = spank_context();
    for (i = 0; spank_option_array[i].name != NULL; ++i) {
        char *optarg = NULL;
        j = spank_option_getopt(sp, &spank_option_array[i], &optarg);
        if (j != ESPANK_SUCCESS) {
            continue;
        }
        (spank_option_array[i].cb)(spank_option_array[i].val, optarg, 1);
    }
    if (strlen(image) == 0) {
        return rc;
    }

    udiConfig = read_config(argc, argv);
    if (udiConfig == NULL) {
        EPILOG_ERROR("Failed to read/parse shifter configuration.\n", rc);
    }


    snprintf(path, PATH_MAX, "%s%s/sbin/unsetupRoot", udiConfig->nodeContextPrefix, udiConfig->udiRootPath);
    epilogueArgs[0] = path;
    epilogueArgs[1] = NULL;
    pid = fork();
    if (pid < 0) {
        EPILOG_ERROR("FAILED to fork unsetupRoot", ESPANK_ERROR);
    } else if (pid > 0) {
        /* this is the parent */
        int status = 0;
        slurm_error("waiting on child\n");
        do {
            pid_t ret = waitpid(pid, &status, 0);
            if (ret != pid) {
                slurm_error("This might be impossible: forked by couldn't wait, FAIL!\n");
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        if (WIFEXITED(status)) {
             status = WEXITSTATUS(status);
        } else {
             status = 1;
        }
        if (status != 0) EPILOG_ERROR("FAILED to run unsetupRoot", ESPANK_ERROR);
    } else {
        execv(epilogueArgs[0], epilogueArgs);
    }
    
_epilog_exit_unclean:
    if (udiConfig != NULL)
        free_UdiRootConfig(udiConfig);
    return rc;
}

int slurm_spank_task_init_privileged(spank_t sp, int argc, char **argv) {
    int rc = ESPANK_SUCCESS;
    UdiRootConfig *udiConfig = NULL;

#define TASKINITPRIV_ERROR(message, errCode) \
    slurm_error(message); \
    rc = errCode; \
    goto _taskInitPriv_exit_unclean;


    if (nativeSlurm == 0) return ESPANK_SUCCESS;
    if (strlen(image) == 0 || strlen(image_type) == 0) {
        return rc;
    }
    udiConfig = read_config(argc, argv);
    if (udiConfig == NULL) {
        TASKINITPRIV_ERROR("Failed to load udiRoot config!", ESPANK_ERROR);
    }

    if (strlen(udiConfig->udiMountPoint) > 0) {
        char currcwd[PATH_MAX];
        char newcwd[PATH_MAX];
        struct stat st_data;
        if (getcwd(currcwd, PATH_MAX) == NULL) {
            TASKINITPRIV_ERROR("FAILED to determine working directory", ESPANK_ERROR);
        }

        // check to see if newcwd exists, if not, just chdir to the new chroot base
        snprintf(newcwd, PATH_MAX, "%s/%s", udiConfig->udiMountPoint, currcwd);
        if (stat(newcwd, &st_data) != 0) {
            snprintf(newcwd, PATH_MAX, "%s", udiConfig->udiMountPoint);
        }
        if (chdir(newcwd) != 0) {
            char error[PATH_MAX];
            snprintf(error, PATH_MAX, "FAILED to change directory to %s", newcwd);
            TASKINITPRIV_ERROR(error, ESPANK_ERROR);
        }

        if (chroot(udiConfig->udiMountPoint) != 0) {
            TASKINITPRIV_ERROR("FAILED to chroot to designated image", ESPANK_ERROR);
        }
    }
_taskInitPriv_exit_unclean:
    if (udiConfig != NULL)
        free_UdiRootConfig(udiConfig);
    return rc;
}
