#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

unsigned char *disk;

void *locate_block(int i);
int is_bit_set(int bit, const unsigned char *bitmap);
void print_bitmap(const unsigned char *bitmap,
                  int count, const char *name);
void print_inodes(const struct ext2_inode *inodes,
                  const unsigned char *bitmap, int count);
void print_directories(int n, const struct ext2_inode *inodes,
                       const unsigned char *bitmap, int count);


int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
	if(fd == -1) {
		perror("open");
		exit(1);
    }

    disk = mmap(NULL, 128 * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        close(fd);
        perror("mmap");
        exit(1);
    }

    struct ext2_super_block *sb = (struct ext2_super_block *)locate_block(1);
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);

    struct ext2_group_desc *gd = (struct ext2_group_desc *)locate_block(2);
    printf("Block group:\n");
    printf("    block bitmap: %d\n", gd->bg_block_bitmap);
    printf("    inode bitmap: %d\n", gd->bg_inode_bitmap);
    printf("    inode table: %d\n", gd->bg_inode_table);
    printf("    free blocks: %d\n", gd->bg_free_blocks_count);
    printf("    free inodes: %d\n", gd->bg_free_inodes_count);
    printf("    used_dirs: %d\n", gd->bg_used_dirs_count);

    unsigned char *block_bitmap = (unsigned char *)locate_block(gd->bg_block_bitmap);
    print_bitmap(block_bitmap, sb->s_blocks_count, "Block bitmap");

    unsigned char *inode_bitmap = (unsigned char *)locate_block(gd->bg_inode_bitmap);
    print_bitmap(inode_bitmap, sb->s_inodes_count, "Inode bitmap");

    struct ext2_inode *inodes = (struct ext2_inode *)locate_block(gd->bg_inode_table);
    printf("\n");
    printf("Inodes:\n");
    print_inodes(inodes, inode_bitmap, sb->s_inodes_count);

    printf("\n");
    printf("Directory Blocks:\n");
    print_directories(2, inodes, inode_bitmap, sb->s_inodes_count);

    munmap(disk, 128 * EXT2_BLOCK_SIZE);
    close(fd);
    
    return 0;
}


void *locate_block(int i) {
    return disk + EXT2_BLOCK_SIZE * i;
}

int is_bit_set(int bit, const unsigned char *bitmap) {
    int i = bit / 8;
    int j = bit % 8;

    return (bitmap[i] >> j) & 1 ? 1 : 0;
}

void print_bitmap(const unsigned char *bitmap,
                  int count, const char *name) {
    int imax = count / 8 + (count % 8 ? 1 : 0);

    printf("%s:", name);
    for (int i = 0, j = count; i < imax; ++i, j -= 8) {
        unsigned char b = bitmap[i];
        printf(" ");
        int kmax = j < 8 ? j : 8;
        for (int k = 0; k < kmax; ++k) {
            printf("%d", b & 1 ? 1 : 0);
            b >>= 1;
        }
    }
    printf("\n");
}

void print_inodes(const struct ext2_inode *inodes,
                  const unsigned char *bitmap, int count) {
    for (int i = 1; i < count; ++i) {

        if (!is_bit_set(i, bitmap)) {
            continue;
        }

        const struct ext2_inode *inode = inodes + i;
        unsigned short i_mode_type = inode->i_mode & 0xF000;
        char type;

        switch (i_mode_type) {
            case EXT2_S_IFLNK:
                type = 'l';
                break;
            case EXT2_S_IFREG:
                type = 'f';
                break;
            case EXT2_S_IFDIR:
                type = 'd';
                break;
            default:
                type = 0;
                break;
        }

        if (type) {
            printf("[%d] type: %c size: %d links: %d blocks: %d\n",
                   i + 1, type, inode->i_size, inode->i_links_count, inode->i_blocks);
            printf("[%d] Blocks: ", i + 1);
            for (int j = 0; j < 15; ++j) {
                if (!inode->i_block[j]) {
                    break;
                }
                printf(" %u", inode->i_block[j]);
            }
            printf("\n");
        }

        if (i + 1 == EXT2_ROOT_INO) {
            i = EXT2_GOOD_OLD_FIRST_INO - 1;
        }
    }
}

void print_directories(int n, const struct ext2_inode *inodes,
                       const unsigned char *bitmap, int count) {
    int i = n - 1;
    const struct ext2_inode *inode = inodes + i;

    for (int j = 0; j < 12; ++j) {
        if (!inode->i_block[j]) {
            break;
        }
        printf("    DIR BLOCK NUM: %u (for inode %d)\n", inode->i_block[j], n);

        struct ext2_dir_entry *dir = (struct ext2_dir_entry *)locate_block(inode->i_block[j]);
        for (int len = 0; len < EXT2_BLOCK_SIZE && dir->rec_len != 0; len += dir->rec_len) {
            int ft_type = dir->file_type & 0x7;
            char type;
            switch (ft_type) {
                case EXT2_FT_REG_FILE:
                    type = 'f';
                    break;
                case EXT2_FT_DIR:
                    type = 'd';
                    break;
                case EXT2_FT_SYMLINK:
                    type = 'l';
                    break;
                default:
                    type = 0;
                    break;
            }
            if (type) {
                printf("Inode: %d rec_len: %d name_len: %d type= %c name=%.*s\n",
                       dir->inode, dir->rec_len, dir->name_len, type,
                       dir->name_len, dir->name);
            }
            dir = (struct ext2_dir_entry *)((char *)dir + dir->rec_len);
        }

        dir = (struct ext2_dir_entry *)locate_block(inode->i_block[j]);
        for (int len = 0; len < EXT2_BLOCK_SIZE && dir->rec_len != 0; len += dir->rec_len) {
            int ft_type = dir->file_type & 0x7;
            if (ft_type == EXT2_FT_DIR) {
                if (!(dir->inode < 12 ||
                      (dir->name_len == 1 && dir->name[0] == '.') ||
                      (dir->name_len == 2 && dir->name[0] == '.' && dir->name[1] == '.'))) {
                    print_directories(dir->inode, inodes, bitmap, count);
                }
            }
            dir = (struct ext2_dir_entry *)((char *)dir + dir->rec_len);
        }
    }
}
