#ifndef _EXT2_UTILS_
#define _EXT2_UTILS_

#include "ext2.h"

#include <stdio.h>

void open_image(const char *filename);
void close_image();
void create_dir(const char *dir_path);
void create_reg(FILE *fp, const char *dst_path);
void create_lnk(const char *src_path, const char *dst_path, int symlnk);
void remove_reg_or_lnk(const char *dst_path);
void restore_reg_or_lnk(const char *dst_path);
void check_image();

#endif /* _EXT2_UTILS_ */
