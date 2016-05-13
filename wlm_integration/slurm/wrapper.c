#include <slurm/spank.h>

SPANK_PLUGIN(shifter, 1)

/* global variable used by spank to get plugin options */
struct spank_option spank_option_array[] = {
    { "image", "image", "shifter image to use", 1, 0,
      (spank_opt_cb_f) _opt_image},
    { "volume", "volume", "shifter image bindings", 1, 0,
      (spank_opt_cb_f) _opt_imagevolume },
    { "ccm", "ccm", "ccm emulation mode", 0, 0,
      (spank_opt_cb_f) _opt_ccm},
    SPANK_OPTIONS_TABLE_END
};

slurmShifter_config *ssconfig = NULL;

int slurm_spank_init(spank_t sp, int argc, char **argv) {
    spank_context_t context = spank_context();

    if (context == S_CTX_ALLOCATOR ||
        context == S_CTX_LOCAL ||
        context == S_CTX_REMOTE)
    {
        /* need to initialize ssconfig for callbacks */
        if (ssconfig == NULL)
            ssconfig = shifterSlurm_init((unsigned int) sp, argc, argv);

        /* register command line options */
        struct spank_option **optPtr = spank_option_array;
        for ( ; optPtr && *optPtr != SPANK_OPTIONS_TABLE_END; optPtr++) {
            int lrc = spank_option_register(sp, *optPtr);
            if (lrc != ESPANK_SUCCESS) {
                rc = ESPANK_ERROR;
            }
        }
    }
    return rc;
}

int slurm_spank_init_post_opt(spank_t sp, int argc, char **argv) {
    spank_context_t context = spank_context();

    shifterSpank_validate_input(
        context == S_CTX_ALLOCATOR | context == S_CTX_LOCAL
    );

    if (context == S_CTX_ALLOCATOR || context == S_CTX_LOCAL) {
        shifterSpank_init_allocator_setup();
    }
}
    
int slurm_spank_task_post_fork(spank_t sp, int argc, char **argv) {
    return shifterSpank_task_post_fork((unsigned int) sp, argc, argv);
}

int slurm_spank_job_prolog(spank_t sp, int argc, char **argv) {
    if (ssconfig == NULL)
         ssconfig = shifterSlurm_init((unsigned int) sp, argc, argv, 1);
    shifterSpank_prolog();
    
}
int slurm_spank_job_epilog(spank_t sp, int argc, char **argv) {
    if (ssconfig == NULL)
         ssconfig = shifterSlurm_init((unsigned int) sp, argc, argv, 1);
    shifterSpank_epilog();
}

int wrap_spank_setenv(unsigned int spid, const char *envname, const char *value, int overwrite) {
    return spank_setenv((spank_t) spid, envname, value, overwrite);
}

int wrap_spank_job_control_setenv(unsigned int spid, const char *envname, const char *value, int overwrite) {
    return spank_job_control_setenv((spank_t) spid, envname, value, overwrite);
}
