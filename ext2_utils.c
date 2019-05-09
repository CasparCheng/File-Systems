#include "ext2_utils.h"
#include "ext2_pathtokens.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define ABS(x) ((x > 0) ? (x) : (-x))

typedef int (*cb_iterate_dent)(struct ext2_dir_entry *dent);

/* ------------------- check type ------------------- */
int get_inode_type(int n_inode);
int is_inode_dir(int n_inode);
int is_inode_reg(int n_inode);
int is_inode_sym(int n_inode);
int get_dent_type(const struct ext2_dir_entry *dent);
int is_dent_dir(const struct ext2_dir_entry *dent);
int is_dent_reg(const struct ext2_dir_entry *dent);
int is_dent_sym(const struct ext2_dir_entry *dent);
void set_dent_type(struct ext2_dir_entry *dent, int type);
void set_dent_dir(struct ext2_dir_entry *dent);
void set_dent_reg(struct ext2_dir_entry *dent);
void set_dent_sym(struct ext2_dir_entry *dent);
/* ------------------- manipulate block/inode ------------------- */
int alloc_block();
int alloc_inode();
void restore_block(int n_block);
void restore_inode(int n_inode);
void free_block(int n_block);
void free_inode(int n_inode);
void init_block(int n_block);
void init_inode(int n_inode);
int alloc_block_any(int n_inode);
int alloc_inode_w_mode(int mode);
int alloc_inode_dir();
int alloc_inode_reg();
int alloc_inode_sym();
int find_free_block();
int find_free_inode();
/* ------------------- manipulate block/inode bitmap ------------------- */
int chk_bit(int bit, const unsigned char *bitmap);
void set_bit(int bit, unsigned char *bitmap);
void clr_bit(int bit, unsigned char *bitmap);
int chk_inodebit(int n_inode);
int chk_blockbit(int n_block);
void set_inodebit(int n_inode);
void set_blockbit(int n_block);
void clr_inodebit(int n_inode);
void clr_blockbit(int n_block);
/* ------------------- manipulate disk pointer ------------------- */
void *offset_ptr(void *ptr, int dist);
int calc_offset_ptr(void *ptr1, void *ptr2);
struct ext2_inode *locate_inode(int n_inode);
void *locate_block(int n_block);
/* ------------------- manipulate dir_entry.name_len ------------------- */
int get_name_len(const char *name);
int padding_name_len(int name_len);
/* ------------------- manipulate dir_entry ------------------- */
void init_dent(struct ext2_dir_entry *dir, unsigned int n_inode,
               unsigned short rec_len, unsigned char name_len,
               unsigned char file_type, const char *name);
void add_dent(int n_inode, int n_pdir_inode, const char *name, int file_type);
int add_dent_in_block(int n_inode, int n_pdir_inode, const char *name, int type,
                      int n_block);
void del_dent(int n_inode, int n_pdir_inode);
int del_dent_in_block(int n_inode, int n_pdir_inode, int n_block);
void add_dent_dir(int n_inode, int n_pdir_inode, const char *name);
void add_dent_reg(int n_inode, int n_pdir_inode, const char *name);
void add_dent_sym(int n_inode, int n_pdir_inode, const char *name);
int iterate_dent(int n_pdir_inode, cb_iterate_dent cb);
int find_dent_by_name(int n_pdir_inode, const char *name,
                      struct ext2_dir_entry **dent);
int find_dent_dir_by_path(const struct path_tokens *pt);
int find_dent_any_by_path(const struct path_tokens *pt);
int find_dent_by_path(const struct path_tokens *pt, int *type);
struct ext2_dir_entry *find_deleteddent_helper(int n_pdir_inode,
                                               const char *name,
                                               struct ext2_dir_entry **prev_dir,
                                               int *rec_len);
int find_deleteddent(int n_pdir_inode, const char *name, int *type);
void restore_deleteddent(int n_pdir_inode, const char *name);
/* ------------------- iterate blocks ------------------- */
int find_block_linear(int n_inode, int i);
int find_block_lastused(int n_inode);
/* ------------------- check image ------------------- */
int check_bitmaps();
int cb_check_i_mode(struct ext2_dir_entry *dent);
int cb_check_inode_mark(struct ext2_dir_entry *dent);
int cb_check_inode_i_dtime(struct ext2_dir_entry *dent);
int cb_check_block_mark(struct ext2_dir_entry *dent);

static int fd = 0;
static unsigned char *disk = NULL;
static struct ext2_super_block *sb = NULL;
static struct ext2_group_desc *gd = NULL;
static unsigned char *block_bmp = NULL;
static unsigned char *inode_bmp = NULL;
static struct ext2_inode *inode_tbl = NULL;

/* ----------- Public Functions ----------- */

