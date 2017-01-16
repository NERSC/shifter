/**
 * @author Kean Mariotti <kean.mariotti@cscs.ch>
 */
#include "gpu_support.h"
#include "utility.h"
#include "shifter_core.h"
#include <stdlib.h>
#include <string.h>

int parse_gpu_env(struct gpu_support_config* config) {
    char *cuda_visible_devices = getenv("CUDA_VISIBLE_DEVICES");

    if( cuda_visible_devices != NULL
        && strcmp(cuda_visible_devices, "") != 0
        && strcmp(cuda_visible_devices, "NoDevFiles") != 0) {
        config->gpu_ids = strdup(cuda_visible_devices);
        config->is_gpu_support_enabled = 1;
    }
    else {
        config->gpu_ids = NULL;
        config->is_gpu_support_enabled = 0;
    }
    return 0;
}

int execute_hook_to_activate_gpu_support(int verbose, const UdiRootConfig* udiConfig) {
    int ret = 0;

    if(udiConfig->gpu_config.is_gpu_support_enabled) {
        char* script_path = alloc_strgenf("%s/bin/activate_gpu_support.sh", udiConfig->udiRootPath);

        char* args[8];
        args[0] = strdup("/bin/bash");
        args[1] = script_path;
        args[2] = strdup(udiConfig->gpu_config.gpu_ids);
        args[3] = strdup(udiConfig->udiMountPoint);
        args[4] = strdup(udiConfig->siteResources);
        args[5] = verbose ? strdup("verbose-on") : strdup("verbose-off");
        args[6] = NULL;

        ret = forkAndExecv(args);

        char** p;
        for (p=args; *p != NULL; p++) {
            free(*p);
        }
    }

    return ret;
}

int fprint_gpu_support_config(FILE* fp, const struct gpu_support_config* config)
{
    size_t written = 0;
    written += fprintf(fp, "***** GPU support config *****\n");
    written += fprintf(fp, "gpu_ids = %s\n", config->gpu_ids);
    written += fprintf(fp, "is_gpu_support_enabled = %d\n", config->is_gpu_support_enabled);
    return written;
}

void free_gpu_support_config(struct gpu_support_config* config) {
    free(config->gpu_ids);
    config->gpu_ids = NULL;
}
