/** \~english
 * \file hashTables.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Hash tables structures implementation module.
 *
 * Open addressing method.
 */

#ifndef __LIBBLU_MUXER__UTIL__HASH_TABLES_H__
#define __LIBBLU_MUXER__UTIL__HASH_TABLES_H__

#include "common.h"

typedef union {
  void *ptr;
  struct {
    uint32_t val;
    bool non_empty;
  } num;
} HashentryKey;

typedef struct {
  HashentryKey key;
  void *data;
} Hashentry;

typedef uint32_t (*HashTable_keyHashFun) (
  const void *
);

typedef int (*HashTable_keyCompFun) (
  const void *,
  const void *
);

typedef void (*HashTable_freeFun) (
  void *
);

typedef struct {
  HashTable_keyHashFun key_hash_fun;
  HashTable_keyCompFun key_comp_fun;
  HashTable_freeFun key_free_fun;
  HashTable_freeFun dat_free_fun;
  bool numeric_key;

  Hashentry *array;
  size_t array_used_len;
  size_t array_allocated_len;
  bool initialized;
} Hashtable;

static inline Hashtable newHashtable(
  void
)
{
  return (Hashtable) {0};
}

static inline Hashtable newNumHashtable(
  void
)
{
  return (Hashtable) {.numeric_key = true};
}

void setFunHashTable(
  Hashtable *table,
  HashTable_keyHashFun key_hash_fun,
  HashTable_keyCompFun key_comp_fun,
  HashTable_freeFun key_free_fun,
  HashTable_freeFun dat_free_fun
);

void cleanHashTable(
  Hashtable table
);

void * getHashTable(
  Hashtable *table,
  const void *entry_key
);

void * getNumHashTable(
  Hashtable *table,
  uint32_t entry_key
);

int putHashTable(
  Hashtable *table,
  void *entry_key,
  void *entry_data
);

int putNumHashTable(
  Hashtable *table,
  uint32_t entry_key,
  void *entry_data
);

#endif