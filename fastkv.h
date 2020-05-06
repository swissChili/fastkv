#ifndef FASTKV_FASTKV_H
#define FASTKV_FASTKV_H

#include <stdint.h>

enum
{
    TYPE_ERROR = 0,
    TYPE_STRING,
    TYPE_OBJECT,
};

struct pair_t;

typedef struct item_t
{
	union
	{
		char *string;
		struct pair_t *object;
	};
    uint8_t type;
    uint32_t length;
} item_t;

typedef struct pair_t
{
    item_t key; // a string
    item_t value;
} pair_t;

typedef struct vars_t
{
	uint64_t length;
	char **vars;
} vars_t;

void kv_printitem(item_t item, uint32_t depth);

void kv_freeitem(item_t item);

item_t kv_parse(char *text, uint64_t *i, uint64_t length, vars_t defs);

item_t kv_get(item_t where, char *q);

item_t kv_query(item_t where, char *q);

#endif
