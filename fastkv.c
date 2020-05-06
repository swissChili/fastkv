#include "fastkv.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#if !defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
	#ifndef _WIN32
		#include <endian.h>
	#else
		#warning "Endianness unspecified and endian.h does not exist, define " \
				"LITTLE_ENDIAN or BIG_ENDIAN, defaulting to LITTLE_ENDIAN"

		#define LITTLE_ENDIAN
	#endif
#endif

#if defined(NDEBUG) && !defined(_WIN32)
	#define INLINE __attribute__((flatten))
#else
	#define INLINE // __attribute__((flatten))
#endif

#ifndef NDEBUG
	#define dbgf(...) fprintf(stderr, __VA_ARGS__)
#else
	#define dbgf // nope
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
    while (text[*i] && text[*i] <= ' ')
    {
        ++*i;
    }

    dbgf("Checking %.2s\n", &text[*i]);

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
        str.string = text + ++*i;
        for (; text[*i] && text[*i] != '"'; ++*i)
        {}
    }
    else
    {
        str.string = text + *i;
        for (; (text[*i] > '0' && text[*i] <= 'Z') ||
               (text[*i] >= 'a' && text[*i] <= 'z') ||
			   (text[*i] == '_'); ++*i)
        {}
    }
    // abuse the fact that strings cannot be followed directly by other strings
    text[*i] = '\0';
    ++*i;

    dbgf("parsed string: '%s'\n", str.string);

    return str;
}

INLINE int term(char *text, uint64_t *i, vars_t defs)
{
	int negate = 0;
	if (text[*i] == '!')
	{
		negate = 1;
		skipws(text, i);
	}

	if (text[*i] == '$')
	{
		++*i;
		char *var = parsestring(text, i).string;
		dbgf("Parsed str in [] %s\n", var);
		for (int j = 0; j < defs.length; j++)
		{
			//dbgf("At j=%d, checking var %s\n", j, defs.vars[j]);
			if (strcmp(var, defs.vars[j]) == 0)
			{
				return !negate;
			}
		}
	}
	return negate;
}

INLINE int expr(char *text, uint64_t *i, vars_t defs)
{
	skipws(text, i);

	int first = term(text, i, defs);
	skipws(text, i);

	if (text[*i] == '&')
	{
		while (text[*i] && text[*i] == '&')
		{
			++*i;
		}

		skipws(text, i);
		int second = expr(text, i, defs);
		return first && second;
	}
	else if (text[*i] == '|')
	{
		while (text[*i] && text[*i] == '|')
		{
			++*i;
		}

		skipws(text, i);
		int second = expr(text, i, defs);
		return first || second;
	}
	return first;
}

INLINE int parsecond(char *text, uint64_t *i, uint64_t length, vars_t defs)
{
	skipws(text, i);
	if (text[*i] == '[')
	{
		++*i;
		int res = expr(text, i, defs);
		skipws(text, i);
		if (text[*i] != ']')
		{
			fprintf(stderr, "Expected a ]\n");
		}
		else ++*i;
		return res;
	}
}

// slightly faster if you don't INLINE this function
item_t kv_parse(char *text, uint64_t *i, uint64_t length, vars_t defs)
{
    item_t object = { NULL, TYPE_OBJECT, 0 };
    // this is the biggest impact on optimization. 2-8 gives the best
    // results on most files. Anything over 64 is almost certain to slow
    // down the kv_parse
    uint64_t size = 4;
    object.object = calloc(sizeof(pair_t), size);

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
            object.object = realloc(object.object, sizeof(pair_t) * size);
        }

        dbgf("got key, at '%s'\n", text + *i);
        item_t key = parsestring(text, i);

        skipws(text, i);

        dbgf("Peeking char %c at i: %ld\n", text[*i], *i);

        item_t value;

        if (text[*i] == '{') // sub object
        {
            ++*i;
            dbgf("got an object at %c, i(++) %ld\n", text[*i], *i);
            value = kv_parse(text, i, length, defs);
            dbgf("the object returned\n");
        }
        else
        {
            value = parsestring(text, i);
            dbgf("it was '%s'\n", object.object[object.length].value.string);
        }

        dbgf("after sub-object, at: '%s', %ld\n", text + *i, *i);

		int cond = parsecond(text, i, length, defs);

		if (cond)
		{
			object.object[object.length].key = key;
			object.object[object.length].value = value;
			object.length++;
		}

        dbgf("[] checking cond: %d\n", cond);
        dbgf("Finished [] at %ld\n", *i);

        skipws(text, i);

        dbgf("starting next loop with %c (%d), i: %ld\n", text[*i], text[*i], *i);
    }
    return object;
}

void kv_printitem(item_t item, uint32_t depth)
{
    if (item.type == TYPE_STRING)
    {
        printf("\t\"%s\"\n", item.string);
    }
    else if (item.type == TYPE_OBJECT)
    {
        // dont print {} for top level objects
        if (depth > 0)
            printf("{ // %d items\n", item.length);
        for (uint32_t i = 0; i < item.length; i++)
        {
            pair_t current = item.object[i];
            for (uint32_t j = 0; j < depth; j++)
                printf("\t");
            printf("\"%s\" ", current.key.string);
			kv_printitem(current.value, depth + 1);
        }
        if (depth > 0)
        {
            for (uint32_t j = 0; j < depth - 1; j++)
                printf("\t");
            printf("}\n");
        }
    }
}

item_t kv_get(item_t where, char *q)
{
    if (where.type == TYPE_OBJECT)
    {
        for (int i = 0; i < where.length; i++)
        {
            pair_t current = where.object[i];
            if (strcmp(current.key.string, q) == 0)
            {
                return current.value;
            }
        }
    }
    item_t err = { NULL, TYPE_ERROR, 0 };
    return err;
}

void kv_freeitem(item_t item)
{
    if (item.type == TYPE_OBJECT)
    {
        for (int i = 0; i < item.length; i++)
        {
            pair_t current = item.object[i];
			kv_freeitem(current.value);
        }
        free(item.object);
    }
}

item_t kv_query(item_t where, char *_q)
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
            current = kv_get(current, start);

            start = q + 1;
        }
    }
    if (q - start > 0)
    {
        current = kv_get(current, start);
    }

    free(q_start);

    return current;
}
