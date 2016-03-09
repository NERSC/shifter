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

#include <slurm/spank.h>
#include <slurm/slurm.h> // for job prolog where job data structure is loaded

#include "UdiRootConfig.h"
#include "shifter_core.h"
#include "utility.h"

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
static int ccmMode = 0;
static int serialMode = 0;
static int trustedImage = 1;
static int autoshift = 0;

static int _opt_image(int val, const char *optarg, int remote);
static int _opt_imagevolume(int val, const char *optarg, int remote);
static int _opt_ccm(int val, const char *optarg, int remote);
static int _opt_autoshift(int val, const char *optarg, int remote);

/* using a couple functions from libslurm that aren't prototyped in any
   accessible header file */
extern hostlist_t slurm_hostlist_create_dims(const char *hostlist, int dims);
extern char *slurm_hostlist_deranged_string_malloc(hostlist_t hl);

struct spank_option spank_option_array[] = {
    { "image", "image", "shifter image to use", 1, 0, (spank_opt_cb_f) _opt_image},
    { "volume", "volume", "shifter image bindings", 1, 0, (spank_opt_cb_f) _opt_imagevolume },
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
    snprintf(image, IMAGE_MAXLEN, "/");
    snprintf(image_type, IMAGE_MAXLEN, "local");
    ccmMode = 1;
    return ESPANK_SUCCESS;
}

int _opt_autoshift(int val, const char *optarg, int remote) {
    autoshift = 1;
    return ESPANK_SUCCESS;
}

