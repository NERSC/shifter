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

SPANK_PLUGIN(shifter, 1)

#ifndef IS_NATIVE_SLURM
#define IS_NATIVE_SLURM 0
#endif

#define IMAGE_MAXLEN 1024
#define IMAGEVOLUME_MAXLEN 2048
static char image[IMAGE_MAXLEN] = "";
static char image_type[IMAGE_MAXLEN] = "";
static char imagevolume[IMAGEVOLUME_MAXLEN] = "";
static char udiRoot_prefix[1024] = "";
static char chroot_path[1024] = "";
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

int generateSshKey() {
    struct stat st_data;

    char filename[1024];
    char buffer[4096];
    struct passwd pwd;
    struct passwd *ptr = NULL;
    int generateKey = 0;
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
        int rc = 0;
        snprintf(filename, 1024, "%s/.udiRoot", pwd.pw_dir);
        mkdir(filename, 0700); // intentionally ignoring errors for this
        snprintf(filename, 1024, "%s/.udiRoot/id_rsa.key", pwd.pw_dir);
        snprintf(cmd, 1024, "ssh-keygen -t rsa -f %s -N '' >/dev/null 2>/dev/null", filename);
        rc = system(cmd);
        return rc;
    }
    return 0;
}

int read_config_item(const char *filename, const char *item, char *buffer, size_t buflen) {
    char cmdBuffer[1024];
    ssize_t nread;
    size_t linePtrSize;
    char *linePtr = NULL;
    char *ptr = NULL;
    FILE *fp = NULL;
    int rc = 0;

    if (filename == NULL || strlen(filename) == 0) {
        return 1;
    }
    if (item == NULL || strlen(item) == 0) {
        return 1;
    }

    snprintf(cmdBuffer, 1024, "/bin/sh -c 'source %s; echo $%s'", filename, item);
    fp = popen(cmdBuffer, "r");
    while ((nread = getline(&linePtr, &linePtrSize, fp)) > 0) {
        linePtr[nread] = 0;
        ptr = trim(linePtr);
        snprintf(buffer, buflen, "%s", ptr);
    }
    if (linePtr != NULL) {
        free(linePtr);
        linePtr = NULL;
        linePtrSize = 0;
    }
    if ((rc = pclose(fp)) != 0) {
        slurm_error("Failed to read configuration file: %d", rc);
        return 1;
    }
    return strlen(buffer);;
}



int read_config(const char *filename) {
    struct stat st_data;

    chroot_path[0] = 0;
    udiRoot_prefix[0] = 0;

    memset(&st_data, 0, sizeof(struct stat));
    if (stat(filename, &st_data) != 0) {
        perror("Failed to stat config file: ");
        return 1;
    }
    if (st_data.st_uid != 0 || st_data.st_gid != 0 || (st_data.st_mode & S_IWOTH)) {
        fprintf(stderr, "Configuration file not owned by root, or is writable by others.\n");
        return 1;
    }
    if (read_config_item(filename, "udiMount", chroot_path, 1024) <= 0) {
        fprintf(stderr, "udiMount path invalid (len=0)\n");
        return 1;
    }
    if (read_config_item(filename, "udiRootPath", udiRoot_prefix, 1024) <= 0) {
        fprintf(stderr, "udiRootPath invalid (len=0)\n");
        return 1;
    } else {
        memset(&st_data, 0, sizeof(struct stat));
        if (stat(udiRoot_prefix, &st_data) != 0) {
            perror("Could not stat udiRoot prefix");
            return 1;
        }
        if (st_data.st_uid != 0 || st_data.st_gid != 0 || (st_data.st_mode & S_IWOTH)) {
            fprintf(stderr, "udiRoot installation not owned by root, or is globally writable.\n");
            return 1;
        }
    }
    return 0;
}