void open_image(const char *filename) {
    if ((fd = open(filename, O_RDWR)) == -1) {
        perror("open");
        exit(ENOENT);
    }
    if ((disk = mmap(NULL, 128 * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0)) == MAP_FAILED) {
        close(fd);
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    sb = locate_block(1);
    gd = locate_block(2);
    block_bmp = locate_block(gd->bg_block_bitmap);
    inode_bmp = locate_block(gd->bg_inode_bitmap);
    inode_tbl = locate_block(gd->bg_inode_table);
}

void close_image() {
    if (disk && munmap(disk, 128 * EXT2_BLOCK_SIZE) != 0) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }
    if (close(fd) != 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }
    fd = 0;
    disk = NULL;
    sb = NULL;
    gd = NULL;
    block_bmp = inode_bmp = NULL;
    inode_tbl = NULL;
}

void check_image() {
    int cnt;

    cnt = check_bitmaps();
    cnt += iterate_dent(2, cb_check_i_mode);
    cnt += iterate_dent(2, cb_check_inode_mark);
    cnt += iterate_dent(2, cb_check_inode_i_dtime);
    cnt += iterate_dent(2, cb_check_block_mark);

    if (cnt > 0) {
        printf("%d file system inconsistencies repaired!\n", cnt);
    } else {
        printf("No file system inconsistencies detected!\n");
    }
}

int cb_check_block_mark(struct ext2_dir_entry *dent) {
    int n_inode;
    int n_block;
    static struct ext2_inode *inode;
    int n_fixed_blocks;

    n_inode = dent->inode;

    if (n_inode < 1) {
        return 0;
    }

    inode = locate_inode(n_inode);
    n_fixed_blocks = 0;

    int i = 0;
    while ((n_block = find_block_linear(n_inode, i++))) {
        if (!chk_blockbit(n_block)) {
            set_blockbit(n_block);
            --sb->s_free_blocks_count;
            --gd->bg_free_blocks_count;
            ++n_fixed_blocks;
        }
    }
    if (i > 13) {
        n_block = inode->i_block[12];
        if (!chk_blockbit(n_block)) {
            set_blockbit(n_block);
            --sb->s_free_blocks_count;
            --gd->bg_free_blocks_count;
            ++n_fixed_blocks;
        }
    }

    if (n_fixed_blocks > 0) {
        printf("Fixed: %d in­use data blocks not marked in data bitmap for "
               "inode: [%d]\n",
               n_fixed_blocks, n_inode);
        return n_fixed_blocks;
    }

    return 0;
}

int cb_check_inode_i_dtime(struct ext2_dir_entry *dent) {
    int n_inode;
    static struct ext2_inode *inode;

    n_inode = dent->inode;

    if (n_inode < 1) {
        return 0;
    }

    inode = locate_inode(n_inode);

    if (inode->i_dtime != 0) {
        inode->i_dtime = 0;
        printf("Fixed: valid inode marked for deletion: [%d]\n", n_inode);
        return 1;
    }

    return 0;
}

int cb_check_inode_mark(struct ext2_dir_entry *dent) {
    int n_inode;

    n_inode = dent->inode;

    if (n_inode < 1) {
        return 0;
    }

    if (!chk_inodebit(n_inode)) {
        set_inodebit(n_inode);
        --sb->s_free_inodes_count;
        --gd->bg_free_inodes_count;
        printf("Fixed: inode [%d] not marked as in­use\n", n_inode);
        return 1;
    }

    return 0;
}

int cb_check_i_mode(struct ext2_dir_entry *dent) {
    int n_inode;

    n_inode = dent->inode;

    if (n_inode < 1) {
        return 0;
    }

    if (is_dent_reg(dent)) {
        if (!is_inode_reg(n_inode)) {
            set_dent_type(dent, get_inode_type(n_inode));
            printf("Fixed: Entry type vs inode mismatch: inode [%d]\n",
                   n_inode);
            return 1;
        }
    } else if (is_dent_dir(dent)) {
        if (!is_inode_dir(n_inode)) {
            set_dent_type(dent, get_inode_type(n_inode));
            printf("Fixed: Entry type vs inode mismatch: inode [%d]\n",
                   n_inode);
            return 1;
        }
    } else if (is_dent_sym(dent)) {
        if (!is_inode_sym(n_inode)) {
            set_dent_type(dent, get_inode_type(n_inode));
            printf("Fixed: Entry type vs inode mismatch: inode [%d]\n",
                   n_inode);
            return 1;
        }
    }

    return 0;
}

