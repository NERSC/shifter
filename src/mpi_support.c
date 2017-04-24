/**
 * @author Kean Mariotti <kean.mariotti@cscs.ch>
 */
#include "mpi_support.h"
#include "utility.h"
#include "shifter_core.h"

static char* make_non_empty_argument(char* arg) {
    if(arg==NULL || strcmp(arg, "")==0) {
        return strdup(";");
    }
    else {
        return arg;
    }
}

int execute_hook_to_activate_mpi_support(int verbose, const UdiRootConfig* udiConfig) {
    int ret = 0;

    if(udiConfig->mpi_config.is_mpi_support_enabled) {
        if(udiConfig->siteResources == NULL) {
            fprintf(stderr, "FAILED to activate MPI support."
                            " The configuration of the site resources folder is missing.\n");
            return 1;
        }

        if (udiConfig->mpi_config.mpi_shared_libs == NULL
            || strlen(udiConfig->mpi_config.mpi_shared_libs) == 0) {
            fprintf(stderr, "Native MPI support requested but no site-specific MPI libraries "
                            "defined in udiRoot configuration file\n");
            return 1;
        }

        char* args[9];
        args[0] = strdup("/bin/bash");
        args[1] = alloc_strgenf("%s/bin/activate_mpi_support.sh", udiConfig->udiRootPath);
        args[2] = strdup(udiConfig->udiMountPoint);
        args[3] = strdup(udiConfig->siteResources);
        args[4] = strdup(udiConfig->mpi_config.mpi_shared_libs);
        args[5] = make_non_empty_argument(udiConfig->mpi_config.mpi_dependency_libs);
        args[6] = verbose ? strdup("verbose-on") : strdup("verbose-off");
        args[7] = NULL;

        /* Call the script to activate MPI support, then free argument pointers */
        ret = forkAndExecv(args);
        char** p;
        for (p=args; *p != NULL; p++) {
            free(*p);
        }
    }

    return ret;
}

int fprint_mpi_support_config(FILE* fp, const struct mpi_support_config* config) {
    size_t written = 0;
    written += fprintf(fp, "***** MPI support config *****\n");
    written += fprintf(fp, "is_mpi_support_enabled = %d\n", config->is_mpi_support_enabled);
    written += fprintf(fp, "siteMPISharedLibs = %s\n",
        (config->mpi_shared_libs != NULL ? config->mpi_shared_libs : ""));
    written += fprintf(fp, "siteMPIDependencyLibs = %s\n",
        (config->mpi_dependency_libs != NULL ? config->mpi_dependency_libs : ""));
    return written;
}

void free_mpi_support_config(struct mpi_support_config* config) {
    if(config->mpi_shared_libs != NULL) {
        free(config->mpi_shared_libs);
        config->mpi_shared_libs = NULL;
    }
    if(config->mpi_dependency_libs != NULL) {
        free(config->mpi_dependency_libs);
        config->mpi_dependency_libs = NULL;
    }
}

