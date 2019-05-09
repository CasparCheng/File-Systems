#include "ext2_pathtokens.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* ------------------- manipulate path ------------------- */

char *get_path_tokens_last(const struct path_tokens *pt) {
    char *token = NULL;
    if (pt->num > 0)
        token = pt->tokens[pt->num - 1];
    return token;
}

void print_path_tokens(const struct path_tokens *pt) {
    printf("---------- path tokens ---------\n");
    for (int i = 0; i < pt->num; ++i)
        printf("[%d] %s\n", i, pt->tokens[i]);
    printf("--------------------------------\n");
}

struct path_tokens *create_path_tokens(const char *path) {
    char *path_dup;
    char *token;
    int num;
    char **tokens;
    struct path_tokens *pt;

    if (!(path_dup = strdup(path))) {
        perror("malloc");
        exit(ENOMEM);
    }

    num = 0;
    tokens = NULL;
    token = strtok(path_dup, "/");
    while (token) {
        ++num;
        if (!(tokens = realloc(tokens, sizeof(char *) * num))) {
            perror("realloc");
            exit(ENOMEM);
        }
        if (!(tokens[num - 1] = strdup(token))) {
            perror("strdup");
            exit(ENOMEM);
        }
        token = strtok(NULL, "/");
    }

    free(path_dup);

    if (!(pt = malloc(sizeof(struct path_tokens)))) {
        perror("malloc");
        exit(ENOMEM);
    }

    pt->num = num;
    pt->tokens = tokens;

    return pt;
}

void destroy_path_tokens(struct path_tokens *pt) {
    if (pt) {
        if (pt->num > 0) {
            for (int i = 0; i < pt->num; ++i)
                free(pt->tokens[i]);
            free(pt->tokens);
        }
        free(pt);
    }
}

void add_path_token(struct path_tokens *pt, const char *token) {
    ++pt->num;
    if (!(pt->tokens = realloc(pt->tokens, sizeof(char *) * pt->num))) {
        perror("realloc");
        exit(ENOMEM);
    }
    if (!(pt->tokens[pt->num - 1] = strdup(token))) {
        perror("strdup");
        exit(ENOMEM);
    }
}

void pop_path_token(struct path_tokens *pt) {
    if (pt->num > 0) {
        --pt->num;
        free(pt->tokens[pt->num]);
        if (pt->num == 0) {
            free(pt->tokens);
        } else if (!(pt->tokens = realloc(pt->tokens, sizeof(char *) * pt->num))) {
            perror("realloc");
            exit(ENOMEM);
        }
    }
}