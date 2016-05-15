/* Shifter, Copyright (c) 2016, The Regents of the University of California,
 through Lawrence Berkeley National Laboratory (subject to receipt of any
 required approvals from the U.S. Dept. of Energy).  All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
  3. Neither the name of the University of California, Lawrence Berkeley
     National Laboratory, U.S. Dept. of Energy nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

See LICENSE for full text.
*/

#ifndef __SHIFTERSPANK_INCLUDE
#define __SHIFTERSPANK_INCLUDE

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* config options from plugstack.conf */
    char *shifter_config;         /*! path to configuration file */
    char *extern_setup;           /*! path to extern setup script */
    char *memory_cgroup;          /*! path to memory cgroup mount */

    /* config options from user */
    char *image;                  /*! user requested image identifier */
    char *imageType;              /*! image type */
    char *volume;                 /*! volume remap options */
    int ccmMode;                  /*! flag if this is ccm mode */

    /* derived configurations */
    UdiRootConfig *udiConfig;     /*! udiroot configuration */
    void *id;                     /*! spank structure pointer */
} shifterSpank_config;

#define ERROR 1
#define SUCCESS 0

/** shifterSpank_init read config options and populates config 
 * 
 *  Reads arguments from plugstack.conf. Then reads resulting
 *  derived configurations (such as the 
 */
shifterSpank_config *shifterSpank_init(void *spid,
    int argc, char **argv, int loadoptarg);

/** shifterSpank_config_free destroys a configuration structure
 *
 * Frees and sets to NULL all memory within the configuration data structure
 */
void shifterSpank_config_free(shifterSpank_config *ssconfig);

/** shifterSpank_validate_input checks user-provided data
 *
 * Validates the contents of the global shifter_spank_config
 * once populated by shifterSpank_init.  If allocator mode is
 * it may exit the program upon invalid input. Otherwise, it
 * will fix the contents of config obj to ensure safe
 * operation.
 *
 * @param ssconfig configuration structure
 * @param allocator boolean (1 for true, 0 for false) if called
 *                  from allocator or local context
 * @return Nothing
 */
void shifterSpank_validate_input(shifterSpank_config *ssconfig, int allocator);

/** shifterSpank_init_allocator_setup
 *
 * Performs the image gateway lookup during allocation step and caches the
 * resulting values in the environment of the job.  Must be called after
 * shifterSpank_init and optarg processing.
 * @param ssconfig configuration structure
 */ 
void shifterSpank_init_allocator_setup(shifterSpank_config *ssconfig);


/** shifterSpank_test_post_fork used to do final setup in the extern step
 *
 * Determines if the current task is the extern step, and if so performs
 * any needed setup
 * @returns SUCCESS for success or FAILURE for failure
 */
int shifterSpank_task_post_fork(void *ptr, int argc, char **argv);

/** shifterSpank_prolog
 *
 * Performs all the required setup to stage the image on the compute node
 * prior to job initialization
 * @param ssconfig configuration structure
 * @returns SUCCESS for success or FAILURE for failure
 */
int shifterSpank_prolog(shifterSpank_config *ssconfig);

/** shifterSpank_epilog
 *
 * Performs all the required setup to teardown the image on the compute node
 * following job completion
 * @param ssconfig configuration structure
 * @returns SUCCESS for success or FAILURE for failure
 */
int shifterSpank_epilog(shifterSpank_config *ssconfig);

int shifterSpank_process_option_ccm(
    shifterSpank_config *ssconfig, int val, const char *optarg, int remote);

int shifterSpank_process_option_image(
    shifterSpank_config *ssconfig, int val, const char *optarg, int remote);

int shifterSpank_process_option_volume(
    shifterSpank_config *ssconfig, int val, const char *optarg, int remote);


#ifdef __cplusplus
}
#endif

#endif
