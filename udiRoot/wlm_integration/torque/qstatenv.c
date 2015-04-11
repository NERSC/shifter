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
    printf("qstatenv [-s <server>] <jobid>\n");
    printf("\n");
    printf("  server     : PBS/torque server to connect to\n");
    printf("  jobid      : jobnumber.servername of the job to change\n");
    printf("\n");
    printf("OPTIONS\n");
    printf("  --help,-h     : show this help message\n");
    printf("  --version,-V  : show version number\n");
    exit(status);
}

char *job_getenv_varstr(struct batch_status *job) {
    char *ref_variables = NULL;
    struct attrl *attribute = NULL;

    if (job == NULL) {
        return NULL;
    }

    /* examine attribute list to get current environment variables */
    attribute = job->attribs;
    while (attribute != NULL) {
        if (strcmp(attribute->name, ATTR_v) == 0) {
            ref_variables = strdup(attribute->value);
            return ref_variables;
        }
        attribute = attribute->next;
    }

    return NULL;
}

int main(int argc, char **argv) {
    char *server = NULL;
    char *jobid = NULL;
    int server_fd = 0;
    int ret = 0;
    int c = 0;
    struct batch_status *job = NULL;
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
                printf("qstatenv version: %s; for torque version %s\n", QSETENV_VERSION, TORQUE_VERSION);
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
            default:
                printf("Too many arguments!\n");
                usage(1);
                break;
        }
    }
    if (jobid == NULL) {
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
    job = pbs_statjob(server_fd, jobid, NULL, 0);
    if (job != NULL) {
        char *tok_string, *tgt_string;
        var_string = job_getenv_varstr(job);
        if (var_string != NULL) {
            tok_string = var_string;
            while ((tgt_string = strtok(tok_string, ",")) != NULL) {
                tok_string = NULL;
                if (strchr(tgt_string, '=')) {
                    printf("%s\n", tgt_string);
                }
            }
        } else {
            ret = 1;
        }
    } else {
        ret = 1;
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