int check_bitmaps() {
    int cnt;
    int n_free_inodes, n_free_blocks;
    int n_free_inodes_diff, n_free_blocks_diff;

    cnt = 0;

    n_free_inodes = 0;
    for (int n_inode = 1; n_inode <= sb->s_inodes_count; ++n_inode) {
        if (!chk_inodebit(n_inode)) {
            ++n_free_inodes;
        }
    }

    if ((n_free_inodes_diff = sb->s_free_inodes_count - n_free_inodes)) {
        sb->s_free_inodes_count = n_free_inodes;
        printf("Fixed: superblock's free inodes counter was off by %d compared "
               "to the bitmap\n",
               ABS(n_free_inodes_diff));
        cnt += ABS(n_free_inodes_diff);
    }
    if ((n_free_inodes_diff = gd->bg_free_inodes_count - n_free_inodes)) {
        gd->bg_free_inodes_count = n_free_inodes;
        printf("Fixed: block group's free inodes counter was off by %d "
               "compared to the bitmap\n",
               ABS(n_free_inodes_diff));
        cnt += ABS(n_free_inodes_diff);
    }

    n_free_blocks = 0;
    for (int n_block = 1; n_block <= sb->s_blocks_count; ++n_block) {
        if (!chk_blockbit(n_block)) {
            ++n_free_blocks;
        }
    }

    if ((n_free_blocks_diff = sb->s_free_blocks_count - n_free_blocks)) {
        sb->s_free_blocks_count = n_free_blocks;
        printf("Fixed: superblock's free blocks counter was off by %d compared "
               "to the bitmap\n",
               ABS(n_free_blocks_diff));
        cnt += ABS(n_free_blocks_diff);
    }
    if ((n_free_blocks_diff = gd->bg_free_blocks_count - n_free_blocks)) {
        gd->bg_free_blocks_count = n_free_blocks;
        printf("Fixed: block group's free blocks counter was off by %d "
               "compared to the bitmap\n",
               ABS(n_free_blocks_diff));
        cnt += ABS(n_free_blocks_diff);
    }

    return cnt;
}

void restore_reg_or_lnk(const char *dst_path) {
    int n_dst_inode, n_pdir_inode;
    struct ext2_inode *dst_inode;
    struct path_tokens *dst_pt, *pdir_pt;
    int n_block;
    int type;

    dst_pt = create_path_tokens(dst_path);
    pdir_pt = create_path_tokens(dst_path);
    pop_path_token(pdir_pt);

    if ((n_pdir_inode = find_dent_dir_by_path(pdir_pt)) < 0) {
        fprintf(stderr, "parent directory of %s not found\n", dst_path);
        exit(ENOENT);
    }

    if ((find_dent_any_by_path(dst_pt)) > 0) {
        fprintf(stderr, "%s already exists\n", dst_path);
        exit(EEXIST);
    }

    if ((n_dst_inode = find_deleteddent(
             n_pdir_inode, get_path_tokens_last(dst_pt), &type)) < 0) {
        fprintf(stderr, "%s not found as deleted file\n", dst_path);
        exit(ENONET);
    }
    if (type == EXT2_FT_DIR) {
        fprintf(stderr, "%s refers to a deleted directoy\n", dst_path);
        exit(EISDIR);
    }

    if (chk_inodebit(n_dst_inode)) {
        fprintf(stderr, "inode of %s is already taken\n", dst_path);
        exit(ENOENT);
    }

    restore_deleteddent(n_pdir_inode, get_path_tokens_last(dst_pt));

    dst_inode = locate_inode(n_dst_inode);
    if (dst_inode->i_links_count > 0) {
        int i = 0;
        while ((n_block = find_block_linear(n_dst_inode, i++))) {
            restore_block(n_block);
        }
        if (i > 13) {
            restore_block(dst_inode->i_block[12]);
        }
        restore_inode(n_dst_inode);
        dst_inode->i_dtime = 0;
    }

    destroy_path_tokens(dst_pt);
    destroy_path_tokens(pdir_pt);
}

void remove_reg_or_lnk(const char *dst_path) {
    int n_dst_inode, n_pdir_inode;
    struct ext2_inode *dst_inode;
    struct path_tokens *dst_pt, *pdir_pt;
    int n_block;

    dst_pt = create_path_tokens(dst_path);
    pdir_pt = create_path_tokens(dst_path);
    pop_path_token(pdir_pt);

    if ((n_dst_inode = find_dent_any_by_path(dst_pt)) < 0) {
        fprintf(stderr, "%s not found\n", dst_path);
        exit(ENOENT);
    }

    if (is_inode_dir(n_dst_inode)) {
        fprintf(stderr, "%s refers to a directory\n", dst_path);
        exit(EISDIR);
    }

    if ((n_pdir_inode = find_dent_dir_by_path(pdir_pt)) < 0) {
        fprintf(stderr, "parent directory of %s not found\n", dst_path);
        exit(ENOENT);
    }

    del_dent(n_dst_inode, n_pdir_inode);

    dst_inode = locate_inode(n_dst_inode);
    if (dst_inode->i_links_count == 0) {
        int i = 0;
        while ((n_block = find_block_linear(n_dst_inode, i++))) {
            free_block(n_block);
        }
        if (i > 13) {
            free_block(dst_inode->i_block[12]);
        }
        free_inode(n_dst_inode);
        dst_inode->i_dtime = time(NULL);
    }

    destroy_path_tokens(dst_pt);
    destroy_path_tokens(pdir_pt);
}

