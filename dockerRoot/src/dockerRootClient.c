/**************************************************
** dockerRootClient
**    setuid utility to chroot into dockerRoot
** Author: Douglas Jacobsen <dmjacobsen@lbl.gov>
**************************************************/

#define _GNU_SOURCE
#include <unistd.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <grp.h>

extern char **environ;

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

int read_config(char *chroot_path, size_t bufsize) {
    const char *filename = CONFIG_FILE;
    FILE *fp = fopen(filename, "r");
    int fd = fileno(fp);
    struct stat st_data;
    size_t nBytes = 0;
    size_t nRead = 0;
    char *buffer = NULL;
    char *tokptr = NULL;
    char *ptr = NULL;

    memset(&st_data, 0, sizeof(struct stat));
    if (fstat(fd, &st_data) != 0) {
        perror("Failed to stat config file: ");
        exit(1);
    }
    nBytes = st_data.st_size;
    if (st_data.st_uid != 0 || st_data.st_gid != 0 || (st_data.st_mode & S_IWOTH)) {
        fprintf(stderr, "%s\n", "Configuration file not owned by root, or is writable by others.");
        exit(1);
    }
    buffer = (char *) malloc(sizeof(char)*nBytes);
    nRead = fread(buffer, 1, nBytes, fp);
    if (nRead == 0) {
        fprintf(stderr, "%s\n", "Failed to read configuration file.");
        exit(1);
    }
    tokptr = buffer;
    while ((ptr = strtok(tokptr, "=\n")) != NULL) {
        tokptr = NULL;
        while (isspace(*ptr)) {
            ptr++;
        }
        if (*ptr == 0) {
            continue;
        }
        ptr = trim(ptr);
        if (strcmp(ptr, "dockerRoot") == 0) {
            ptr = strtok(NULL, "=\n");
            ptr = trim(ptr);
            if (ptr != NULL) {
                snprintf(chroot_path, bufsize, "%s", ptr);
                break;
            }
        }
    }
    fclose(fp);

    if (strlen(chroot_path) != 0) {
        memset(&st_data, 0, sizeof(struct stat));
        if (stat(chroot_path, &st_data) != 0) {
            perror("Could not stat target root path: ");
            exit(1);
        }
        if (st_data.st_uid != 0 || st_data.st_gid || (st_data.st_mode & S_IWOTH)) {
            fprintf(stderr, "%s\n", "Target / path is not owned by root, or is globally writable.");
            exit(1);
        }
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {

    /* save a copy of the environment for the exec */
    char **environ_copy = copyenv();

    /* declare needed variables */
    char **args = NULL;
    const size_t pathbuf_sz = PATH_MAX+1;
    char wd[pathbuf_sz];
    char chroot_path[pathbuf_sz];
    uid_t tgtUid, eUid;
    gid_t tgtGid, eGid;
    gid_t *gidList = NULL;
    int nGroups = 0;
    int idx = 0;


    /* destroy this environment */
    clearenv();

    /* figure out who we are and who we want to be */
    tgtUid = getuid();
    eUid   = geteuid();
    tgtGid = getgid();
    eGid   = getegid();
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
            printf("gidList entry: %d\n", gidList[idx]);
            if (gidList[idx] == 0) {
                gidList[idx] = tgtGid;
            }
        }
        for (idx = 0; idx < nGroups; ++idx) {
            printf("revised gidList entry: %d\n", gidList[idx]);
            if (gidList[idx] == 0) {
                gidList[idx] = tgtGid;
            }
        }
    }

    if (eUid != 0 || eGid != 0) {
        fprintf(stderr, "%s\n", "Not running with root privileges, will fail.");
        exit(1);
    }
    if (tgtUid == 0 || tgtGid == 0) {
        fprintf(stderr, "%s\n", "Will not run as root.");
        exit(1);
    }

    /* get the chroot path from the config file */
    read_config(chroot_path, pathbuf_sz);

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
    if (chroot(chroot_path) != 0) {
        perror("Could not chroot: ");
        exit(1);
    }

    /* drop privileges */
    if (setgroups(nGroups, gidList) != 0) {
        fprintf(stderr, "Failed to setgroups\n");
        exit(1);
    }
    if (setresgid(tgtGid, tgtGid, tgtGid) != 0) {
        fprintf(stderr, "Failed to setgid to %d\n", tgtGid);
        exit(1);
    }
    if (setresuid(tgtUid, tgtUid, tgtUid) != 0) {
        fprintf(stderr, "Failed to setuid to %d\n", tgtUid);
        exit(1);
    }

    /* chdir (within chroot) to where we belong again */
    if (chdir(wd) != 0) {
        fprintf(stderr, "Failed to switch to original cwd: %s\n", wd);
        exit(1);
    }

    /* process arguments and exec */
    args = (char **) malloc(sizeof(char*) * (argc+1));
    if (args == NULL) {
        fprintf(stderr, "Failed to allocate memory for args\n");
        exit(1);
    }
    for (idx = 1; idx < argc; ++idx) {
        args[idx] = strdup(argv[idx]);
    }
    args[idx] = NULL;
    execve(args[0], args, environ_copy);
    return 0;
}
