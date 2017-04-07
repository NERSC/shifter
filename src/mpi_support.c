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
        if (udiConfig->siteMPISharedLibs == NULL || strlen(udiConfig->siteMPISharedLibs) == 0) {
            fprintf(stderr, "Native MPI support requested but no site-specific MPI libraries defined\n");
            return 1;
        }

        char* args[9];
        args[0] = strdup("/bin/bash");
        args[1] = alloc_strgenf("%s/bin/activate_mpi_support.sh", udiConfig->udiRootPath);
        args[2] = strdup(udiConfig->udiMountPoint);
        args[3] = strdup(udiConfig->siteResources);
        args[4] = strdup(udiConfig->siteMPISharedLibs);
        args[5] = make_non_empty_argument(udiConfig->siteMPIDependencyLibs);
        args[6] = make_non_empty_argument(udiConfig->siteMPIConfigurationFiles);
        args[7] = verbose ? strdup("verbose-on") : strdup("verbose-off");
        args[8] = NULL;

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
    return written;
}

void free_mpi_support_config(struct mpi_support_config* config) {
}

