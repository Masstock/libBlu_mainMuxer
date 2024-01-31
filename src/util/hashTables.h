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

typedef struct {
  void * key;
  void * data;
} Hashentry;

typedef uint32_t (*HashTable_keyHashFun) (
  const void *
);

typedef int (*HashTable_keyCompFun) (
  const void *,
  const void *
);

typedef void (*HashTable_datFreeFun) (
  void *
);

typedef struct {
  HashTable_keyHashFun key_hash_fun;
  HashTable_keyCompFun key_comp_fun;
  HashTable_datFreeFun dat_free_fun;

  Hashentry * array;
  size_t array_used_len;
  size_t array_allocated_len;
  bool initialized;
} Hashtable;

void setFunHashTable(
  Hashtable * table,
  HashTable_keyHashFun key_hash_fun,
  HashTable_keyCompFun key_comp_fun,
  HashTable_datFreeFun dat_free_fun
);

static inline void initHashTable(
  Hashtable * dst
)
{
  *dst = (Hashtable) {0};
  setFunHashTable(dst, NULL, NULL, NULL);
}

static inline void cleanHashTable(
  Hashtable table
)
{
  for (size_t i = 0; i < table.array_allocated_len; i++) {
    const Hashentry entry = table.array[i];
    if (NULL != entry.key) {
      free(entry.key);
      table.dat_free_fun(entry.data);
    }
  }
  free(table.array);
}

void * getHashTable(
  Hashtable * table,
  const void * entry_key
);

int putHashTable(
  Hashtable * table,
  void * entry_key,
  void * entry_data
);

#endif