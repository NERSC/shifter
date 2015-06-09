/**************************************************
** udiRootEnter
**    setuid utility to chroot into udiRoot
** Author: Douglas Jacobsen <dmjacobsen@lbl.gov>
**************************************************/

#define _GNU_SOURCE
#include <unistd.h>
#include <sched.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <grp.h>

extern char **environ;

const char *setupRootRelativePath = "sbin/setupRoot.sh";

struct options {
    char *chroot_path;
    char *udiRoot_prefix;
    int udiRoot_flag;
    char *udiRoot_type;
    char *udiRoot_value;
    uid_t tgtUid;
    gid_t tgtGid;
    char **args;
};

void usage(int status) {
}

void version() {
    printf("udiRootEnter version %s\n", VERSION);
}

char **copyenv() {
    char **outenv = NULL;
    char **ptr = NULL;
    char **wptr = NULL;

    if (environ == NULL) {
        return NULL;
    }

    for (ptr = environ; *ptr != NULL; ++ptr) {
    }
    outenv = (char **) malloc(sizeof(char*) * ((ptr - environ) + 1));
    for (ptr = environ, wptr = outenv; *ptr != NULL; ++ptr, ++wptr) {
        *wptr = strdup(*ptr);
    }
    *wptr = NULL;
    return outenv;
}

char *trim(char *str) {
    char *end = NULL;
    if (str == NULL) {
        return str;
    }
    while (isspace(*str)) {
        str++;
    }
    if (*str == 0) {
        return str;
    }
    end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) {
        end--;
    }
    *(end+1) = 0;
    return str;
}

