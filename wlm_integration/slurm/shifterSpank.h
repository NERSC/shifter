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
    char *shifter_config;
    char *extern_setup;
    char *memory_cgroup;

    /* config options from user */
    char *image;
    char *imageType;
    char *imageVolume;
    int ccmMode;

    /* derived configurations */
    UdiRootConfig *udiConfig;
} shifter_spank_config;

/** shifterSpank_init read config options and populates config 
 * 
 *  Reads arguments from plugstack.conf. Then reads resulting
 *  derived configurations (such as the 
 */
int shifterSpank_init(int argc, char **argv);
void shifterSpank_validate_input(int allocator);


#ifdef __cplusplus
}
#endif

#endif
