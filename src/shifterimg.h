/**
 *  @file shifterimg.h
 *  @brief header file for shifterimg
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

enum ImageGwAction {
    MODE_LOOKUP = 0,
    MODE_INSPECT,
    MODE_PULL,
    MODE_IMAGES,
    MODE_LOGIN,
    MODE_PULL_NONBLOCK,
    MODE_EXPIRE,
    MODE_AUTOEXPIRE,
    MODE_INVALID
};

enum AclCredential {
    USER_ACL = 0,
    GROUP_ACL,
    INVALID_ACL
};

typedef struct _LoginCredential {
    char *system;
    char *location;
    char *cred;
} LoginCredential;

struct options {
    int verbose;
    enum ImageGwAction mode;
    char *type;
    char *tag;
    char *rawtype;
    char *rawtag;
    char *location;
    char *rawlocation;
    LoginCredential **loginCredentials;

    int *allowed_uids;
    size_t allowed_uids_len;
    size_t allowed_uids_sz;

    int *allowed_gids;
    size_t allowed_gids_len;
    size_t allowed_gids_sz;
};

typedef struct _ImageGwState {
    char *message;
    int isJsonMessage;
    size_t expContentLen;
    size_t messageLen;
    size_t messageCurr;
    int messageComplete;
} ImageGwState;

typedef struct _ImageGwImageRec {
    char *entryPoint;
    char **env;
    char *workdir;
    char **groupAcl;
    char *identifier;
    char *type;
    char *status;
    double last_pull;
    char *system;
    char **tag;
    char **userAcl;
} ImageGwImageRec;