int read_config(int argc, char **argv, struct options *opts) {
    const char *cn_filename = CONFIG_FILE;
    const char *filename = cn_filename;
    FILE *fp = NULL;
    struct stat st_data;
    size_t nRead = 0;
    char buffer[PATH_MAX];
    char *ptr = NULL;
    int idx, aidx;
    int computeContext = 0;

    if (opts == NULL) {
        fprintf(stderr, "Invalid configuration structure, abort.\n");
        exit(1);
    }
    opts->udiRoot_flag = 0;

    /* parse very simple command line options */
    for (idx = 1; idx < argc; ++idx) {
        if (strcmp(argv[idx], "--setup") == 0) {
            /* expect two more arguments */
            if (idx + 2 >= argc) {
                fprintf(stderr, "Invalid options for --setup\n");
                usage(1);
                exit(1);
            }
            opts->udiRoot_flag = 1;
            opts->udiRoot_type = strdup(argv[++idx]);
            opts->udiRoot_value = strdup(argv[++idx]);
        } else if (strcmp(argv[idx], "--help") == 0 || strcmp(argv[idx], "-h") == 0) {
            usage(0);
            exit(0);
        } else if (strcmp(argv[idx], "--version") == 0 || strcmp(argv[idx], "-V") == 0) {
            version();
            exit(0);
        } else if (strcmp(argv[idx], "--uid") == 0 || strcmp(argv[idx], "-u") == 0) {
            idx++;
            opts->tgtUid = 0;
            if (idx < argc) {
                opts->tgtUid = atoi(argv[idx]);
            }
            if (opts->tgtUid == 0) {
                fprintf(stderr, "Could not interpret uid (cannot be 0)\n");
                usage(1);
                exit(1);
            }
        } else if (strcmp(argv[idx], "--gid") == 0 || strcmp(argv[idx], "-g") == 0) {
            idx++;
            opts->tgtGid = 0;
            if (idx < argc) {
                opts->tgtGid = atoi(argv[idx]);
            }
            if (opts->tgtGid == 0) {
                fprintf(stderr, "Could not interpret gid (cannot be 0)\n");
                usage(1);
                exit(1);
            }
        } else if (strcmp(argv[idx], "--compute") == 0) {
            computeContext = 1;
        } else {
            break;
        }
    }

    if (computeContext) {
        snprintf(buffer, PATH_MAX, "%s%s", "/dsl", cn_filename);
        filename = strdup(buffer);
        setenv("context", "/dsl", 1);
    }

    memset(&st_data, 0, sizeof(struct stat));
    if (stat(filename, &st_data) != 0) {
        perror("Failed to stat config file: ");
        exit(1);
    }
    if (st_data.st_uid != 0 || st_data.st_gid != 0 || (st_data.st_mode & S_IWOTH)) {
        fprintf(stderr, "Configuration file not owned by root, or is writable by others.\n");
        exit(1);
    }
    snprintf(buffer, 1024, "/bin/sh -c 'source %s; echo $%s'", filename, "udiMount");
    fp = popen(buffer, "r");
    nRead = fread(buffer, 1, 1024, fp);
    pclose(fp);
    if (nRead == 0) {
        fprintf(stderr, "Failed to read configuration file.\n");
        exit(1);
    }
    buffer[nRead] = 0;
    ptr = trim(buffer);
    opts->chroot_path = strdup(ptr);
    snprintf(buffer, 1024, "/bin/sh -c 'source %s; echo $%s'", filename, "udiRootPath");
    fp = popen(buffer, "r");
    nRead = fread(buffer, 1, 1024, fp);
    pclose(fp);
    if (nRead == 0) {
        fprintf(stderr, "Failed to read configuration file.\n");
        exit(1);
    }
    buffer[nRead] = 0;
    ptr = trim(buffer);
    opts->udiRoot_prefix = strdup(ptr);

    if (opts->chroot_path != NULL && strlen(opts->chroot_path) != 0) {
        memset(&st_data, 0, sizeof(struct stat));
        if (stat(opts->chroot_path, &st_data) != 0) {
            perror("Could not stat target root path: ");
            exit(1);
        }
        if (st_data.st_uid != 0 || st_data.st_gid != 0 || (st_data.st_mode & S_IWOTH)) {
            fprintf(stderr, "%s\n", "Target / path is not owned by root, or is globally writable.");
            exit(1);
        }
    } else {
        return 1;
    }
    if (opts->udiRoot_prefix != NULL && strlen(opts->udiRoot_prefix) != 0) {
        memset(&st_data, 0, sizeof(struct stat));
        if (stat(opts->udiRoot_prefix, &st_data) != 0) {
            perror("Could not stat udiRoot prefix");
            exit(1);
        }
        if (st_data.st_uid != 0 || st_data.st_gid != 0 || (st_data.st_mode & S_IWOTH)) {
            fprintf(stderr, "udiRoot installation not owned by root, or is globally writable.\n");
            exit(1);
        }
    } else {
        return 1;
    }

    /* remainder of arguments are for the application to be executed */
    opts->args = (char **) malloc(sizeof(char *) * (argc + 1));
    for (aidx = 0; idx < argc; ++idx) {
        opts->args[aidx++] = strdup(argv[idx]);
    }
    opts->args[aidx] = NULL;

    return 0;
}

