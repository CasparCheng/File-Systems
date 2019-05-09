#include "ext2_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


void core_func(const char *img_filename, const char *dir_path) {
    if (dir_path[0] != '/') {
        fprintf(stderr, "invalid disk path found\n");
        exit(EINVAL);
    }
    open_image(img_filename);
    create_dir(dir_path);
    close_image();
}

int main(int argc, char **argv) {
    char *img_filename;  /* image filename */
    char *dir_path;      /* directory path on image */

    if (argc != 3) {
        fprintf(stderr, "%s <image file name> <path>\n", argv[0]);
        exit(EINVAL);
    }

    img_filename = argv[1];
    dir_path = argv[2];

    core_func(img_filename, dir_path);

    return 0;
}