void create_lnk(const char *src_path, const char *dst_path, int symlnk) {
    int n_src_inode, n_dst_inode, n_pdir_inode;
    struct ext2_inode *dst_inode;
    struct path_tokens *src_pt, *dst_pt, *pdir_pt;
    int n_block;
    unsigned char *block;

    src_pt = create_path_tokens(src_path);
    dst_pt = create_path_tokens(dst_path);
    pdir_pt = create_path_tokens(dst_path);
    pop_path_token(pdir_pt);

    if ((n_pdir_inode = find_dent_dir_by_path(pdir_pt)) < 0) {
        fprintf(stderr, "parent directory of %s not found\n", dst_path);
        exit(ENOENT);
    }

    if ((n_src_inode = find_dent_any_by_path(src_pt)) < 0) {
        fprintf(stderr, "%s not found\n", src_path);
        exit(ENOENT);
    }

    if (find_dent_any_by_path(dst_pt) > 0) {
        fprintf(stderr, "%s already exists\n", dst_path);
        exit(EEXIST);
    }

    if (symlnk) {
        n_dst_inode = alloc_inode_sym();
        dst_inode = locate_inode(n_dst_inode);
        add_dent_sym(n_dst_inode, n_pdir_inode, get_path_tokens_last(dst_pt));
        n_block = alloc_block_any(n_dst_inode);
        block = locate_block(n_block);
        memcpy(block, src_path, strlen(src_path));
        dst_inode->i_size = strlen(src_path);
        dst_inode->i_dtime = 0;
    } else {
        if (is_inode_dir(n_src_inode)) {
            fprintf(stderr, "%s refers to a directory\n", src_path);
            exit(EISDIR);
        }
        add_dent_reg(n_src_inode, n_pdir_inode, get_path_tokens_last(dst_pt));
    }

    destroy_path_tokens(src_pt);
    destroy_path_tokens(dst_pt);
    destroy_path_tokens(pdir_pt);
}

void create_reg(FILE *fp, const char *dst_path) {
    int n_dst_inode, n_pdir_inode;
    struct ext2_inode *dst_inode;
    struct path_tokens *dst_pt, *pdir_pt;
    int n_block;
    int actual_sz, expect_sz, remaining_sz, total_sz;
    unsigned char *block;

    dst_pt = create_path_tokens(dst_path);
    pdir_pt = create_path_tokens(dst_path);
    pop_path_token(pdir_pt);

    if ((n_pdir_inode = find_dent_dir_by_path(pdir_pt)) < 0) {
        fprintf(stderr, "parent directory of %s not found\n", dst_path);
        exit(ENOENT);
    }

    if (find_dent_any_by_path(dst_pt) > 0) {
        fprintf(stderr, "%s already exists\n", dst_path);
        exit(EEXIST);
    }

    n_dst_inode = alloc_inode_reg();

    dst_inode = locate_inode(n_dst_inode);

    add_dent_reg(n_dst_inode, n_pdir_inode, get_path_tokens_last(dst_pt));

    fseek(fp, 0L, SEEK_END);
    remaining_sz = total_sz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    while (remaining_sz > 0) {
        n_block = alloc_block_any(n_dst_inode);
        block = locate_block(n_block);
        expect_sz = MIN(remaining_sz, EXT2_BLOCK_SIZE);
        actual_sz = fread(block, 1, expect_sz, fp);
        while (actual_sz < expect_sz) {
            actual_sz += fread(block + actual_sz, 1, expect_sz - actual_sz, fp);
        }
        remaining_sz -= actual_sz;
    }

    dst_inode->i_size = total_sz;
    dst_inode->i_dtime = 0;

    destroy_path_tokens(dst_pt);
    destroy_path_tokens(pdir_pt);
}

