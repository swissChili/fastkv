#pragma once

#include <stdint.h>

enum
{
    TYPE_ERROR = 0,
    TYPE_STRING,
    TYPE_OBJECT,
};

typedef struct item_t
{
    void *pointer;
    uint8_t type;
    uint32_t length;
} item_t;

typedef struct pair_t
{
    item_t key; // a string
    item_t value;
} pair_t;

void printitem(item_t item, uint32_t depth);

void freeitem(item_t item);

item_t parse(char *text, uint64_t *i, uint64_t length);

item_t get(item_t where, char *q);

item_t query(item_t where, char *q);