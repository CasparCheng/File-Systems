#include "ext2_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


void core_func(const char *img_filename, const char *dst_path) {
    if (dst_path[0] != '/') {
        fprintf(stderr, "invalid disk path found\n");
        exit(EINVAL);
    }
    open_image(img_filename);
    remove_reg_or_lnk(dst_path);
    close_image();
}

int main(int argc, char **argv) {
    char *img_filename;  /* image filename */
    char *dst_path;      /* directory path on image */

    if (argc != 3) {
        fprintf(stderr, "%s <image file name> <path to link>\n", argv[0]);
        exit(EINVAL);
    }

    img_filename = argv[1];
    dst_path = argv[2];

    core_func(img_filename, dst_path);

    return 0;
}