int lookup_image(int verbose, const char *mode, char **image_id) {
    int rc = 0;
    char buffer[4096];
    char buff1[1024];
    char buff2[1024];
    FILE *fp = NULL;
    ssize_t nread = 0;
    char *ptr = NULL;
    char *linePtr = NULL;
    size_t linePtrSize = 0;
    /* perform image lookup */
    snprintf(buffer, 4096, "%s/bin/getDockerImage.pl %s%s %s", udiRoot_prefix, (verbose == 0 ? "-quiet " : ""), mode, image);
    fp = popen(buffer, "r");
    while ((nread = getline(&linePtr, &linePtrSize, fp)) > 0) {
        linePtr[nread] = 0;
        ptr = trim(linePtr);
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
                ptr = trim(ptr);
                *image_id = (char *) realloc(*image_id, sizeof(char) * (strlen(ptr) + 1));
                snprintf(*image_id, strlen(ptr), "%s", ptr);
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

    char config_file[1024] = "";
    char *ptr = NULL;
    int idx = 0;

    context = spank_context();

    for (idx = 0; idx < argc; ++idx) {
        if (strncmp("shifter_config=", argv[idx], 15) == 0) {
            snprintf(config_file, 1024, "%s", (argv[idx] + 15));
            ptr = trim(config_file);
            if (ptr != config_file) memmove(config_file, ptr, strlen(ptr) + 1);
        }
    }
    if (strlen(config_file) == 0) {
        slurm_debug("shifter_config not set, cannot use shifter");
        return rc;
    }
    if (read_config(config_file) != 0) {
        slurm_error("Failed to parse shifter config. Cannot use shifter.");
        return rc;
    }

    image[0] = 0;
    imagevolume[0] = 0;

    context = spank_context();
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

    // only perform this validation at submit time
    context = spank_context();
    if (context != S_CTX_ALLOCATOR) {
        return ESPANK_SUCCESS;
    }
    verbose_lookup = 1;
    if (imagevolume != NULL && image == NULL) {
        slurm_error("Cannot specify shifter volumes without specifying the image first!");
        exit(-1);
    }
    
    if (strncmp(image_type, "docker", IMAGE_MAXLEN) == 0) {
        char *image_id = NULL;
        lookup_image(verbose_lookup, "pull", &image_id);
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
        generateSshKey();
    }
    
    spank_setenv(sp, "CRAY_ROOTFS", "UDI", 1);
    spank_setenv(sp, "SLURM_PROTEUS_IMAGE", image, 1);
    spank_setenv(sp, "SLURM_PROTEUS_IMAGETYPE", image_type, 1);
    spank_job_control_setenv(sp, "SLURM_PROTEUS_IMAGE", image, 1);
    spank_job_control_setenv(sp, "SLURM_PROTEUS_IMAGETYPE", image_type, 1);
    
    if (imagevolume != NULL) {
         spank_setenv(sp, "SLURM_PROTEUS_VOLUME", imagevolume, 1);
    }
    return rc;
}

int slurm_spank_job_prolog(spank_t sp, int argc, char **argv) {
    int rc = ESPANK_SUCCESS;

    char config_file[1024] = "";
    char *ptr = NULL;
    int idx = 0;
    int i,j;
    pid_t child = 0;
    unsigned int job;
    uid_t uid;
    gid_t gid;

    char job_str[128];
    char user_str[128];
    char group_str[128];
    char **volArgs = NULL;
    size_t n_volArgs = 0;
    char *nodelist = NULL;
    size_t tasksPerNode = 0;

    for (idx = 0; idx < argc; ++idx) {
        if (strncmp("shifter_config=", argv[idx], 15) == 0) {
            snprintf(config_file, 1024, "%s", (argv[idx] + 15));
            ptr = trim(config_file);
            if (ptr != config_file) memmove(config_file, ptr, strlen(ptr) + 1);
        }
    }
    if (strlen(config_file) == 0) {
        slurm_debug("shifter_config not set, cannot use shifter");
        return rc;
    }
    if (read_config(config_file) != 0) {
        slurm_error("Failed to parse shifter config. Cannot use shifter.");
        return rc;
    }

    for (i = 0; spank_option_array[i].name != NULL; ++i) {
        char *optarg = NULL;
        j = spank_option_getopt(sp, &spank_option_array[i], &optarg);
        if (j != ESPANK_SUCCESS) {
            continue;
        }
        (spank_option_array[i].cb)(spank_option_array[i].val, optarg, 1);
    }
    if (strlen(image) == 0 || strlen(image_type) == 0) {
        return rc;
    }

    for (ptr = image_type; ptr - image_type < strlen(image_type); ptr++) {
        *ptr = toupper(*ptr);
    }

    if (nativeSlurm) {
        uint32_t jobid;
        job_info_msg_t *job_buf;
        if (spank_get_item(sp, S_JOB_ID, &jobid) != ESPANK_SUCCESS) {
            slurm_error("Couldnt get job id");
            return rc;
        }
        if (slurm_load_job(&job_buf, jobid, SHOW_ALL) != 0) {
            slurm_error("Couldn't load job data");
            return rc;
        }
        if (job_buf->record_count != 1) {
            slurm_error("Can't deal with this job!");
            slurm_free_job_info_msg(job_buf);
            return rc;
        }
        nodelist = strdup(job_buf->job_array->nodes);
        tasksPerNode = job_buf->job_array->num_cpus / job_buf->job_array->num_nodes;
        slurm_free_job_info_msg(job_buf);
    }

    if (strlen(imagevolume) > 0) {
        char *ptr = imagevolume;
        for ( ; ; ) {
            char *limit = strchr(ptr, ',');
            volArgs = (char **) realloc(volArgs,sizeof(char *) * (n_volArgs+2));
            if (limit != NULL) *limit = 0;
            volArgs[n_volArgs++] = strdup(ptr);
            volArgs[n_volArgs] = NULL;
            if (limit == NULL) {
                break;
            }
            ptr = limit + 1;
        }
    }

    if (spank_get_item(sp, S_JOB_ID, &job) != ESPANK_SUCCESS) {
        slurm_error("FAIL: cannot deterime job userid");
        return rc;
    } else {
        snprintf(job_str, 128, "%u", job);
    }
    if (spank_get_item(sp, S_JOB_UID, &uid) != ESPANK_SUCCESS) {
        slurm_error("FAIL: cannot determine job userid");
        return rc;
    } else {
        struct passwd pwd;
        struct passwd *ptr = NULL;
        char buffer[4096];
        getpwuid_r(uid, &pwd, buffer, 4096, &ptr);
        if (ptr != NULL) {
            snprintf(user_str, 128, "%s", pwd.pw_name);
        } else {
            slurm_error("FAIL cannot lookup username");
            return rc;
        }
    }
    if (spank_get_item(sp, S_JOB_GID, &gid) != ESPANK_SUCCESS) {
        slurm_error("FAIL: cannot determine job group, gonna fake it");
        snprintf(group_str, 128, "%s", user_str); //hack workaround because spank_get_item S_JOB_GID fails
    } else {
        struct group grp;
        struct group *ptr;
        char buffer[4096];
        getgrgid_r(gid, &grp, buffer, 4096, &ptr);
        if (ptr != NULL) {
            snprintf(group_str, 128, "%s", grp.gr_name);
        } else {
            slurm_error("FAIL cannot lookup group name");
            return rc;
        }
    }
    child = fork();
    if (child == 0) {
        char buffer[PATH_MAX];
        char ntaskBuf[128];
        char *args[6 + n_volArgs*2 + 3 + 4];
        char **argPtr = args;
        size_t idx = 0;
        size_t idx2 = 0;
        clearenv();
        snprintf(buffer, PATH_MAX, "%s/libexec/udiRoot-prologue", udiRoot_prefix);
        memset(args, 0, sizeof(char*) * (6 + n_volArgs*2 + 3));
        args[idx++] = buffer;
        args[idx++] = job_str;
        args[idx++] = user_str;
        args[idx++] = image_type;
        args[idx++] = image;
        for (idx2 = 0; idx2 < n_volArgs; idx2++) {
            args[idx++] = "-v";
            args[idx++] = volArgs[idx2];
        }
        if (nativeSlurm != 0) {
            args[idx++] = "-m";
            args[idx++] = "local";
        }
        if (nodelist != NULL) {
            snprintf(ntaskBuf, 128, "%lu", tasksPerNode);
            args[idx++] = "-n";
            args[idx++] = nodelist;
            args[idx++] = "-N";
            args[idx++] = ntaskBuf;
        }
        args[idx++] = NULL;
        /* null termination set by memset above */
        while (*argPtr != NULL) {
            slurm_info("prolog arg: %s", *argPtr++);
        }
        execv(args[0], args);
    } else if (child > 0) {
        int status = 0;
        waitpid(child, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            /* chroot area should be ready! */
        } else {
            slurm_error("shifter: failed to setup image");
            rc = ESPANK_ERROR;
        }
    } else {
        slurm_error("shifter: failed to fork setupRoot");
        rc = ESPANK_ERROR;
    }
    if (nodelist != NULL) free(nodelist);
    
    return rc;
} 

int slurm_spank_job_epilog(spank_t sp, int argc, char **argv) {
    int rc = ESPANK_SUCCESS;

    char config_file[1024] = "";
    char *ptr = NULL;
    int idx = 0;
    int i,j;
    pid_t child = 0;
    unsigned int job;
    uid_t uid;
    gid_t gid;

    char job_str[128];
    char user_str[128];
    char group_str[128];
    for (idx = 0; idx < argc; ++idx) {
        if (strncmp("shifter_config=", argv[idx], 15) == 0) {
            snprintf(config_file, 1024, "%s", (argv[idx] + 15));
            ptr = trim(config_file);
            if (ptr != config_file) memmove(config_file, ptr, strlen(ptr) + 1);
        }
    }
    if (strlen(config_file) == 0) {
        slurm_debug("shifter_config not set, cannot use shifter");
        return rc;
    }
    if (read_config(config_file) != 0) {
        slurm_error("Failed to parse shifter config. Cannot use shifter.");
        return rc;
    }
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

    if (spank_get_item(sp, S_JOB_ID, &job) != ESPANK_SUCCESS) {
        slurm_error("FAIL: cannot deterime job userid");
        return rc;
    } else {
        snprintf(job_str, 128, "%u", job);
    }
    if (spank_get_item(sp, S_JOB_UID, &uid) != ESPANK_SUCCESS) {
        slurm_error("FAIL: cannot determine job userid");
        return rc;
    } else {
        struct passwd pwd;
        struct passwd *ptr = NULL;
        char buffer[4096];
        getpwuid_r(uid, &pwd, buffer, 4096, &ptr);
        if (ptr != NULL) {
            snprintf(user_str, 128, "%s", pwd.pw_name);
        } else {
            slurm_error("FAIL cannot lookup username");
            return rc;
        }
    }
    if (spank_get_item(sp, S_JOB_GID, &gid) != ESPANK_SUCCESS) {
        slurm_error("FAIL: cannot determine job group, gonna fake it");
        snprintf(group_str, 128, "%s", user_str);
    } else {
        struct group grp;
        struct group *ptr;
        char buffer[4096];
        getgrgid_r(gid, &grp, buffer, 4096, &ptr);
        if (ptr != NULL) {
            snprintf(group_str, 128, "%s", grp.gr_name);
        } else {
            slurm_error("FAIL cannot lookup group name");
            return rc;
        }
    }
    child = fork();
    if (child == 0) {
        char buffer[PATH_MAX];
        char *args[9];
        size_t idx = 0;
        clearenv();
        snprintf(buffer, PATH_MAX, "%s/libexec/udiRoot-epilogue", udiRoot_prefix);
        args[idx++] = buffer;
        args[idx++] = job_str;
        args[idx++] = user_str;
        if (nativeSlurm != 0) {
            args[idx++] = "-m";
            args[idx++] = "local";
        }
        args[idx++] = NULL;
        execv(args[0], args);
    } else if (child > 0) {
        int status = 0;
        waitpid(child, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            /* chroot area should be ready! */
        } else {
            slurm_error("shifter: failed to deconstruct image");
            rc = ESPANK_ERROR;
        }
    } else {
        slurm_error("shifter: failed to fork epilogue script");
        rc = ESPANK_ERROR;
    }
    
    return rc;
}

int slurm_spank_task_init_privileged(spank_t sp, int argc, char **argv) {
    int rc = ESPANK_SUCCESS;

    if (nativeSlurm == 0) return ESPANK_SUCCESS;

    if (strlen(image) == 0 || strlen(image_type) == 0) {
        return rc;
    }

    if (strlen(chroot_path) > 0) {
        char currcwd[PATH_MAX];
        char newcwd[PATH_MAX];
        struct stat st_data;
        if (getcwd(currcwd, PATH_MAX) == NULL) {
            slurm_error("FAILED to determine working directory");
            return ESPANK_ERROR;
        }

        // check to see if newcwd exists, if not, just chdir to the new chroot base
        snprintf(newcwd, PATH_MAX, "%s/%s", chroot_path, currcwd);
        if (stat(newcwd, &st_data) != 0) {
            snprintf(newcwd, PATH_MAX, "%s", chroot_path);
        }
        if (chdir(newcwd) != 0) {
            slurm_error("FAILED to change directory to %s", newcwd);
            return ESPANK_ERROR;
        }

        if (chroot(chroot_path) != 0) {
            slurm_error("FAILED to chroot to designated image");
            return ESPANK_ERROR;
        }
    }
    return rc;
}
