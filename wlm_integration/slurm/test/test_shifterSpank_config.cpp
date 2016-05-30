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

#include "shifterSpank.h"

#include <CppUTest/CommandLineTestRunner.h>

TEST_GROUP(shifterSpankConfigGroup) {
};

TEST(shifterSpankConfigGroup, BasicInitTest) {
    shifterSpank_config *config = shifterSpank_init(NULL, 0, NULL, 0);
    CHECK(config != NULL);
    CHECK(config->udiConfig != NULL);

    shifterSpank_config_free(config);

    char *args[] = {
        strdup("extern_setup=/usr/bin/echo"),
        NULL
    };
    config = shifterSpank_init(NULL, 1, args, 0);
    CHECK(config != NULL);
    CHECK(config->extern_setup != NULL);
    CHECK(strcmp(config->extern_setup, "/usr/bin/echo") == 0);
    shifterSpank_config_free(config);

    free(args[0]);
    
}

int main(int argc, char** argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
