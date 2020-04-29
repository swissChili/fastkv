#include "fastkv.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <endian.h>

#ifdef NDEBUG
#define INLINE __attribute__((flatten))
#else
#define INLINE // __attribute__((flatten))
#endif

#ifndef NDEBUG
#define dbgf(a...) fprintf(stderr, a)
#else
#define dbgf(a...) // nope
#endif

// Do comment comparisons in one instruction (massive brain)
#define SLASH_SLASH ('/' << 8 | '/')

#ifdef LITTLE_ENDIAN
#define STAR_SLASH ('/' << 8 | '*')
#define SLASH_STAR ('*' << 8 | '/')
#else
#define SLASH_STAR ('/' << 8 | '*')
#define STAR_SLASH ('*' << 8 | '/')
#endif

INLINE void skipws(char *text, uint64_t *i)
{
start:
    while (text[*i] <= ' ')
    {
        ++*i;
    }

    dbgf("Checking %.2s\n", text + *i);

    if (*((uint16_t *)(text + *i)) == SLASH_SLASH)
    {
        dbgf("Got a comment //\n");
        *i += 2;
        while (text[*i] && text[*i] != '\n')
        {
            dbgf("skipping...\n");
            ++*i;
        }
        goto start;
    }
    else if (*((uint16_t*)(text + *i)) == SLASH_STAR)
    {
        dbgf("Got a comment /*\n");
        *i += 2;
        while (text[*i] && *((uint16_t*)(text + *i)) != STAR_SLASH)
        {
            dbgf("skipping...\n");
            ++*i;
        }
        *i += 2;
        goto start;
    }
}

INLINE item_t parsestring(char *text, uint64_t *i)
{
    item_t str = { NULL, TYPE_STRING, 0 };
    if (text[*i] == '"')
    {
        dbgf("text is %s, i is %ld\n", text, *i);
        str.pointer = text + ++*i;
        for (; text[*i] && text[*i] != '"'; ++*i)
        {}
    }
    else
    {
        str.pointer = text + *i;
        for (; (text[*i] > '0' && text[*i] <= 'Z') ||
               (text[*i] >= 'a' && text[*i] <= 'z'); ++*i)
        {}
    }
    // abuse the fact that strings cannot be followed directly by other strings
    text[*i] = '\0';
    ++*i;

    dbgf("parsed string: '%s'\n", str.pointer);

    return str;
}

// slightly faster if you don't INLINE this function
item_t parse(char *text, uint64_t *i, uint64_t length)
{
    item_t object = { NULL, TYPE_OBJECT, 0 };
    // this is the biggest impact on optimization. 2-8 gives the best
    // results on most files. Anything over 64 is almost certain to slow
    // down the parse
    uint64_t size = 4;
    object.pointer = calloc(sizeof(pair_t), size);

    skipws(text, i);

    for (; text[*i] && *i < length;)
    {
        if (text[*i] == '}')
        {
            ++*i;
            dbgf("returning at }, '%s', %ld\n", text + *i, *i);
            return object;
        }
        dbgf("continuing loop with %c (%d), i: %ld\n", text[*i], text[*i], *i);
        if (object.length >= size)
        {
            size *= 2;
            object.pointer = realloc(object.pointer, sizeof(pair_t) * size);
        }

        dbgf("got key, at '%s'\n", text + *i);
        ((pair_t*)object.pointer)[object.length].key = parsestring(text, i);

        skipws(text, i);

        dbgf("Peeking char %c at i: %ld\n", text[*i], *i);

        if (text[*i] == '{') // sub object
        {
            ++*i;
            dbgf("got an object at %c, i(++) %ld\n", text[*i], *i);
            ((pair_t*)object.pointer)[object.length].value = parse(text, i, length);
            dbgf("the object returned\n");
        }
        else
        {
            ((pair_t*)object.pointer)[object.length].value = parsestring(text, i);
            dbgf("it was '%s'\n", object.pointer[object.length].value.pointer);
        }

        dbgf("after sub-object, at: '%s', %ld\n", text + *i, *i);

        object.length++;

        skipws(text, i);

        dbgf("starting next loop with %c (%d), i: %ld\n", text[*i], text[*i], *i);
    }
    return object;
}

void printitem(item_t item, uint32_t depth)
{
    if (item.type == TYPE_STRING)
    {
        printf("\t\"%s\"\n", (char *)item.pointer);
    }
    else if (item.type == TYPE_OBJECT)
    {
        // dont print {} for top level objects
        if (depth > 0)
            printf("{ // %d items\n", item.length);
        for (uint32_t i = 0; i < item.length; i++)
        {
            pair_t current = ((pair_t *)item.pointer)[i];
            for (uint32_t j = 0; j < depth; j++)
                printf("\t");
            printf("\"%s\" ", (char *)current.key.pointer);
            printitem(current.value, depth + 1);
        }
        if (depth > 0)
        {
            for (uint32_t j = 0; j < depth - 1; j++)
                printf("\t");
            printf("}\n");
        }
    }
}

item_t get(item_t where, char *q)
{
    if (where.type == TYPE_OBJECT)
    {
        for (int i = 0; i < where.length; i++)
        {
            pair_t current = ((pair_t *)where.pointer)[i];
            if (strcmp(current.key.pointer, q) == 0)
            {
                return current.value;
            }
        }
    }
    item_t err = { NULL, TYPE_ERROR, 0 };
    return err;
}

void freeitem(item_t item)
{
    if (item.type == TYPE_OBJECT)
    {
        for (int i = 0; i < item.length; i++)
        {
            pair_t current = ((pair_t *)item.pointer)[i];
            freeitem(current.value);
        }
        free(item.pointer);
    }
}

item_t query(item_t where, char *_q)
{
    char *q = strdup(_q);
    char *q_start = q;

    char *start = q;
    item_t current = where;

    if (*q == '.')
    {
        *q = 0;
        start = ++q;
    }

    for (; *q; q++)
    {
        if (*q == '.')
        {
            *q = 0;
            current = get(current, start);

            start = q + 1;
        }
    }
    if (q - start > 0)
    {
        current = get(current, start);
    }

    free(q_start);

    return current;
}
