#include "ext2_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


void core_func(const char *img_filename) {
    open_image(img_filename);
    check_image();
    close_image();
}

int main(int argc, char **argv) {
    char *img_filename;  /* image filename */

    if (argc != 2) {
        fprintf(stderr, "%s <image file name>\n", argv[0]);
        exit(EINVAL);
    }

    img_filename = argv[1];

    core_func(img_filename);

    return 0;
}
