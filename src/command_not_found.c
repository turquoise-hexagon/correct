#include <dirent.h>
#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

/*
 * structure used to sort commands by distance
 */
struct item {
    char *name;
    unsigned dist;
};

/*
 * use a static array to avoid reallocating memory
 */
static char array[LINE_MAX][LINE_MAX];

/*
 * helper functions
 */
static void
usage(char *name)
{
    fprintf(stderr, "usage : %s [string]\n", basename(name));

    exit(EXIT_FAILURE);
}

static inline int
compitem(const void *ptr1, const void *ptr2)
{
    return (*(struct item *)ptr1).dist
         - (*(struct item *)ptr2).dist;
}

static inline int
compstring(const void *ptr1, const void *ptr2)
{
    return strncmp(
        *(char * const *)ptr1,
        *(char * const *)ptr2,
        LINE_MAX
    );
}

static inline unsigned
min2(unsigned a, unsigned b)
{
    return a < b ? a : b;
}

static inline unsigned
min3(unsigned a, unsigned b, unsigned c)
{
    return min2(min2(a, b), c);
}

static void *
allocate(size_t size)
{
    void *ptr = malloc(size);

    if (ptr == NULL)
        errx(EXIT_SUCCESS, "failed to allocate memory");

    return ptr;
}

static DIR *
open_dir(const char *path)
{
    DIR *dir = opendir(path);

    if (dir == NULL)
        errx(EXIT_FAILURE, "failed to open '%s'", path);

    return dir;
}

static void
close_dir(DIR *dir, const char *path)
{
    if (closedir(dir) != 0)
        errx(EXIT_FAILURE, "failed to close '%s'", path);
}

static int
is_executable(const char *path)
{
    return access(path, X_OK);
}

/*
 * main functions
 */
static char **
commands_list(char *path, size_t filter, size_t *num)
{
    /* split path */
    char *tok = strtok(path, ":");

    if (tok == NULL)
        errx(EXIT_FAILURE, "failed to parse '$PATH'");

    DIR *dir;
    struct dirent *content;

    size_t cur = 0;
    size_t tot = 1;
    char **list = allocate(tot * sizeof *list);

    size_t len;
    char file_path[256];

    while (tok != NULL) {
        dir = open_dir(tok);

        while ((content = readdir(dir)) != NULL) {
            /* skip directories */
            if (content->d_type == DT_DIR)
                continue;

            len = strnlen(content->d_name, LINE_MAX);

            /* skip executables with a name that is too long / short */
            if (len > filter + 2 && (long)len < (long)filter - 2)
                continue;

            if (snprintf(file_path, sizeof file_path, "%s/%s", tok, content->d_name) < 0)
                errx(EXIT_FAILURE, "failed to build path to '%s'", content->d_name);

            if (is_executable(file_path) == 0) {
                list[cur] = allocate(LINE_MAX * sizeof *list[cur]);

                strncpy(list[cur], content->d_name, LINE_MAX * sizeof *list[cur]);

                /* allocate more space if needed */
                if (++cur == tot)
                    if ((list = realloc(list, (tot *= 2) * sizeof *list)) == NULL)
                        errx(EXIT_FAILURE, "failed to allocate memory");
            }
        }

        close_dir(dir, tok);

        tok = strtok(NULL, ":");
    }

    *num = cur;
    return list;
}

static unsigned
distance(const char *str1, const char *str2)
{
    /* get dimensions */
    size_t len1 = strnlen(str1, LINE_MAX);
    size_t len2 = strnlen(str2, LINE_MAX);

    /* initialize matrix */
    for (size_t i = 0; i <= len1; ++i)
        for (size_t j = 0; j <= len2; ++j)
            array[i][j] = 0;

    for (size_t i = 0; i <= len1; ++i) array[i][0] = i;
    for (size_t j = 0; j <= len2; ++j) array[0][j] = j;

    unsigned cost = 0;

    /* compute string distance */
    for (size_t i = 1; i <= len1; ++i)
        for (size_t j = 1; j <= len2; ++j) {
            cost = str1[i - 1] == str2[j - 1] ? 0 : 1;

            array[i][j] = min3(
                array[i - 1][j] + 1,
                array[i][j - 1] + 1,
                array[i - 1][j - 1] + cost
            );

            if (
                i > 1 && j > 1
                && str1[i - 1] == str2[j - 2]
                && str2[j - 1] == str1[i - 2]
               )
                array[i][j] = min2(array[i][j], array[i - 2][j - 2] + 1);
        }

    return array[len1][len2];
}

static void
init_item(struct item *var, const char *name, const char *cmd)
{
    var->dist = distance(cmd, name);
    var->name = allocate(LINE_MAX * sizeof *var->name);

    strncpy(var->name, name, LINE_MAX * sizeof *var->name);
}

int
main(int argc, char **argv)
{
    if (argc != 2)
        usage(argv[0]);

    char *path = getenv("PATH");

    if (path == NULL)
        errx(EXIT_FAILURE, "failed to get '$PATH'");

    /* build a list of commands */
    size_t num;
    size_t len = strnlen(argv[1], LINE_MAX);

    char **list = commands_list(path, len, &num);

    /* sort it alphabetically */
    qsort(list, num, sizeof *list, compstring);

    /* build a list of string distances */
    struct item *items = allocate(num * sizeof *items);

    for (size_t i = 0; i < num; ++i) {
        init_item(&items[i], list[i], argv[1]);

        free(list[i]);
    }

    free(list);

    /* sort it numerically */
    qsort(items, num, sizeof *items, compitem);

    /* print suggestions */
    printf("no command '%s' found, did you mean :\n", argv[1]);

    for (unsigned i = 0; i < NUM && i < num; ++i)
        puts(items[i].name);

    /* free remaining memory */
    for (size_t i = 0; i < num; ++i)
        free(items[i].name);

    free(items);

    return EXIT_SUCCESS;
}
