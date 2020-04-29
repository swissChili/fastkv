#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include "fastkv.h"

int main(int argc, char **argv)
{
    char *q = NULL;
    if (argc == 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        puts("FastKV help");
        puts("Written by swissChili");
        puts("");
        puts("Usage:");
        puts("\tfastkv [query]");
        puts("\t<stdin>\tThe file to parse");
        puts("\t<query>\tThe query. Format key.subkey.value");
    }
    else if (argc == 2)
    {
        q = argv[1];
    }

    FILE *in = stdin;
    fseek(in, 0, SEEK_END);
    uint64_t size = ftell(in);
    fseek(in, 0, SEEK_SET);

    char *text = malloc(size + 1);
    (void)fread(text, 1, size, in);
    text[size] = 0;

    uint64_t i = 0;

    struct timeval start, end, printed;
    gettimeofday(&start, NULL);
    item_t parsed = parse(text, &i, size);
    gettimeofday(&end, NULL);
    
    if (q)
    {
        item_t result = query(parsed, q);
        if (result.type == TYPE_ERROR)
        {
            fprintf(stderr, "The query failed\n");
            return 1;
        }
        else if (result.type == TYPE_STRING)
        {
            printf("%s\n", (char *)result.pointer);
            return 0;
        }
        else if (result.type == TYPE_OBJECT)
        {
            printitem(result, 0);
            return 0;
        }
        fprintf(stderr, "The result type is something else. How did this happen?\n");
        return 2;
    }

#ifndef NDEBUG
    fflush(stderr);
    printitem(parsed, 0);
    fflush(stdout);
#endif

    uint64_t microsecs =
            (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;

    printf("Parsing took: %lu microseconds\n", microsecs);
    printf("That means %lu mb/second\n", size / microsecs);

    freeitem(parsed);
    free(text);
    return 0;
}