void create_dir(const char *dir_path) {
    int n_dir_inode, n_pdir_inode;
    struct ext2_inode *dir_inode;
    struct path_tokens *dir_pt, *pdir_pt;

    dir_pt = create_path_tokens(dir_path);
    pdir_pt = create_path_tokens(dir_path);
    pop_path_token(pdir_pt);

    if ((n_pdir_inode = find_dent_dir_by_path(pdir_pt)) < 0) {
        fprintf(stderr, "parent directory of %s not found\n", dir_path);
        exit(ENOENT);
    }

    if (find_dent_any_by_path(dir_pt) > 0) {
        fprintf(stderr, "%s already exists\n", dir_path);
        exit(EEXIST);
    }

    n_dir_inode = alloc_inode_dir();
    dir_inode = locate_inode(n_dir_inode);

    dir_inode->i_dtime = 0;

    add_dent_dir(n_dir_inode, n_pdir_inode, get_path_tokens_last(dir_pt));
    alloc_block_any(n_dir_inode);
    add_dent_dir(n_dir_inode, n_dir_inode, ".");
    add_dent_dir(n_pdir_inode, n_dir_inode, "..");

    ++gd->bg_used_dirs_count;

    destroy_path_tokens(dir_pt);
    destroy_path_tokens(pdir_pt);
}

/* ----------- Private Functions ----------- */

/* ------------------- iterate blocks ------------------- */

int find_block_linear(int n_inode, int i) {
    struct ext2_inode *inode;
    int n_block;
    unsigned int *block;

    inode = locate_inode(n_inode);

    if (i < 12) {
        n_block = inode->i_block[i];
    } else {
        block = locate_block(inode->i_block[12]);
        n_block = block[i - 12];
    }

    return n_block;
}

int find_block_lastused(int n_inode) {
    struct ext2_inode *inode;
    int n_block;
    int is_found;
    unsigned int *block;

    inode = locate_inode(n_inode);

    for (int i = 1; i < 13; ++i) {
        if (!inode->i_block[i]) {
            n_block = inode->i_block[i - 1];
            is_found = 1;
            break;
        }
    }

    if (!is_found) {
        block = locate_block(inode->i_block[12]);
        for (int i = 1; i < EXT2_BLOCK_SIZE / 4; ++i) {
            if (!block[i]) {
                n_block = block[i - 1];
                break;
            }
        }
    }

    return n_block;
}

/* ------------------- manipulate dir_entry ------------------- */

void add_dent_dir(int n_inode, int n_pdir_inode, const char *name) {
    add_dent(n_inode, n_pdir_inode, name, EXT2_FT_DIR);
}
void add_dent_reg(int n_inode, int n_pdir_inode, const char *name) {
    add_dent(n_inode, n_pdir_inode, name, EXT2_FT_REG_FILE);
}
void add_dent_sym(int n_inode, int n_pdir_inode, const char *name) {
    add_dent(n_inode, n_pdir_inode, name, EXT2_FT_SYMLINK);
}

void init_dent(struct ext2_dir_entry *dir, unsigned int n_inode,
               unsigned short rec_len, unsigned char name_len,
               unsigned char file_type, const char *name) {
    // memset(dir, 0, rec_len);
    dir->inode = n_inode;
    dir->rec_len = rec_len;
    dir->name_len = name_len;
    dir->file_type = file_type;
    strncpy(dir->name, name, name_len);
}

void add_dent(int n_inode, int n_pdir_inode, const char *name, int type) {
    struct ext2_inode *inode;
    int n_block;
    int added;

    inode = locate_inode(n_inode);
    n_block = find_block_lastused(n_pdir_inode);
    added = 0;

    if (add_dent_in_block(n_inode, n_pdir_inode, name, type, n_block) < 0) {
        n_block = alloc_block_any(n_pdir_inode);
        if (!add_dent_in_block(n_inode, n_pdir_inode, name, type, n_block)) {
            added = 1;
        }
    } else {
        added = 1;
    }

    if (added) {
        ++inode->i_links_count;
    }
}

int add_dent_in_block(int n_inode, int n_pdir_inode, const char *name, int type,
                      int n_block) {
    int name_len;
    int dir_entry_len;
    struct ext2_dir_entry *dir;
    int min_rec_len, extra_len;
    int ret;

    name_len = strlen(name);
    dir_entry_len = sizeof(struct ext2_dir_entry) + get_name_len(name);
    ret = -1;

    dir = locate_block(n_block);

    for (int len = 0; len < EXT2_BLOCK_SIZE;
         len += dir->rec_len, dir = offset_ptr(dir, dir->rec_len)) {
        if (dir->rec_len == 0) {
            extra_len = EXT2_BLOCK_SIZE - len;
            if (extra_len >= dir_entry_len) {
                init_dent(dir, n_inode, extra_len, name_len, type, name);
                ret = 0;
            }
            break;
        }
        if (len + dir->rec_len == EXT2_BLOCK_SIZE) {
            min_rec_len =
                sizeof(struct ext2_dir_entry) + padding_name_len(dir->name_len);
            extra_len = dir->rec_len - min_rec_len;
            if (extra_len >= dir_entry_len) {
                dir->rec_len = min_rec_len;
                dir = offset_ptr(dir, dir->rec_len);
                init_dent(dir, n_inode, extra_len, name_len, type, name);
                ret = 0;
            }
            break;
        }
    }

    return ret;
}

