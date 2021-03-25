#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

/* number of suggestions to print */
static const unsigned NUM = 5;

#define LEVENSHTEIN_MAX 1024

#define ERROR(status, ...) {      \
    fprintf(stderr, __VA_ARGS__); \
                                  \
    exit(status);                 \
}

struct item {
    int distance; char line[LINE_MAX];
};

static noreturn void
usage(char *name)
{
    ERROR(1, "usage : <list> | %s <string>\n", basename(name));
}

static inline int
min2(int a, int b)
{
    return a < b ? a : b;
}

static inline int
min3(int a, int b, int c)
{
    return min2(a, min2(b, c));
}

/* https://en.wikipedia.org/wiki/damerau%e2%80%93levenshtein_distance */
static int
string_distance(const char *a, const char *b)
{
    static int array[LEVENSHTEIN_MAX + 1][LEVENSHTEIN_MAX + 1] = {{0}};

    size_t len_a;
    size_t len_b;

    len_a = strnlen(a, LEVENSHTEIN_MAX);
    len_b = strnlen(b, LEVENSHTEIN_MAX);

    for (size_t i = 0; i <= len_a; ++i) array[i][0] = (int)i;
    for (size_t i = 0; i <= len_b; ++i) array[0][i] = (int)i;

    for (size_t i = 1; i <= len_a; ++i)
        for (size_t j = 1; j <= len_b; ++j) {
            int cost = a[i - 1] == b[j - 1] ? 0 : 1;

            array[i][j] = min3(
                array[i - 1][    j] + 1,
                array[    i][j - 1] + 1,
                array[i - 1][j - 1] + cost);

            if (i > 1 && j > 1
                && a[i - 1] == b[j - 2]
                && a[i - 2] == b[j - 1])
                array[i][j] = min2(
                    array[    i][    j],
                    array[i - 2][j - 2] + 1);
        }

    return array[len_a][len_b];
}

static struct item *
read_input(size_t *size)
{
    struct item *input;

    {
        size_t alloc  = 2;
        size_t assign = 0;

        if (!(input = malloc(alloc * sizeof(*input))))
            return NULL;

        for (;;) {
            struct item *tmp = &input[assign];

            if (!fgets(tmp->line, sizeof(tmp->line), stdin))
                break;

            /* fix string */
            tmp->line[strnlen(tmp->line, sizeof(tmp->line)) - 1] = 0;

            if (++assign == alloc)
                if (!(input = realloc(input, (alloc = alloc * 3 / 2) * sizeof(*input))))
                    return NULL;
        }

        *size = assign;
    }

    return input;
}

static int
comparison(const void *a, const void *b)
{
    const struct item *tmp_a = &(*(struct item const *)a);
    const struct item *tmp_b = &(*(struct item const *)b);

    /* sort by string distance first */
    int comp = tmp_a->distance -
               tmp_b->distance;

    /* sort alphabetically second */
    return comp != 0
        ? comp
        : strncmp(
            tmp_a->line,
            tmp_b->line,
            sizeof(tmp_a->line));
}

int
main(int argc, char **argv)
{
    if (argc != 2)
        usage(argv[0]);

    size_t size;
    struct item *input;

    if (!(input = read_input(&size)))
        ERROR(1, "error : failed to acquire input from stdin\n");

    for (size_t i = 0; i < size; ++i) {
        struct item *tmp = &input[i];

        tmp->distance = string_distance(argv[1], tmp->line);
    }

    qsort(input, size, sizeof(*input), comparison);

    for (size_t i = 0; i < NUM && i < size; ++i) {
        struct item *tmp = &input[i];

        puts(tmp->line);
    }

    free(input);
    return 0;
}
