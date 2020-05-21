#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned array[LINE_MAX][LINE_MAX];

struct item {
    unsigned dist;
    char *word;
};

static void
usage(char *name)
{
    fprintf(stderr, "usage : %s <string> <complist>\n", basename(name));

    exit(EXIT_FAILURE);
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

static inline int
comparison(const void *a, const void *b)
{
    return (*(struct item *)a).dist - (*(struct item *)b).dist;
}

static unsigned
distance(
    const char *str1,
    const char *str2
)
{
    /* init matrix */
    size_t len1 = strnlen(str1, LINE_MAX);
    size_t len2 = strnlen(str2, LINE_MAX);

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
                   i > 1
                && j > 1
                && str1[i - 1] == str2[j - 2]
                && str2[j - 1] == str1[i - 2]
               )
                array[i][j] = min2(array[i][j], array[i - 2][j - 2] + 1);
        }

    return array[len1][len2];
}

int
main(int argc, char **argv)
{
    if (argc < 3)
        usage(argv[0]);

    struct item *array = malloc(argc * sizeof *array);

    if (array == NULL)
        errx(EXIT_FAILURE, "failed to allocate memory");

    unsigned cnt = 0;
    size_t len = strnlen(argv[1], LINE_MAX);

    for (unsigned i = 2; i < (unsigned)argc; ++i) {
        size_t tmp = strnlen(argv[i], LINE_MAX);

        /* don't compute strings that are too long / short */
        if (tmp > len + 2 || (long)tmp < (long)len - 2)
            continue;

        array[cnt].word = argv[i];
        array[cnt].dist = distance(argv[1], argv[i]);
        ++cnt;
    }

    qsort(array, cnt, sizeof *array, comparison);

    for (unsigned i = 0; i < 5 && i < cnt; ++i)
        printf("%s\n", array[i].word);

    free(array);

    return EXIT_SUCCESS;
}
