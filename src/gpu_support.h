/**
 * @author Kean Mariotti <kean.mariotti@cscs.ch>
 */
#ifndef _SHIFTER_GPU_SUPPORT_H
#define _SHIFTER_GPU_SUPPORT_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Pupulate the GPU configuration structure according to the value
 * of the environment variable CUDA_VISIBLE_DEVICES.
 */
int parse_gpu_env(struct gpu_support_config* config);

/**
 * Executes the external bash script responsible for exposing
 * to the container the GPU-support related site resources.
 */
int execute_hook_to_activate_gpu_support(int verbose, const UdiRootConfig* udiConfig);

/**
 * Prints the GPU configuration to the passed file and returns the number of bytes written.
 */
int fprint_gpu_support_config(FILE* fp, const struct gpu_support_config* config);

/**
 * Free all resources owned by the specified GPU support configuration structure.
 */
void free_gpu_support_config(struct gpu_support_config* config);

#ifdef __cplusplus
}
#endif

#endif
