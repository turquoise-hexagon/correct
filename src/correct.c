#include <dirent.h>
#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

static void
usage(char *name)
{
    fprintf(stderr, "usage : %s <string>\n", basename(name));

    exit(1);
}

/*
 * small structure used to sort commands by string distance
 */
struct item {
    char *name;
    int  distance;
};

/*
 * use a static array to avoid reallocating memory all the time
 */
static int array[NAME_MAX + 1][NAME_MAX + 1];

/*
 * helper functions
 */
static inline int
compitem(const void *ptr1, const void *ptr2)
{
    return (*(struct item const *)ptr1).distance -
           (*(struct item const *)ptr2).distance;
}

static inline int
compstring(const void *ptr1, const void *ptr2)
{
    return strncmp(
        *(char * const *)ptr1,
        *(char * const *)ptr2,
        NAME_MAX + 1
    );
}

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

static inline void *
allocate(size_t size)
{
    void *ptr;

    if ((ptr = malloc(size)) == NULL)
        errx(1, "failed to allocate memory");

    return ptr;
}

static void
close_dir(const char *path, DIR *dir)
{
    if (closedir(dir) != 0)
        errx(1, "failed to close '%s'", path);
}

/*
 * main functions
 */
static char **
build_list_commands(char *user_path, size_t *number_commands,
                       size_t length_filter)
{
    /* split path on ':' */
    char *path_dir;

    if ((path_dir = strtok(user_path, ":")) == NULL)
        errx(1, "failed to parse '$PATH'");

    DIR *dir;
    struct dirent *dir_content;

    size_t length;
    size_t number_lines = 0;
    size_t allocated_lines = 1;

    char **list_commands;
    char file_path[PATH_MAX] = {0};

    list_commands = allocate(allocated_lines * sizeof(*list_commands));

    while (path_dir != NULL) {
        /* skip non existing directories */
        if ((dir = opendir(path_dir)) == NULL)
            continue;

        while ((dir_content = readdir(dir)) != NULL) {
            /* skip sub directories */
            if (dir_content->d_type == DT_DIR)
                continue;

            length = strnlen(dir_content->d_name, NAME_MAX + 1);

            /* skip files with names too long / too short */
            if (length > length_filter + 2 ||
                   (long)length < (long)length_filter - 2)
                continue;

            if (snprintf(file_path, sizeof(file_path),
                   "%s/%s", path_dir, dir_content->d_name) < 0)
                errx(1, "failed to build path to '%s'", dir_content->d_name);

            /* skip non executables */
            if (access(file_path, X_OK) != 0)
                continue;

            list_commands[number_lines] = allocate((NAME_MAX + 1) *
                sizeof(*list_commands[number_lines]));

            strncpy(list_commands[number_lines], dir_content->d_name, NAME_MAX + 1);

            if (++number_lines == allocated_lines)
                if ((list_commands = realloc(list_commands,
                        (allocated_lines *= 2) * sizeof(*list_commands))) == NULL)
                    errx(1, "failed to allocate memory");
        }

        close_dir(path_dir, dir);
        path_dir = strtok(NULL, ":");
    }

    *number_commands = number_lines;
    return list_commands;
}

/*
 * function translated from pseudocode from :
 * https://en.wikipedia.org/wiki/Damerau%E2%80%93Levenshtein_distance
 */
static int
string_distance(const char *str1, const char *str2)
{
    /* get strings dimensions */
    size_t length1;
    size_t length2;

    length1 = strnlen(str1, NAME_MAX + 1);
    length2 = strnlen(str2, NAME_MAX + 1);

    /* initialize array */
    for (size_t i = 0; i <= length1; ++i)
        memset(array[i], 0, length2 + 1);

    for (size_t i = 0; i <= length1; ++i) array[i][0] = (int)i;
    for (size_t i = 0; i <= length2; ++i) array[0][i] = (int)i;

    /* compute string distance */
    unsigned short cost = 0;

    for (size_t i = 1; i <= length1; ++i)
        for (size_t j = 1; j <= length2; ++j) {
            cost = str1[i - 1] == str2[j - 1] ? 0 : 1;

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

    return array[length1][length2];
}

static void
init_item(struct item *var, const char *name,
              const char *str)
{
    var->distance = string_distance(str, name);
    var->name = allocate((NAME_MAX + 1) * sizeof(*var->name));

    strncpy(var->name, name, NAME_MAX + 1);
}

int
main(int argc, char **argv)
{
    if (argc != 2)
        usage(argv[0]);

    char *user_path;

    if ((user_path = getenv("PATH")) == NULL)
        errx(1, "failed to get '$PATH'");

    /* build a list of commands*/
    char **list_commands;
    size_t length_filter;
    size_t number_commands;

    length_filter = strnlen(argv[1], NAME_MAX + 1);

    list_commands = build_list_commands(user_path,
        &number_commands, length_filter);

    /* sort list of commands alphabetically */
    qsort(list_commands, number_commands,
        sizeof(*list_commands), compstring);

    /* build a list of corresponding string distances */
    struct item *list_distances;

    list_distances = allocate(number_commands * sizeof(*list_distances));

    for (size_t i = 0; i < number_commands; ++i) {
        init_item(&list_distances[i], list_commands[i], argv[1]);

        free(list_commands[i]);
    }

    free(list_commands);

    /* sort list of string distances numerically */
    qsort(list_distances, number_commands,
        sizeof(*list_distances), compitem);

    /* print suggestions */
    for (size_t i = 0; i < NUM && i < number_commands; ++i)
        puts(list_distances[i].name);

    /* free remaining memory */
    for (size_t i = 0; i < number_commands; ++i)
        free(list_distances[i].name);

    free(list_distances);

    return 0;
}
