#include "ext2_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


void core_func(const char *img_filename, const char *src_path, const char *dst_path, int symlnk) {
    if (src_path[0] != '/' || dst_path[0] != '/') {
        fprintf(stderr, "invalid disk path found\n");
        exit(EINVAL);
    }
    if (!strcmp(src_path, dst_path)) {
        fprintf(stderr, "identical paths found\n");
        exit(EINVAL);
    }
    open_image(img_filename);
    create_lnk(src_path, dst_path, symlnk);
    close_image();
}

int main(int argc, char **argv) {
    char *img_filename;  /* image filename */
    char *src_path;      /* source path on image */
    char *dst_path;      /* destination path on image */

    if (argc != 4 && argc != 5) {
        fprintf(stderr, "%s <image file name> [-s] <source path> <dest path>\n", argv[0]);
        exit(EINVAL);
    }

    if (argc == 4) {
        img_filename = argv[1];
        src_path = argv[2];
        dst_path = argv[3];
        core_func(img_filename, src_path, dst_path, 0);
    } else if (strcmp(argv[2], "-s")) {
        fprintf(stderr, "%s <image file name> [-s] <source path> <dest path>\n", argv[0]);
        exit(EINVAL);
    } else {
        img_filename = argv[1];
        src_path = argv[3];
        dst_path = argv[4];
        core_func(img_filename, src_path, dst_path, 1);
    }

    return 0;
}

