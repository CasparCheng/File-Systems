#include "ext2_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


void core_func(const char *img_filename, const char *src_filename, const char *dst_path) {
    FILE *fp;
    if (dst_path[0] != '/') {
        fprintf(stderr, "invalid disk path found\n");
        exit(EINVAL);
    }
    if (!(fp = fopen(src_filename, "rb"))) {
        perror("fopen");
        exit(EINVAL);
    }
    open_image(img_filename);
    create_reg(fp, dst_path);
    close_image();
    fclose(fp);
}

int main(int argc, char **argv) {
    char *img_filename;  /* image filename */
    char *src_filename;  /* source filename on native FS */
    char *dst_path;      /* destination filename on image */

    if (argc != 4) {
        fprintf(stderr, "%s <image file name> <path to source file> <path to dest>\n", argv[0]);
        exit(EINVAL);
    }

    img_filename = argv[1];
    src_filename = argv[2];
    dst_path = argv[3];

    core_func(img_filename, src_filename, dst_path);

    return 0;
}

