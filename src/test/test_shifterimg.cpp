/**
 *  @file test_shifterimg.cpp
 *  @brief test harness for functions in shifterimg
 * 
 * @author Douglas M. Jacobsen <dmjacobsen@lbl.gov>
 */

/* Shifter, Copyright (c) 2017, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of any
 * required approvals from the U.S. Dept. of Energy).  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. Neither the name of the University of California, Lawrence Berkeley
 *     National Laboratory, U.S. Dept. of Energy nor the names of its
 *     contributors may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 * 
 * See LICENSE for full text.
 */

#include <CppUTest/CommandLineTestRunner.h>
#include <stdlib.h>
#include <string.h>

#include "shifterimg.h"

extern "C" {
extern void _add_allowed(enum AclCredential aclType, struct options *config, const char *arg);
}

TEST_GROUP(ShifterimgTestGroup) {
};

TEST(ShifterimgTestGroup, addAllowedTest) {
    struct options config;
    memset(&config, 0, sizeof(struct options));

    _add_allowed(USER_ACL, &config, "1,2,3,4");
    CHECK(config.allowed_uids != NULL);
    CHECK(config.allowed_gids == NULL);
    CHECK(config.allowed_uids_len == 4);
    for (size_t i = 0; i < config.allowed_uids_len; i++) {
        CHECK(config.allowed_uids[i] == (int)i + 1);
    }
    free(config.allowed_uids);
}

int main(int argc, char **argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
