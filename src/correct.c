#include <dirent.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>

static const size_t NUM = 5;

struct item {
    int distance;
    char *command;
};

/*
 * declare array that will be used to compute string
 * distances later on (instead of allocating every time)
 */
static int array[NAME_MAX + 1][NAME_MAX + 1];

static inline int
min2(int a, int b)
{
    return a < b ? a : b;
}

static inline int
min3(int a, int b, int c)
{
    return min2(min2(a, b), c);
}

/*
 * translated from pseudocode found here :
 * https://en.wikipedia.org/wiki/Damerau%E2%80%93Levenshtein_distance
 */
static int
string_distance(const char *str1, const char *str2)
{
    size_t len1;
    size_t len2;

    len1 = strnlen(str1, NAME_MAX + 1);
    len2 = strnlen(str2, NAME_MAX + 1);

    for (size_t i = 0; i <= len1; ++i)
        memset(array[i], 0, len2 + 1);

    for (size_t i = 0; i <= len1; ++i) array[i][0] = (int)i;
    for (size_t i = 0; i <= len2; ++i) array[0][i] = (int)i;

    for (size_t i = 1; i <= len1; ++i)
        for (size_t j = 1; j <= len2; ++j) {
            unsigned short cost = str1[i - 1] == str2[j - 1] ? 0 : 1;

            array[i][j] = min3(
                array[i - 1][    j] + 1,
                array[    i][j - 1] + 1,
                array[i - 1][j - 1] + cost);

            if (i > 1 && j > 1
                && str1[i - 1] == str2[j - 2]
                && str2[j - 1] == str1[i - 2])
                array[i][j] = min2(
                    array[    i][    j],
                    array[i - 2][j - 2] + 1);
        }

    return array[len1][len2];
}

static noreturn void
usage(char *name)
{
    fprintf(stderr, "usage : %s <command>\n", basename(name));

    exit(1);
}

static void *
allocate(size_t size)
{
    void *ptr;

    if (! (ptr = malloc(size))) {
        fprintf(stderr, "error : failed to allocate memory\n");

        exit(1);
    }

    return ptr;
}

static void *
reallocate(void *old, size_t size)
{
    void *new;

    if (! (new = realloc(old, size))) {
        fprintf(stderr, "error : failed to reallocate memory\n");

        exit(1);
    }

    return new;
}

static char *
copy_string(const char *str)
{
    char *cpy;

    {
        size_t len;

        len = strnlen(str, PATH_MAX);
        cpy = allocate((len + 1) * sizeof(*cpy));
        strncpy(cpy, str, len);

        /* fix string */
        cpy[len] = 0;
    }

    return cpy;
}

static char **
get_path_dirs(size_t *size)
{
    char **dirs;

    {
        char *path;

        if (! (path = getenv("PATH"))) {
            fprintf(stderr, "error : failed to get '$PATH' from env\n");

            exit(1);
        }

        char *tok;

        if (! (tok = strtok(path, ":"))) {
            fprintf(stderr, "error : failed to parse '$PATH'\n");

            exit(1);
        }

        size_t allocated = 2;
        size_t assigned  = 0;

        dirs = allocate(allocated * sizeof(*dirs));

        while (tok) {
            dirs[assigned] = copy_string(tok);

            if (++assigned == allocated)
                dirs = reallocate(dirs, (allocated = allocated * 3 / 2) * sizeof(*dirs));

            tok = strtok(NULL, ":");
        }

        *size = assigned;
    }

    return dirs;
}

static void
cleanup_path_dirs(char **dirs, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        free(dirs[i]);

    free(dirs);
}

static void
close_dir(const char *path, DIR *dir)
{
    if (closedir(dir) != 0) {
        fprintf(stderr, "error : failed to close '%s'\n", path);

        exit(1);
    }
}

static struct item *
get_list_commands(size_t *size, const char *command)
{
    struct item *list;

    {
        size_t filter;

        filter = strnlen(command, NAME_MAX);

        size_t allocated = 2;
        size_t assigned  = 0;

        list = allocate(allocated * sizeof(*list));

        char **path_dirs;
        size_t path_size;

        path_dirs = get_path_dirs(&path_size);

        char path[PATH_MAX] = {0};

        for (size_t i = 0; i < path_size; ++i) {
            DIR *dir;
            struct dirent *content;

            /* skip directories that we can't open */
            if (! (dir = opendir(path_dirs[i])))
                continue;

            while ((content = readdir(dir))) {
                /* skip sub-directories */
                if (content->d_type == DT_DIR)
                    continue;

                size_t len;

                len = strnlen(content->d_name, NAME_MAX + 1);

                /* skip files with names too long / short */
                if (len > filter + 2 || (long)len < (long)filter - 2)
                    continue;

                if (snprintf(path, PATH_MAX, "%s/%s", path_dirs[i], content->d_name) < 0) {
                    fprintf(stderr, "error : failed to build path to '%s'\n", content->d_name);

                    exit(1);
                }

                /* skip files that aren't executable */
                if (access(path, X_OK) != 0)
                    continue;

                list[assigned].command = copy_string(content->d_name);
                list[assigned].distance = string_distance(content->d_name, command);

                if (++assigned == allocated)
                    list = reallocate(list, (allocated = allocated * 3 / 2) * sizeof(*list));
            }

            close_dir(path_dirs[i], dir);
        }

        *size = assigned;
        cleanup_path_dirs(path_dirs, path_size);
    }

    return list;
}

static void
cleanup_list_commands(struct item *list, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        free(list[i].command);

    free(list);
}

static int
compare_item(const void *ptr1, const void *ptr2)
{
    int comp;

    /* sort by string distance first */
    comp = (*(struct item const *)ptr1).distance -
           (*(struct item const *)ptr2).distance;

    /* sort by alphabetical order second */
    return comp != 0
        ? comp
        : strncmp(
            (*(struct item const *)ptr1).command,
            (*(struct item const *)ptr2).command,
            NAME_MAX + 1);
}

int
main(int argc, char **argv)
{
    if (argc != 2)
        usage(argv[0]);

    size_t size;
    struct item *list;

    list = get_list_commands(&size, argv[1]);

    qsort(list, size, sizeof(*list), compare_item);

    for (size_t i = 0; i < NUM && i < size; ++i)
        puts(list[i].command);

    cleanup_list_commands(list, size);

    return 0;
}
