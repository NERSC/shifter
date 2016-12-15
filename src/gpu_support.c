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

int execute_hook_to_activate_gpu_support(const struct gpu_support_config* gpu_config, UdiRootConfig* udiConfig) {
    int ret = 0;

    if(gpu_config->is_gpu_support_enabled) {
        char* script_path = alloc_strgenf("%s/bin/activate_gpu_support.sh", udiConfig->udiRootPath);

        char* args[8];
        args[0] = strdup("/bin/bash");
        args[1] = script_path;
        args[2] = strdup(gpu_config->gpu_ids);
        args[3] = strdup(udiConfig->udiMountPoint);
        args[4] = strdup(udiConfig->siteResources);
        args[5] = NULL;

        ret = forkAndExecv(args);

        char** p;
        for (p=args; *p != NULL; p++) {
            free(*p);
        }
    }

    return ret;
}

void free_gpu_support_config(struct gpu_support_config* config) {
    free(config->gpu_ids);
}
