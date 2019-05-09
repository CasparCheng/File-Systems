#ifndef _EXT2_PATHTOKENS_
#define _EXT2_PATHTOKENS_

struct path_tokens {
    char **tokens;
    int num;
};

char *get_path_tokens_last(const struct path_tokens *pt);
void print_path_tokens(const struct path_tokens *pt);
struct path_tokens *create_path_tokens(const char *path);
void destroy_path_tokens(struct path_tokens *pt);
void add_path_token(struct path_tokens *pt, const char *token);
void pop_path_token(struct path_tokens *pt);

#endif /* _EXT2_PATHTOKENS_ */