int _opt_image(int val, const char *optarg, int remote) {
    char *tmp = NULL;
    int rc = ESPANK_SUCCESS;
    if (optarg != NULL && strlen(optarg) > 0) {
        tmp = strdup(optarg);
        char *p = strchr(tmp, ':');
        if (p == NULL) {
            slurm_error("Invalid image input: must specify image type: %s", optarg);
            rc = ESPANK_ERROR;
            goto _opt_image_exit;
        }
        *p++ = 0;
        snprintf(image_type, IMAGE_MAXLEN, "%s", tmp);
        snprintf(image, IMAGE_MAXLEN, "%s", p);
        free(tmp);
        tmp = NULL;
        p = trim(image);
        if (p != image) memmove(image, p, strlen(p) + 1);
        p = trim(image_type);
        if (p != image_type) memmove(image_type, p, strlen(p) + 1);

        for (p = image_type; *p != 0 && p-image_type < IMAGE_MAXLEN; ++p) {
            if (!isalpha(*p)) {
                slurm_error("Invalid image type - alphabetic characters only");
                rc = ESPANK_ERROR;
                goto _opt_image_exit;
            }
        }
        for (p = image; *p != 0 && p-image < IMAGE_MAXLEN; ++p) {
            if (!isalnum(*p) && (*p!=':') && (*p!='_') && (*p!='-') && (*p!='.') && (*p!='/')) {
                slurm_error("Invalid image type - A-Za-z:-_./ characters only");
                rc =  ESPANK_ERROR;
                goto _opt_image_exit;
            }
        }
        return ESPANK_SUCCESS;
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
                spank_job_control_setenv(sp, "SHIFTER_SSH_PUBKEY", linePtr, 1);
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

    if (parse_UdiRootConfig(config_filename, udiConfig, UDIROOT_VAL_ALL) != 0) {
        fprintf(stderr, "FAILED to read udiRoot configuration file!\n");
        free(udiConfig);
        return NULL;
    }

    return udiConfig;
}

char *lookup_ImageIdentifier(const char *imageType, const char *imageTag, int verbose, UdiRootConfig *);

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
    if (strlen(image) == 0) {
        return rc;
    }

    udiConfig = read_config(argc, argv);
    if (udiConfig == NULL) {
        slurm_error("Failed to parse shifter config. Cannot use shifter.");
        return rc;
    }

    verbose_lookup = 1;
    if (strlen(imagevolume) > 0 && strlen(image) == 0) {
        slurm_error("Cannot specify shifter volumes without specifying the image first!");
        exit(-1);
    }
    
    if (strcmp(image_type, "id") != 0 && strcmp(image_type, "local") != 0) {
        char *image_id = NULL;
        image_id = lookup_ImageIdentifier(image_type, image, verbose_lookup, udiConfig);
        if (image_id == NULL) {
            slurm_error("Failed to lookup image.  Aborting.");
            exit(-1);
        }
        snprintf(image, IMAGE_MAXLEN, "%s", image_id);
        snprintf(image_type, IMAGE_MAXLEN, "id");
        free(image_id);
    }
    if (strlen(image) == 0) {
        return rc;
    }

    if (nativeSlurm) {
        /* for slurm native, generate ssh keys here */
        generateSshKey(sp);
    }
    
    spank_setenv(sp, "SHIFTER_IMAGE", image, 1);
    spank_setenv(sp, "SHIFTER_IMAGETYPE", image_type, 1);
    spank_job_control_setenv(sp, "SHIFTER_IMAGE", image, 1);
    spank_job_control_setenv(sp, "SHIFTER_IMAGETYPE", image_type, 1);
    
    if (strlen(imagevolume) > 0) {
        spank_setenv(sp, "SHIFTER_VOLUME", imagevolume, 1);
        spank_job_control_setenv(sp, "SHIFTER_VOLUME", imagevolume, 1);
    }
    if (getgid() != 0) {
        char buffer[128];
        snprintf(buffer, 128, "%d", getgid());
        spank_setenv(sp, "SHIFTER_GID", buffer, 1);
        spank_job_control_setenv(sp, "SHIFTER_GID", buffer, 1);
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
    return ESPANK_SUCCESS;
}

const char *find_memory_cgroup_base(int argc, char **argv) {
    int idx = 0;
    for (idx = 0; idx < argc; idx++) {
        if (strncmp(argv[idx], "memory_cgroup=", 14) == 0) {
            const char *ptr = argv[idx];
            ptr += 14;
            return ptr;
        }
    }
    return NULL;
}

const char *find_cpuset_cgroup_base(int argc, char **argv) {
    int idx = 0;
    for (idx = 0; idx < argc; idx++) {
        if (strncmp(argv[idx], "cpuset_cgroup=", 14) == 0) {
            const char *ptr = argv[idx];
            ptr += 14;
            return ptr;
        }
    }
    return NULL;
}

int setupCpusetCgroupForSshd(const char *cpuset_cgroup_base,
        uid_t uid,
        int job,
        pid_t pid)
{
    char *ptr = NULL;
    FILE *fp = NULL;

    char *allowableCpus = NULL;
    char *allowableMems = NULL;
    char *line = NULL;
    char *value = NULL;
    size_t line_sz = 0;

    char buffer[PATH_MAX];
    char wbuffer[PATH_MAX];
    ssize_t nbytes = 0;
    int fd = 0;

    int ret = 1;

    /* first need to read cpus and mems to figure out what to give sshd */
    snprintf(buffer, PATH_MAX, "%s/cpus", cpuset_cgroup_base);
    fp = fopen(buffer, "r");
    if (fp == NULL) {
        slurm_error("setupCpusetCgroupForSshd: failed to open base cpus allocation");
        goto _setupCpusetCgroupForSshd_unclean;
    }
    nbytes = getline(&line, &line_sz, fp);
    value = nbytes > 0 ? trim(line) : NULL;
    if (value != NULL) {
        allowableCpus = strdup(value);
    } else {
        slurm_error("setupCpusetCgroupForSshd: failed to read base cpus allocation");
        goto _setupCpusetCgroupForSshd_unclean;
    }
    fclose(fp);
    fp = NULL;

    snprintf(buffer, PATH_MAX, "%s/mems", cpuset_cgroup_base);
    fp = fopen(buffer, "r");
    if (fp == NULL) {
        slurm_error("setupCpusetCgroupForSshd: failed to open base mems allocation");
        goto _setupCpusetCgroupForSshd_unclean;
    }
    nbytes = getline(&line, &line_sz, fp);
    value = nbytes > 0 ? trim(line) : NULL;
    if (value != NULL) {
        allowableMems = strdup(value);
    } else {
        slurm_error("setupCpusetCgroupForSshd: failed to read base mems allocation");
        goto _setupCpusetCgroupForSshd_unclean;
    }
    fclose(fp);
    fp = NULL;

    /* build path writing cpus and mems the whole way */
    snprintf(buffer, PATH_MAX, "%s/shifter/uid_%d/job_%d/ssh/", cpuset_cgroup_base, uid, job);
    ptr = strstr(buffer, "shifter");
    while ((ptr = strchr(ptr, '/')) != NULL) {
        int fd = 0;

        /* temporarily terminate the string here */
        *ptr = 0;

        /* create the cpuset, it's OK for mkdir to fail if dir already exists */
        slurm_debug("setupCpusetCgroupForSshd: about to attempt to create: %s", buffer);
        if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
            slurm_error("setupCpusetCgroupForSshd: failed to mkdir %s: %d", buffer, errno);
            goto _setupCpusetCgroupForSshd_unclean;
        }
        errno = 0;

        /* write allowable cpus */
        snprintf(wbuffer, PATH_MAX, "%s/cpus", buffer);
        fd = open(wbuffer, O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, 0666);
        if (fd >= 0) {
            nbytes = write(fd, allowableCpus, strlen(allowableCpus));
            slurm_debug("setupCpusetCgroupForSshd: write %d bytes to set %s on %s, errno: %d", nbytes, allowableCpus, wbuffer, errno);
            close(fd);
        } else {
            slurm_error("setupCpusetCgroupForSshd: failed to open %s", wbuffer);
            goto _setupCpusetCgroupForSshd_unclean;
        }

        /* write allowable mems */
        snprintf(wbuffer, PATH_MAX, "%s/mems", buffer);
        fd = open(wbuffer, O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, 0666);
        if (fd >= 0) {
            nbytes = write(fd, allowableMems, strlen(allowableMems));
            slurm_debug("setupCpusetCgroupForSshd: write %d bytes to set %s on %s, errno: %d", nbytes, allowableMems, wbuffer, errno);
            close(fd);
        } else {
            slurm_error("setupCpusetCgroupForSshd: failed to open %s", wbuffer);
            goto _setupCpusetCgroupForSshd_unclean;
        }

        /* restore string to usual state */
        *ptr = '/';
        ptr++;
    }

    /* add pid to tasks list */
    snprintf(wbuffer, PATH_MAX, "%s/tasks", buffer);
    fd = open(wbuffer, O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, 0666);
    if (fd >= 0) {
        snprintf(buffer, PATH_MAX, "%d", pid);
        nbytes = write(fd, buffer, strlen(buffer));
        slurm_debug("setupCpusetCgroupForSshd: write %d bytes to cpuset cgroup, errno: %d", nbytes, errno);
        close(fd);
    } else {
        slurm_error("setupCpusetCgroupForSshd: failed to open %s", wbuffer);
        goto _setupCpusetCgroupForSshd_unclean;
    }

    ret = 0;

_setupCpusetCgroupForSshd_unclean:
    if (line != NULL) {
        free(line);
        line = NULL;
    }
    if (allowableCpus != NULL) {
        free(allowableCpus);
        allowableCpus = NULL;
    }
    if (allowableMems != NULL) {
        free(allowableMems);
        allowableMems = NULL;
    }
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
    return ret;
}

