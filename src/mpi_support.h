/**
 * @author Kean Mariotti <kean.mariotti@cscs.ch>
 */
#ifndef _SHIFTER_MPI_SUPPORT_H
#define _SHIFTER_MPI_SUPPORT_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _UdiRootConfig UdiRootConfig;

struct mpi_support_config {
    int is_mpi_support_enabled;
};

/**
 * Executes the external bash script responsible for exposing
 * to the container the MPI-support related site resources.
 */
int execute_hook_to_activate_mpi_support(int verbose, const UdiRootConfig* udiConfig);

/**
 * Prints the MPI configuration to the passed file and returns the number of bytes written.
 */
int fprint_mpi_support_config(FILE* fp, const struct mpi_support_config* config);

/**
 * Free all resources owned by the specified MPI support configuration structure.
 */
void free_mpi_support_config(struct mpi_support_config* config);

#ifdef __cplusplus
}
#endif

#endif