void del_dent(int n_inode, int n_pdir_inode) {
    struct ext2_inode *inode;
    int n_block;
    int deleted;

    inode = locate_inode(n_inode);
    deleted = 0;

    for (int i = 0; (n_block = find_block_linear(n_pdir_inode, i)); ++i) {
        if (!del_dent_in_block(n_inode, n_pdir_inode, n_block)) {
            deleted = 1;
            break;
        }
    }

    if (deleted) {
        --inode->i_links_count;
    }
}

int del_dent_in_block(int n_inode, int n_pdir_inode, int n_block) {
    struct ext2_dir_entry *dir, *prev_dir;
    int ret;

    prev_dir = NULL;
    dir = locate_block(n_block);
    ret = -1;

    for (int len = 0; len < EXT2_BLOCK_SIZE; len += dir->rec_len,
             prev_dir = dir, dir = offset_ptr(dir, dir->rec_len)) {
        if (dir->inode == n_inode) {
            prev_dir->rec_len += dir->rec_len;
            ret = 0;
            break;
        }
    }

    return ret;
}

int iterate_dent(int n_pdir_inode, cb_iterate_dent cb) {
    int cnt;
    struct ext2_inode *pdir_inode;
    struct ext2_dir_entry *dir;
    int n_block;

    pdir_inode = locate_inode(n_pdir_inode);
    cnt = 0;

    for (int i = 0; (n_block = find_block_linear(n_pdir_inode, i)); ++i) {
        dir = locate_block(pdir_inode->i_block[i]);
        for (int len = 0; len < EXT2_BLOCK_SIZE && dir->rec_len != 0;
             len += dir->rec_len, dir = offset_ptr(dir, dir->rec_len)) {
            cnt += cb(dir);
            if (dir->name_len == 1 && !strncmp(dir->name, ".", 1)) {
                continue;
            }
            if (dir->name_len == 2 && !strncmp(dir->name, "..", 2)) {
                continue;
            }
            if (is_dent_dir(dir)) {
                cnt += iterate_dent(dir->inode, cb);
            }
        }
    }

    return cnt;
}

int find_dent_by_name(int n_pdir_inode, const char *name,
                      struct ext2_dir_entry **dent) {
    int name_len;
    struct ext2_inode *pdir_inode;
    struct ext2_dir_entry *dir;
    int n_block;

    name_len = strlen(name);
    pdir_inode = locate_inode(n_pdir_inode);

    for (int i = 0; (n_block = find_block_linear(n_pdir_inode, i)); ++i) {
        dir = locate_block(pdir_inode->i_block[i]);
        for (int len = 0; len < EXT2_BLOCK_SIZE && dir->rec_len != 0;
             len += dir->rec_len, dir = offset_ptr(dir, dir->rec_len)) {
            if (dir->inode != 0 && dir->name_len == name_len &&
                !strncmp(name, dir->name, name_len)) {
                *dent = dir;
                return dir->inode;
            }
        }
    }

    return -1;
}

int find_dent_dir_by_path(const struct path_tokens *pt) {
    int type;
    int n_inode;
    n_inode = find_dent_by_path(pt, &type);
    return n_inode < 0 ? -1 : (type == EXT2_FT_DIR ? n_inode : -1);
}

int find_dent_any_by_path(const struct path_tokens *pt) {
    int type;
    return find_dent_by_path(pt, &type);
}

int find_dent_by_path(const struct path_tokens *pt, int *type) {
    struct ext2_dir_entry *dir;
    int n_dir_inode;

    n_dir_inode = 2;
    *type = EXT2_FT_DIR;

    for (int i = 0; i < pt->num; ++i) {
        if ((n_dir_inode =
                 find_dent_by_name(n_dir_inode, pt->tokens[i], &dir)) < 0) {
            break;
        }
        *type = get_dent_type(dir);
    }

    return n_dir_inode;
}