int main(int argc, char **argv) {

    /* save a copy of the environment for the exec */
    char **environ_copy = copyenv();
    char **arg;

    /* declare needed variables */
    const size_t pathbuf_sz = PATH_MAX+1;
    char wd[pathbuf_sz];
    uid_t uid,eUid;
    gid_t gid,eGid;
    gid_t *gidList = NULL;
    int nGroups = 0;
    int idx = 0;
    struct options opts;
    memset(&opts, 0, sizeof(struct options));


    /* destroy this environment */
    clearenv();

    /* parse config file and command line options */
    read_config(argc, argv, &opts);


    /* figure out who we are and who we want to be */
    uid = getuid();
    eUid = geteuid();
    gid = getgid();
    eGid = getegid();

    if (opts.tgtUid == 0) opts.tgtUid = uid;
    if (opts.tgtGid == 0) opts.tgtGid = gid;

    nGroups = getgroups(0, NULL);
    if (nGroups > 0) {
        gidList = (gid_t *) malloc(sizeof(gid_t) * nGroups);
        if (gidList == NULL) {
            fprintf(stderr, "Failed to allocate memory for group list\n");
            exit(1);
        }
        if (getgroups(nGroups, gidList) == -1) {
            fprintf(stderr, "Failed to get supplementary group list\n");
            exit(1);
        }
        for (idx = 0; idx < nGroups; ++idx) {
            if (gidList[idx] == 0) {
                gidList[idx] = opts.tgtGid;
            }
        }
        for (idx = 0; idx < nGroups; ++idx) {
            if (gidList[idx] == 0) {
                gidList[idx] = opts.tgtGid;
            }
        }
    }

    if (eUid != 0 && eGid != 0) {
        fprintf(stderr, "%s\n", "Not running with root privileges, will fail.");
        exit(1);
    }
    if (opts.tgtUid == 0 || opts.tgtGid == 0) {
        fprintf(stderr, "%s\n", "Will not run as root.");
        exit(1);
    }

    if (opts.udiRoot_flag == 1) {
        /* call setup root */
        int status;
        pid_t child;
        struct stat st_data;
        size_t buflen = strlen(opts.udiRoot_prefix) + strlen(setupRootRelativePath) + 2;
        char *exeBuffer = (char *) malloc(sizeof(char) * buflen);
        if (exeBuffer == NULL) {
            fprintf(stderr, "Failed to allocate memory.\n");
            exit(1);
        }
        snprintf(exeBuffer, buflen, "%s/%s", opts.udiRoot_prefix, setupRootRelativePath);
        memset(&st_data, 0, sizeof(struct stat));
        if (stat(exeBuffer, &st_data) != 0) {
            perror("Failed to stat setupRoot.sh");
            exit(1);
        }

        /* unshare filesystem namespace */
        if (unshare(CLONE_NEWNS) != 0) {
            perror("Failed to unshare the filesystem namespace.");
            exit(1);
        }

        child = fork();
        if (child == 0) {
            char *args[4];
            args[0] = exeBuffer;
            args[1] = opts.udiRoot_type;
            args[2] = opts.udiRoot_value;
            args[3] = NULL;
            execv(args[0], args);
            exit(1); /*should never get here */
        } else if (child < 0) {
            fprintf(stderr, "Failed to call setupRoot.sh\n");
            exit(1);
        }
        waitpid(child, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            /* chroot area should be ready! */
        } else {
            fprintf(stderr, "Failed to run setupRoot.sh properly\n");
            exit(1);
        }
    }

    /* keep cwd to switch back to it (if possible), after chroot */
    if (getcwd(wd, pathbuf_sz) == NULL) {
        perror("Failed to determine current working directory: ");
        exit(1);
    }

    /* switch to / to prevent the chroot jail from being leaky */
    if (chdir("/") != 0) {
        perror("Failed to switch to root path: ");
        exit(1);
    }

    /* chroot into the jail */
    if (chroot(opts.chroot_path) != 0) {
        perror("Could not chroot: ");
        exit(1);
    }

    /* drop privileges */
    if (setgroups(nGroups, gidList) != 0) {
        fprintf(stderr, "Failed to setgroups\n");
        exit(1);
    }
    if (setresgid(opts.tgtGid, opts.tgtGid, opts.tgtGid) != 0) {
        fprintf(stderr, "Failed to setgid to %d\n", opts.tgtGid);
        exit(1);
    }
    if (setresuid(opts.tgtUid, opts.tgtUid, opts.tgtUid) != 0) {
        fprintf(stderr, "Failed to setuid to %d\n", opts.tgtUid);
        exit(1);
    }

    /* chdir (within chroot) to where we belong again */
    if (chdir(wd) != 0) {
        fprintf(stderr, "Failed to switch to original cwd: %s\n", wd);
        exit(1);
    }

    arg = opts.args;
    while (*arg != NULL) {
        printf("arg: %s\n", *arg);
        arg++;
    }
    execve(opts.args[0], opts.args, environ_copy);
    return 0;
}
