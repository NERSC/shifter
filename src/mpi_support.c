/**
 * @author Kean Mariotti <kean.mariotti@cscs.ch>
 */
#include "mpi_support.h"
#include "utility.h"
#include "shifter_core.h"

int execute_hook_to_activate_mpi_support(int verbose, const UdiRootConfig* udiConfig, const struct mpi_support_config* mpi_config)
{
    int ret = 0;

    if(mpi_config->is_mpi_support_enabled)
    {
        char* args[6];
        args[0] = strdup("/bin/bash");
        args[1] = alloc_strgenf("%s/bin/activate_mpi_support.sh", udiConfig->udiRootPath);
        args[2] = strdup(udiConfig->udiMountPoint);
        args[3] = strdup(udiConfig->siteResources);
        args[4] = verbose ? strdup("verbose-on") : strdup("verbose-off");
        args[5] = NULL;

        ret = forkAndExecv(args);
        char** p;
        for (p=args; *p != NULL; p++)
        {
            free(*p);
        }
    }

    return ret;
}