struct ext2_dir_entry *find_deleteddent_helper(int n_pdir_inode,
                                               const char *name,
                                               struct ext2_dir_entry **prev_dir,
                                               int *rec_len) {
    int name_len;
    int dir_entry_len;
    struct ext2_inode *pdir_inode;
    struct ext2_dir_entry *dir, *try_dir;
    int min_rec_len, extra_len;
    int n_block;

    name_len = strlen(name);
    dir_entry_len = sizeof(struct ext2_dir_entry) + get_name_len(name);
    pdir_inode = locate_inode(n_pdir_inode);

    for (int i = 0; (n_block = find_block_linear(n_pdir_inode, i)); ++i) {
        dir = locate_block(pdir_inode->i_block[i]);
        for (int len = 0; len < EXT2_BLOCK_SIZE && dir->rec_len != 0;
             len += dir->rec_len, dir = offset_ptr(dir, dir->rec_len)) {
            *prev_dir = try_dir = dir;
            for (int _len = 0; _len < dir->rec_len && try_dir->rec_len != 0 &&
                               try_dir->name_len != 0;) {
                min_rec_len = sizeof(struct ext2_dir_entry) +
                              padding_name_len(try_dir->name_len);
                try_dir = offset_ptr(try_dir, min_rec_len);
                _len += min_rec_len;
                extra_len = dir->rec_len - _len;
                if (extra_len < dir_entry_len) {
                    break;
                }
                if (try_dir->inode != 0 && try_dir->rec_len >= dir_entry_len &&
                    try_dir->name_len == name_len &&
                    !strncmp(try_dir->name, name, name_len)) {
                    *rec_len = extra_len;
                    return try_dir;
                }
            }
        }
    }

    return NULL;
}

int find_deleteddent(int n_pdir_inode, const char *name, int *type) {
    int rec_len;
    struct ext2_dir_entry *dir, *prev_dir;

    if (!(dir = find_deleteddent_helper(n_pdir_inode, name, &prev_dir,
                                        &rec_len))) {
        return -1;
    }

    *type = get_dent_type(dir);

    return dir->inode;
}

void restore_deleteddent(int n_pdir_inode, const char *name) {
    int rec_len;
    struct ext2_dir_entry *dir, *prev_dir;
    struct ext2_inode *inode;

    if (!(dir = find_deleteddent_helper(n_pdir_inode, name, &prev_dir,
                                        &rec_len))) {
        return;
    }

    prev_dir->rec_len = calc_offset_ptr(dir, prev_dir);
    dir->rec_len = rec_len;

    inode = locate_inode(dir->inode);
    ++inode->i_links_count;
}

/* ------------------- manipulate disk pointer ------------------- */

void *offset_ptr(void *ptr, int dist) { return (char *)ptr + dist; }

int calc_offset_ptr(void *ptr1, void *ptr2) {
    return (char *)ptr1 - (char *)ptr2;
}

struct ext2_inode *locate_inode(int n_inode) {
    return inode_tbl + (n_inode - 1);
}

void *locate_block(int n_block) { return disk + EXT2_BLOCK_SIZE * n_block; }

/* ------------------- dir_entry name length ------------------- */

int get_name_len(const char *name) { return padding_name_len(strlen(name)); }

int padding_name_len(int name_len) {
    return (name_len / 4 * 4) + (name_len % 4 ? 4 : 0);
}

/* ------------------- block/inode bitmap op ------------------- */

int chk_bit(int bit, const unsigned char *bitmap) {
    int i = bit / 8, j = bit % 8;
    return (bitmap[i] >> j) & 1UL ? 1 : 0;
}
void set_bit(int bit, unsigned char *bitmap) {
    int i = bit / 8, j = bit % 8;
    bitmap[i] |= 1UL << j;
}
void clr_bit(int bit, unsigned char *bitmap) {
    int i = bit / 8, j = bit % 8;
    bitmap[i] &= ~(1UL << j);
}

int chk_inodebit(int n_inode) { return chk_bit(n_inode - 1, inode_bmp); }
int chk_blockbit(int n_block) { return chk_bit(n_block - 1, block_bmp); }
void set_inodebit(int n_inode) { set_bit(n_inode - 1, inode_bmp); }
void set_blockbit(int n_block) { set_bit(n_block - 1, block_bmp); }
void clr_inodebit(int n_inode) { clr_bit(n_inode - 1, inode_bmp); }
void clr_blockbit(int n_block) { clr_bit(n_block - 1, block_bmp); }

/* ------------------- find free block/inode ------------------- */

int find_free_block() {
    for (int n_block = 1; n_block <= sb->s_blocks_count; ++n_block) {
        if (!chk_blockbit(n_block)) {
            return n_block;
        }
    }
    return -1;
}
int find_free_inode() {
    for (int n_inode = EXT2_GOOD_OLD_FIRST_INO; n_inode <= sb->s_inodes_count;
         ++n_inode) {
        if (!chk_inodebit(n_inode)) {
            return n_inode;
        }
    }
    return -1;
}