int slurm_spank_job_prolog(spank_t sp, int argc, char **argv) {
    int rc = ESPANK_SUCCESS;

    char *ptr = NULL;
    int idx = 0;
    int i,j;
    uint32_t job;
    uid_t uid = 0;
    gid_t gid = 0;
    uint16_t shared = 0;

    char buffer[1024];
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
    const char *memory_cgroup_base = NULL;
    const char *cpuset_cgroup_base = NULL;
    size_t tasksPerNode = 0;
    UdiRootConfig *udiConfig = NULL;
    pid_t pid = 0;

    /* pipes for reading from setupRoot */
    int stdoutPipe[2];
    int stderrPipe[2];

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

    ptr = getenv("SHIFTER_IMAGETYPE");
    if (ptr != NULL) {
        snprintf(image_type, IMAGE_MAXLEN, "%s", ptr);
    }

    ptr = getenv("SHIFTER_IMAGE");
    if (ptr != NULL) {
        snprintf(image, IMAGE_MAXLEN, "%s", ptr);
    }

    ptr = getenv("SHIFTER_VOLUME");
    if (ptr != NULL) {
        snprintf(imagevolume, IMAGEVOLUME_MAXLEN, "%s", ptr);
    }

    /* parse udi configuration */
    udiConfig = read_config(argc, argv);
    if (udiConfig == NULL) {
        PROLOG_ERROR("Failed to read/parse shifter configuration.\n", rc);
    }
    memory_cgroup_base = find_memory_cgroup_base(argc, argv);
    cpuset_cgroup_base = find_cpuset_cgroup_base(argc, argv);

    for (ptr = image_type; ptr - image_type < strlen(image_type); ptr++) {
        *ptr = tolower(*ptr);
    }

    rc = read_data_from_job(sp, &job, &nodelist, &tasksPerNode, &shared);
    if (rc != ESPANK_SUCCESS) {
        PROLOG_ERROR("FAILED to get job information.", ESPANK_ERROR);
    }

    /* this prolog should not be used for shared-node jobs */
    if (shared != 0) {
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
        if (spank_get_item(sp, S_JOB_UID, &uid) != ESPANK_SUCCESS) {
            PROLOG_ERROR("FAILED to get job uid!", ESPANK_ERROR);
        }
    }

    gid_str = getenv("SHIFTER_GID");
    if (gid_str != NULL) {
        gid = strtoul(gid_str, NULL, 10);
    } else {
        if (spank_get_item(sp, S_JOB_GID, &gid) != ESPANK_SUCCESS) {
            PROLOG_ERROR("FAILED to get job gid!", ESPANK_ERROR);
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
    snprintf(setupRootPath, PATH_MAX, "%s%s/sbin/setupRoot", udiConfig->nodeContextPrefix, udiConfig->udiRootPath);
    strncpy_StringArray(setupRootPath, strlen(setupRootPath), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
    if (uid != 0) {
        snprintf(buffer, 1024, "%u", uid);
        strncpy_StringArray("-U", 3, &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
        strncpy_StringArray(buffer, strlen(buffer), &setupRootArgs_sv, &setupRootArgs, &n_setupRootArgs, 10);
    }
    if (gid != 0) {
        snprintf(buffer, 1024, "%u", gid);
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
        slurm_error("setupRoot arg %d: %s", (int)(setupRootArgs_sv - setupRootArgs), *setupRootArgs_sv);
    }

    /* return success because we don't want bad input to mark node in
       error state -- would be nice to do something to inform the job
       of this issue */
    pipe(stdoutPipe);
    pipe(stderrPipe);
    pid = fork();
    if (pid < 0) {
        PROLOG_ERROR("FAILED to fork setupRoot", ESPANK_ERROR);
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
                    slurm_error("setupRoot stdout: %s", lineBuffer);
                } else {
                    fclose(stdoutStream);
                    stdoutStream = NULL;
                }
            }
            if (stderrStream) {
                ssize_t nBytes = getline(&lineBuffer, &lineBuffer_sz, stderrStream);
                if (nBytes > 0) {
                    slurm_error("setupRoot stderr: %s", lineBuffer);
                } else {
                    fclose(stderrStream);
                    stderrStream = NULL;
                }
            }
        }


        /* wait on the child */
        slurm_error("waiting on child\n");
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
             status = WEXITSTATUS(status);
        } else {
             status = 1;
        }
        if (status != 0) {
            PROLOG_ERROR("FAILED to run setupRoot", ESPANK_ERROR);
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


        execv(setupRootArgs[0], setupRootArgs);
        exit(127);
    }
    slurm_error("after setupRoot");

    pid = findSshd();
    slurm_debug("shifter_prolog: sshd on pid %d\n", pid);
    if (pid > 0 && cpuset_cgroup_base != NULL) {
        if (setupCpusetCgroupForSshd(cpuset_cgroup_base, uid, job, pid) != 0) {
            slurm_error("shifter_prolog: failed to add sshd %d to cpuset cgroup; killing sshd", pid);
            kill(pid, SIGKILL);
            pid = 0;
        }
    }

    if (pid > 0 && memory_cgroup_base != NULL) {
        char *path = NULL;
        size_t pathLen = 0;
        size_t pathCapacity = 0;
        int taskFilefd = 0;

        /* create cgroup and append ssnd to cgroup */
        path = alloc_strcatf(path, &pathLen, &pathCapacity, "%s/shifter", memory_cgroup_base);
        slurm_debug("shifter_prolog: about to attempt to create: %s", path);
        mkdir(path, 0755);
        path = alloc_strcatf(path, &pathLen, &pathCapacity, "/uid_%d", uid);
        slurm_debug("shifter_prolog: about to attempt to create: %s", path);
        mkdir(path, 0755);
        path = alloc_strcatf(path, &pathLen, &pathCapacity, "/job_%d", job);
        slurm_debug("shifter_prolog: about to attempt to create: %s", path);
        mkdir(path, 0755);
        path = alloc_strcatf(path, &pathLen, &pathCapacity, "/ssh");
        slurm_debug("shifter_prolog: about to attempt to create: %s", path);
        mkdir(path, 0755);
        path = alloc_strcatf(path, &pathLen, &pathCapacity, "/tasks");
        slurm_debug("shifter_prolog: about to attempt to create: %s", path);
        taskFilefd = open(path, O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, 0666);
        if (taskFilefd >= 0) {
            char buffer[128];
            ssize_t written_bytes = 0;
            snprintf(buffer, 128, "%d", pid);
            written_bytes = write(taskFilefd, buffer, strlen(buffer));
            slurm_debug("shifter_prolog: write %d bytes to put pid %d on %s task list, %d", written_bytes, pid, path, errno);
            close(taskFilefd);
        } else {
            slurm_error("shifter_prolog: failed to add sshd %d to %s cgroup", pid, path);
        }
        if (path != NULL) {
            free(path);
            path = NULL;
        }
    }
    
_prolog_exit_unclean:
    if (udiConfig != NULL) free_UdiRootConfig(udiConfig, 1);
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

int slurm_spank_job_epilog(spank_t sp, int argc, char **argv) {
    int rc = ESPANK_SUCCESS;
    spank_context_t context;
    UdiRootConfig *udiConfig = NULL;
    char path[PATH_MAX];
    char *epilogueArgs[2];
    int i, j;
    pid_t pid = 0;
    FILE *cgroup_tasks = NULL;
    char *lineBuffer = NULL;
    size_t lineBuffer_sz = 0;
    char *cgroup_path = NULL;
    const char *memory_cgroup_base = NULL;
    uid_t uid = 0;
    int job = 0;
    int retry = 0;

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

    if (spank_get_item(sp, S_JOB_UID, &uid) != ESPANK_SUCCESS) {
        EPILOG_ERROR("FAILED to get job uid!", ESPANK_ERROR);
    }
    if (spank_get_item(sp, S_JOB_ID, &job) != ESPANK_SUCCESS) {
        EPILOG_ERROR("Couldnt get job id", ESPANK_ERROR);
    }

    memory_cgroup_base = find_memory_cgroup_base(argc, argv);
    if (memory_cgroup_base != NULL) {
        cgroup_path = alloc_strgenf("%s/shifter/uid_%d/job_%d/ssh/tasks", memory_cgroup_base, uid, job);
        slurm_debug("shifter_epilog: cgroup_path is %s", cgroup_path);
        while (retry < 10) {
            int count = 0;
            cgroup_tasks = fopen(cgroup_path, "r");
            if (cgroup_tasks == NULL) break;
            while (!feof(cgroup_tasks) && !ferror(cgroup_tasks)) {
                size_t nread = getline(&lineBuffer, &lineBuffer_sz, cgroup_tasks);
                int pid = 0;
                if (nread == 0 || feof(cgroup_tasks) || ferror(cgroup_tasks)) {
                    break;
                }
                pid = atoi(lineBuffer);
                if (pid == 0) continue;
                slurm_debug("shifter_epilog: sending SIGKILL to %d", pid);
                kill(pid, SIGKILL);
            }
            fclose(cgroup_tasks);
            if (count == 0) break;
            retry++;
        }
        if (lineBuffer != NULL) {
            free(lineBuffer);
            lineBuffer = NULL;
        }
        free(cgroup_path);
    }

    snprintf(path, PATH_MAX, "%s%s/sbin/unsetupRoot", udiConfig->nodeContextPrefix, udiConfig->udiRootPath);
    epilogueArgs[0] = path;
    epilogueArgs[1] = NULL;
    pid = 0;
    pid = fork();
    if (pid < 0) {
        EPILOG_ERROR("FAILED to fork unsetupRoot", ESPANK_ERROR);
    } else if (pid > 0) {
        int status = 0;
        slurm_error("shifter_epilog: waiting on unsetupRoot\n");
        do {
            pid_t ret = waitpid(pid, &status, 0);
            if (ret != pid) {
                slurm_error("This might be impossible: forked by couldn't wait, FAIL!\n");
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        if (WIFEXITED(status)) {
             status = WEXITSTATUS(status);
        } else {
             status = -1;
        }
        slurm_error("shifter_epilog: unsetupRoot completed with status %d", status);
        if (status != 0) {
            rc = ESPANK_ERROR;
        }
    } else {
        execv(epilogueArgs[0], epilogueArgs);
        exit(127);
    }

    slurm_debug("shifter_epilog: done with unsetupRoot");
    if (memory_cgroup_base != NULL) {
        slurm_debug("shifter_epilog: about to remove cgroups");
        cgroup_path = alloc_strgenf("%s/shifter/uid_%d/job_%d/ssh", memory_cgroup_base, uid, job);
        if (rmdir(cgroup_path) != 0) {
            slurm_debug("shifter_epilog: failed to remove %s, errno: %d", cgroup_path, errno);
        }
        free(cgroup_path);
        cgroup_path = alloc_strgenf("%s/shifter/uid_%d/job_%d", memory_cgroup_base, uid, job);
        if (rmdir(cgroup_path) != 0) {
            slurm_debug("shifter_epilog: failed to remove %s, errno: %d", cgroup_path, errno);
        }
        free(cgroup_path);
        cgroup_path = alloc_strgenf("%s/shifter/uid_%d", memory_cgroup_base, uid);
        if (rmdir(cgroup_path) != 0) {
            slurm_debug("shifter_epilog: failed to remove %s, errno: %d", cgroup_path, errno);
        }
        free(cgroup_path);
    }
    
_epilog_exit_unclean:
    if (udiConfig != NULL) {
        free_UdiRootConfig(udiConfig, 1);
    }
    return rc;
}

int slurm_spank_task_init_privileged(spank_t sp, int argc, char **argv) {
    int rc = ESPANK_SUCCESS;
    int i = 0, j = 0;
    UdiRootConfig *udiConfig = NULL;
    ImageData imageData;

    uid_t job_uid = 0;
    uid_t curr_uid = geteuid();

    gid_t *existing_suppl_gids = NULL;
    int n_existing_suppl_gids = 0;
    gid_t existing_gid = getegid();

    memset(&imageData, 0, sizeof(ImageData));

#define TASKINITPRIV_ERROR(message, errCode) \
    slurm_error(message); \
    rc = errCode; \
    goto _taskInitPriv_exit_unclean;

    if (nativeSlurm == 0) return ESPANK_SUCCESS;
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
    if (strlen(image) == 0 || strlen(image_type) == 0) {
        return rc;
    }
    if (ccmMode == 0 && autoshift == 0) return rc;
    udiConfig = read_config(argc, argv);
    if (udiConfig == NULL) {
        TASKINITPRIV_ERROR("Failed to load udiRoot config!", ESPANK_ERROR);
    }
    parse_ImageData(image_type, image, udiConfig, &imageData);

    if (spank_get_item(sp, S_JOB_UID, &job_uid) != ESPANK_SUCCESS) {
        TASKINITPRIV_ERROR("FAILED to get job uid!", ESPANK_ERROR);
    }

    if (strlen(udiConfig->udiMountPoint) > 0) {
        char currcwd[PATH_MAX];
        char newcwd[PATH_MAX];
        gid_t *gids = NULL;
        gid_t gid = 0;
        int ngids = 0;
        struct stat st_data;

        if (getcwd(currcwd, PATH_MAX) == NULL) {
            TASKINITPRIV_ERROR("FAILED to determine working directory", ESPANK_ERROR);
        }

        // check to see if newcwd exists, if not, just chdir to the new chroot base
        snprintf(newcwd, PATH_MAX, "%s", udiConfig->udiMountPoint);
        if (stat(newcwd, &st_data) != 0) {
            char error[PATH_MAX];
            snprintf(error, PATH_MAX, "FAILED to stat UDI directory: %s", newcwd);
            TASKINITPRIV_ERROR(error, ESPANK_ERROR);
        }
        if (chdir(newcwd) != 0) {
            char error[PATH_MAX];
            snprintf(error, PATH_MAX, "FAILED to change directory to %s", newcwd);
            TASKINITPRIV_ERROR(error, ESPANK_ERROR);
        }

        if (chroot(udiConfig->udiMountPoint) != 0) {
            TASKINITPRIV_ERROR("FAILED to chroot to designated image", ESPANK_ERROR);
        }

        if (spank_get_item(sp, S_JOB_SUPPLEMENTARY_GIDS, &gids, &ngids) != ESPANK_SUCCESS) {
            TASKINITPRIV_ERROR("FAILED to obtain group ids", ESPANK_ERROR);
        }

        if (spank_get_item(sp, S_JOB_GID, &gid) != ESPANK_SUCCESS) {
            TASKINITPRIV_ERROR("FAILED to obtain job group id", ESPANK_ERROR);
        }

        n_existing_suppl_gids = getgroups(0, NULL);
        if (n_existing_suppl_gids > 0) {
            existing_suppl_gids = (gid_t *) malloc(sizeof(gid_t) * n_existing_suppl_gids);
            if (existing_suppl_gids == NULL) {
                TASKINITPRIV_ERROR("FAILED to allocate memory to store current suppl gids", ESPANK_ERROR);
            }
            if (getgroups(n_existing_suppl_gids, existing_suppl_gids) < 0) {
                TASKINITPRIV_ERROR("FAILED to get current suppl gids", ESPANK_ERROR);
            }
        }

        if (setgroups(ngids, gids) != 0) {
            TASKINITPRIV_ERROR("FAILED to set supplmentary group ids", ESPANK_ERROR);
        }

        if (setegid(gid) != 0) {
            TASKINITPRIV_ERROR("FAILED to set job group id", ESPANK_ERROR);
        }

        /* briefly assume job uid prior to chdir into target path
           this is necessary in the case that we are chdir'ing into a privilege
           restricted path on a root-squashed filesystem */
        if (seteuid(job_uid) < 0) {
            char error[1024];
            snprintf(error, 1024, "FAILED to set effective uid to %d", job_uid);
            TASKINITPRIV_ERROR(error, ESPANK_ERROR);
        }

        if (chdir(currcwd) != 0) {
            char error[PATH_MAX];
            snprintf(error, PATH_MAX, "FAILED to change directory to: %s", currcwd);
            TASKINITPRIV_ERROR(error, ESPANK_ERROR);
        }

        /* go back to our original effective uid */
        if (seteuid(curr_uid) < 0) {
            char error[1024];
            snprintf(error, 1024, "FAILED to return effective uid to %d", curr_uid);
            TASKINITPRIV_ERROR(error, ESPANK_ERROR);
        }

        if (setegid(existing_gid) != 0) {
            TASKINITPRIV_ERROR("FAILED to return effective gid", ESPANK_ERROR);
        }
        if (setgroups(n_existing_suppl_gids, existing_suppl_gids) != 0) {
            TASKINITPRIV_ERROR("FAILED to drop supplementary gids", ESPANK_ERROR);
        }

        if (shifter_setupenv(&environ, &imageData, udiConfig) != 0) {
            TASKINITPRIV_ERROR("FAILED to setup shifter environment", ESPANK_ERROR);
        }
    }
_taskInitPriv_exit_unclean:
    free_ImageData(&imageData, 0);
    if (udiConfig != NULL)
        free_UdiRootConfig(udiConfig, 1);
    return rc;
}
