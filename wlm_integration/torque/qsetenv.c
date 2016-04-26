/******************************************************************************
 ** qsetenv - A program to add/replace an environment variable in a PBS job
 **
 ** Author: Douglas Jacobsen <dmjacobsen@lbl.gov>
 ** Date  : 2014/08/15
 ** 
 ** Copyright (C) 2014, The Regents of the University of California.
 ** All Rights Reserved
 **
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <pbs_error.h>
#include <pbs_ifl.h>

#define QSETENV_VERSION "1.0"

#define IN_VALUE 1
#define IN_QUOTE 4

void usage(int status) {
    printf("qsetenv [-s <server>] <jobid> <EnvVarName> <EnvVarValue>\n");
    printf("\n");
    printf("  server     : PBS/torque server to connect to\n");
    printf("  jobid      : jobnumber.servername of the job to change\n");
    printf("  EnvVarName : the environment variable name to set\n");
    printf("  EnvVarValue: the environment variable value to set\n");
    printf("\n");
    printf("OPTIONS\n");
    printf("  --help,-h     : show this help message\n");
    printf("  --version,-V  : show version number\n");
    exit(status);
}

char *job_setenv_varstr(struct batch_status *job, const char *var, const char *value) {
    char *ref_variables = NULL;
    struct attrl *attribute = NULL;
    char *tgt_variables = NULL;
    int tgt_buflen = 0;

    if (job == NULL || var == NULL) {
        return NULL;
    }

    /* examine attribute list to get current environment variables */
    attribute = job->attribs;
    while (attribute != NULL) {
        if (strcmp(attribute->name, ATTR_v) == 0) {
            ref_variables = strdup(attribute->value);
        }
        attribute = attribute->next;
    }

    /* create final variable string */
    tgt_buflen = strlen(var) + strlen(value) + 2;
    if (ref_variables != NULL) {
        tgt_buflen += strlen(ref_variables) + 1;
    }
    tgt_variables = (char *) malloc(sizeof(char) * tgt_buflen);
    if (ref_variables != NULL) {
        snprintf(tgt_variables, tgt_buflen, "%s,%s=%s", ref_variables, var, value);
    } else {
        snprintf(tgt_variables, tgt_buflen, "%s=%s", var, value);
    }

    if (ref_variables != NULL) {
        free(ref_variables);
        ref_variables = NULL;
    }
    return tgt_variables;
}

int main(int argc, char **argv) {
    char *server = NULL;
    char *jobid = NULL;
    char *var = NULL;
    char *value = NULL;
    int server_fd = 0;
    int ret = 0;
    int c = 0;
    struct batch_status *job = NULL;
    struct attrl *attribute = NULL;
    char *var_string = NULL;
    struct option prg_options[] = {
        {"help",    no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
    };

    for ( ; ; ) {
        int option_index = 0;
        c = getopt_long(argc, argv, "s:hV",
            prg_options, &option_index
        );
        if (c == -1) break;
        switch (c) {
            case 'h':
                usage(0);
                break;
            case 'V':
                printf("qsetenv version: %s; for torque version %s\n", QSETENV_VERSION, TORQUE_VERSION);
                exit(0);
                break;
            case 's':
                server = optarg;
                break;
        }
    }
    for (c = optind; c != argc; c++) {
        switch (c-optind) {
            case 0:
                jobid = argv[c];
                break;
            case 1:
                var = argv[c];
                break;
            case 2:
                value = argv[c];
                break;
            default:
                printf("Too many arguments!\n");
                usage(1);
                break;
        }
    }
    if (value == NULL) {
        printf("Too few arguments!\n");
        usage(1);
    }

    if (server == NULL) {
        server = pbs_get_server_list();
    }

    char *tok_server = server;
    char *tgt_server = NULL;
    while ((tgt_server = strtok(tok_server, ",")) != NULL) {
        tok_server = NULL;
        server_fd = pbs_connect(tgt_server);
        if (server_fd > 0) {
            break;
        }
    }
    if (server_fd <= 0) {
        fprintf(stderr, "Failed to connect to PBS server!\n");
        exit(1);
    }
    printf("Querying job %s\n", jobid);
    job = pbs_statjob(server_fd, jobid, NULL, 0);
    if (job != NULL) {
        printf("job name: %s\n", job->name);
        var_string = job_setenv_varstr(job, var, value);

        attribute = (struct attrl *) malloc(sizeof(struct attrl));
        memset(attribute, 0, sizeof(struct attrl));
        attribute->name = ATTR_v;
        attribute->value = var_string;
        attribute->next = NULL;

        ret = pbs_alterjob(server_fd, jobid, attribute, NULL);

        if (ret != 0) {
            printf("Got error: %s\n", pbs_strerror(pbs_errno));
        }

        free(attribute);
        attribute = NULL;
    }

    if (var_string != NULL) {
        free(var_string);
    }
    if (job != NULL) {
        pbs_statfree(job);
        job = NULL;
    }
    pbs_disconnect(server_fd);

    if (ret != 0) {
        return 1;
    }
    return 0;
}
