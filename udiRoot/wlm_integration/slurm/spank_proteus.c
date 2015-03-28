#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <slurm/spank.h>

SPANK_PLUGIN(proteus, 1)

#define IMAGE_MAXLEN 1024
#define IMAGEVOLUME_MAXLEN 2048
static char image[IMAGE_MAXLEN];
static char image_type[IMAGE_MAXLEN];
static char imagevolume[IMAGEVOLUME_MAXLEN];

static int _opt_image(int val, const char *optarg, int remote);
static int _opt_imagevolume(int val, const char *optarg, int remote);

struct spank_option spank_option_array[] = {
    { "image", "image", "proteus image to use", 1, 0, (spank_opt_cb_f) _opt_image},
    { "imagevolume", "imagevolume", "proteus image bindings", 1, 0, (spank_opt_cb_f) _opt_imagevolume },
    SPANK_OPTIONS_TABLE_END
};

char *trim(char *string) {
    char *ptr = string;
    size_t len = 0;
    while(isspace(*ptr) && *ptr != 0) {
        ptr++;
    }
    len = strlen(ptr);
    while (--len > 0 && isspace(*(ptr+len))) {
        *(ptr + len) = 0;
    }
    return ptr;
}


int _opt_image(int val, const char *optarg, int remote) {
    if (optarg != NULL && strlen(optarg) > 0) {
        char *tmp = strdup(optarg);
        char *p = strchr(tmp, ':');
        if (p == NULL) {
            slurm_error("Invalid image input: must specify image type: %s", optarg);
            exit(-1);
        }
        *p++ = 0;
        snprintf(image_type, IMAGE_MAXLEN, "%s", tmp);
        snprintf(image, IMAGE_MAXLEN, "%s", p);
        free(tmp);
        p = trim(image);
        if (p != image) memmove(image, p, strlen(p) + 1);
        p = trim(image_type);
        if (p != image_type) memmove(image_type, p, strlen(p) + 1);

        for (p = image_type; *p != 0 && p-image_type < IMAGE_MAXLEN; ++p) {
            if (!isalpha(*p)) {
                slurm_error("Invalid image type - alphabetic characters only");
                exit(-1);
            }
        }
        for (p = image; *p != 0 && p-image < IMAGE_MAXLEN; ++p) {
            if (!isalnum(*p) && (*p!=':') && (*p!='_') && (*p!='-') && (*p!='.')) {
                slurm_error("Invalid image type - A-Za-z:-_. characters only");
                exit(-1);
            }
        }
        return ESPANK_SUCCESS;
    }
    slurm_error("Invalid image - must not be zero length");
    exit(-1);
}
int _opt_imagevolume(int val, const char *optarg, int remote) {
    if (optarg != NULL && strlen(optarg) > 0) {
        /* validate input */
        snprintf(imagevolume, IMAGEVOLUME_MAXLEN, optarg);
        return ESPANK_SUCCESS;
    }
    slurm_error("Invalid image volume options - if specified, must not be zero length");
    exit(-1);
}

int lookup_image(int verbose, char **image_id) {
    int rc = 0;
    char buffer[4096];
    FILE *fp = NULL;
    size_t nread = 0;
    size_t image_id_bufSize = 0;
    /* perform image lookup */
    snprintf(buffer, 4096, "/data/homes/dmj/lookup.pl %s", image);
    fp = popen(buffer, "r");
    while (!feof(fp) && !ferror(fp)) {
        char *ptr = NULL;
        nread = fread(buffer,1,4096,fp);
        if (nread == 0) break;
        *image_id = (char *) realloc(*image_id, image_id_bufSize + nread + 1);
        ptr = *image_id + image_id_bufSize;
        snprintf(ptr, nread, "%s", buffer); 
        image_id_bufSize += nread;
    } 
    if ((rc = pclose(fp)) != 0) {
        slurm_error("Image lookup process failed, exit status: %d", rc);
        exit(-1);
    }
    return 0;
}

int slurm_spank_init(spank_t sp, int argc, char **argv) {
    spank_context_t context;
    int rc = ESPANK_SUCCESS;
    int i, j;
    image[0] = 0;
    imagevolume[0] = 0;

    context = spank_context();
    //if (context == S_CTX_ALLOCATOR || context == S_CTX_LOCAL) {
        for (i = 0; spank_option_array[i].name != NULL; ++i) {
            j = spank_option_register(sp, &spank_option_array[i]);
            if (j != ESPANK_SUCCESS) {
                slurm_error("Could not register spank option %s", spank_option_array[i].name);
                rc = j;
            }
        }
    //}
    return rc;
}

int slurm_spank_init_post_opt(spank_t sp, int argc, char **argv) {
    spank_context_t context;
    int rc = ESPANK_SUCCESS;
    char *image_id = NULL;
    int verbose_lookup = 0;

    // only perform this validation at submit time
    context = spank_context();
    if (context == S_CTX_ALLOCATOR) {
        verbose_lookup = 1;
    }
    if (imagevolume != NULL && image == NULL) {
        slurm_error("Cannot specify proteus volumes without specifying the image first!");
        exit(-1);
    }
    
    lookup_image(verbose_lookup, &image_id);
    if (image_id == NULL) {
        slurm_error("Failed to lookup image.  Aborting.");
        exit(-1);
    }
    if (strlen(image) == 0) {
        return rc;
    }
    
    spank_setenv(sp, "CRAY_ROOTFS", "UDI", 1);
    spank_setenv(sp, "SLURM_PROTEUS_IMAGE", image_id, 1);
    spank_setenv(sp, "SLURM_PROTEUS_IMAGETYPE", image_type, 1);
    free(image_id);
    
    if (imagevolume != NULL) {
         spank_setenv(sp, "SLURM_PROTEUS_VOLUME", imagevolume, 1);
    }
    return rc;
}

int slurm_spank_job_prolog(spank_t sp, int argc, char **argv) {
    int rc = ESPANK_SUCCESS;
    const char *config_file = NULL;
    char image_id[1024];
    int idx = 0;
    for (idx = 0; idx < argc; ++idx) {
        if (strncmp("proteus_config", argv[idx], 14) == 0) {
            config_file = argv[idx] + 14;
            slurm_error("proteus_config file: %s", config_file);
        }
    }
    if (config_file == NULL) {
        slurm_debug("proteus_config not set, cannot use proteus");
        return rc;
    }

    if (spank_getenv(sp, "SLURM_PROTEUS_IMAGE", image_id, 1024) != ESPANK_SUCCESS) {
        return rc;
    }
    if (spank_getenv(sp, "SLURM_PROTEUS_IMAGETYPE", image_type, 1024) != ESPANK_SUCCESS) {
        return rc;
    }
    
    return rc;
} 
