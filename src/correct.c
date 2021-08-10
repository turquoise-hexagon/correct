#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#define ERROR(status, ...) {      \
    fprintf(stderr, __VA_ARGS__); \
                                  \
    exit(status);                 \
}

#define LEVENSHTEIN_MAX 1024

/* number of suggestions to print */
static const size_t NB_CANDIDATES = 5;

struct item {
    char *str;
    size_t len;
    int dist;
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
distance(struct item a, struct item b)
{
    static int array[LEVENSHTEIN_MAX + 1][LEVENSHTEIN_MAX + 1] = {{0}};

    size_t len_a;
    size_t len_b;

    len_a = min2(a.len, LEVENSHTEIN_MAX);
    len_b = min2(b.len, LEVENSHTEIN_MAX);

    for (size_t i = 0; i <= len_a; ++i) array[i][0] = (int)i;
    for (size_t i = 0; i <= len_b; ++i) array[0][i] = (int)i;

    for (size_t i = 1; i <= len_a; ++i)
        for (size_t j = 1; j <= len_b; ++j) {
            int cost = a.str[i - 1] != b.str[j - 1];

            array[i][j] = min3(
                array[i - 1][j    ] + 1,
                array[i    ][j - 1] + 1,
                array[i - 1][j - 1] + cost
            );

            if (i > 1 && j > 1
                && a.str[i - 1] == b.str[j - 2]
                && a.str[i - 2] == b.str[j - 1]
            )
                array[i][j] = min2(
                    array[i    ][j    ],
                    array[i - 2][j - 2] + 1
                );
        }

    return array[len_a][len_b];
}

static int
compare(const void *a, const void *b)
{
    const struct item *ptr_a = &(*(struct item const *)a);
    const struct item *ptr_b = &(*(struct item const *)b);

    /* sort by string distance first */
    int comp = ptr_a->dist -
               ptr_b->dist;

    /* sort alphabetically second */
    return comp != 0
        ? comp
        : strncmp(
            ptr_a->str,
            ptr_b->str,
            ptr_a->len > ptr_b->len
                ? ptr_a->len
                : ptr_b->len
        );
}

static struct item *
get_input_list(struct item arg, size_t *len)
{
    struct item *input;

    {
        size_t alloc  = 2;
        size_t assign = 0;

        if (!(input = malloc(alloc * sizeof(*input))))
            ERROR(1, "error : failed to allocate '%lu' bytes of memory\n",
                alloc * sizeof(*input));

        char buf[LINE_MAX] = {0};

        while (fgets(buf, sizeof(buf), stdin)) {
            struct item *cur = &input[assign];

            cur->len = strnlen(buf, sizeof(buf)) - 1;

            /* fix string */
            buf[cur->len] = 0;

            if (!(cur->str = strndup(buf, cur->len)))
                ERROR(1, "error : failed to copy string of length '%lu'\n", cur->len);

            cur->dist = distance(arg, *cur); ++assign;

            /* reallocate buffer if necessary */
            if (assign == alloc) {
                alloc = alloc * 3 / 2;

                if (!(input = realloc(input, alloc * sizeof(*input))))
                    ERROR(1, "error : failed to reallocate '%lu' bytes of memory\n",
                        alloc * sizeof(*input));
            }
        }

        *len = assign;
    }

    return input;
}

int
main(int argc, char **argv)
{
    if (argc != 2)
        usage(argv[0]);

    struct item arg;
    arg.str = argv[1];
    arg.len = strnlen(arg.str, LINE_MAX);

    size_t len;
    struct item *input;
    input = get_input_list(arg, &len);

    qsort(input, len, sizeof(*input), compare);

    for (size_t i = 0; i < NB_CANDIDATES && i < len; ++i)
        puts(input[i].str);

    for (size_t i = 0; i < len; ++i)
        free(input[i].str);

    free(input);

    return 0;
}