int alloc_block() {
    int n_block;
    if (sb->s_free_blocks_count == 0 || (n_block = find_free_block()) < 0) {
        fprintf(stderr, "no free block found\n");
        exit(ENOSPC);
    }
    set_blockbit(n_block);
    --sb->s_free_blocks_count;
    --gd->bg_free_blocks_count;
    init_block(n_block);
    return n_block;
}
int alloc_inode() {
    int n_inode;
    if (sb->s_free_inodes_count == 0 || (n_inode = find_free_inode()) < 0) {
        fprintf(stderr, "no free inode found\n");
        exit(ENOSPC);
    }
    set_inodebit(n_inode);
    --sb->s_free_inodes_count;
    --gd->bg_free_inodes_count;
    init_inode(n_inode);
    return n_inode;
}

void restore_block(int n_block) {
    set_blockbit(n_block);
    --sb->s_free_blocks_count;
    --gd->bg_free_blocks_count;
}
void restore_inode(int n_inode) {
    set_inodebit(n_inode);
    --sb->s_free_inodes_count;
    --gd->bg_free_inodes_count;
}

void free_block(int n_block) {
    clr_blockbit(n_block);
    ++sb->s_free_blocks_count;
    ++gd->bg_free_blocks_count;
}
void free_inode(int n_inode) {
    clr_inodebit(n_inode);
    ++sb->s_free_inodes_count;
    ++gd->bg_free_inodes_count;
}

void init_block(int n_block) {
    memset(locate_block(n_block), 0, EXT2_BLOCK_SIZE);
}
void init_inode(int n_inode) {
    memset(locate_inode(n_inode), 0, sizeof(struct ext2_inode));
}

int alloc_block_any(int n_inode) {
    int n_block;
    struct ext2_inode *inode;
    int is_allocated;
    unsigned int *block;

    inode = locate_inode(n_inode);

    is_allocated = 0;

    for (int i = 0; i < 12; ++i) {
        if (!inode->i_block[i]) {
            inode->i_block[i] = n_block = alloc_block();
            is_allocated = 1;
            break;
        }
    }

    if (!is_allocated) {
        if (!inode->i_block[12]) {
            inode->i_block[12] = alloc_block();
        }
        block = locate_block(inode->i_block[12]);
        for (int i = 0; i < EXT2_BLOCK_SIZE / 4; ++i) {
            if (!block[i]) {
                block[i] = n_block = alloc_block();
                break;
            }
        }
    }

    inode->i_size += EXT2_BLOCK_SIZE;
    inode->i_blocks += EXT2_BLOCK_SIZE / 512;

    return n_block;
}

int alloc_inode_w_mode(int mode) {
    int n_inode;
    struct ext2_inode *inode;

    n_inode = alloc_inode();

    inode = locate_inode(n_inode);
    inode->i_mode = mode;
    inode->osd1 = 1;

    return n_inode;
}

int alloc_inode_dir() { return alloc_inode_w_mode(EXT2_S_IFDIR); }
int alloc_inode_reg() { return alloc_inode_w_mode(EXT2_S_IFREG); }
int alloc_inode_sym() { return alloc_inode_w_mode(EXT2_S_IFLNK); }

/* ------------------- check type ------------------- */

int get_inode_type(int n_inode) {
    struct ext2_inode *inode;
    inode = locate_inode(n_inode);
    return inode->i_mode & 0xF000UL;
}
int is_inode_dir(int n_inode) {
    return get_inode_type(n_inode) == EXT2_S_IFDIR ? 1 : 0;
}
int is_inode_reg(int n_inode) {
    return get_inode_type(n_inode) == EXT2_S_IFREG ? 1 : 0;
}
int is_inode_sym(int n_inode) {
    return get_inode_type(n_inode) == EXT2_S_IFLNK ? 1 : 0;
}

int get_dent_type(const struct ext2_dir_entry *dent) {
    return dent->file_type & 0x7UL;
}
int is_dent_dir(const struct ext2_dir_entry *dent) {
    return get_dent_type(dent) == EXT2_FT_DIR;
}
int is_dent_reg(const struct ext2_dir_entry *dent) {
    return get_dent_type(dent) == EXT2_FT_REG_FILE;
}
int is_dent_sym(const struct ext2_dir_entry *dent) {
    return get_dent_type(dent) == EXT2_FT_SYMLINK;
}

void set_dent_type(struct ext2_dir_entry *dent, int type) {
    dent->file_type = (dent->file_type & ~0x7UL) | type;
}
void set_dent_dir(struct ext2_dir_entry *dent) {
    set_dent_type(dent, EXT2_FT_DIR);
}
void set_dent_reg(struct ext2_dir_entry *dent) {
    set_dent_type(dent, EXT2_FT_REG_FILE);
}
void set_dent_sym(struct ext2_dir_entry *dent) {
    set_dent_type(dent, EXT2_FT_SYMLINK);
}