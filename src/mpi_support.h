/**
 * @author Kean Mariotti <kean.mariotti@cscs.ch>
 */
#ifndef _SHIFTER_MPI_SUPPORT_H
#define _SHIFTER_MPI_SUPPORT_H

#include "UdiRootConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mpi_support_config {
    int is_mpi_support_enabled;
};

/**
 * Executes the external bash script responsible for exposing
 * to the container the MPI-support related site resources.
 */
int execute_hook_to_activate_mpi_support(int verbose, const UdiRootConfig* udiConfig, const struct mpi_support_config* mpi_config);

#ifdef __cplusplus
}
#endif

#endif
