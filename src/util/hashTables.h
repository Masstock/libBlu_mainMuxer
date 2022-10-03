/** \~english
 * \file hashTables.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Hash tables structures implementation module.
 */

#ifndef __LIBBLU_MUXER__UTIL__HASH_TABLES_H__
#define __LIBBLU_MUXER__UTIL__HASH_TABLES_H__

#include "common.h"

/* Open addressing */

#define GROW_HASH_TABLE_SIZE(size)        \
  (                                       \
    ((size) < HASH_TABLE_INITIAL_SIZE) ?  \
      HASH_TABLE_INITIAL_SIZE             \
    :                                     \
      (size) * 2                          \
  )

#define HASH_TABLE_INITIAL_SIZE 32

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

typedef void (*HashTable_dataFreeFun) (
  void *
);

typedef struct {
  HashTable_keyHashFun keyHashFun;
  HashTable_keyCompFun keyCompFun;
  HashTable_dataFreeFun datFreeFun;

  Hashentry * array;
  size_t usedLen;
  size_t allocatedLen;
} Hashtable;

Hashtable * createHashTable(
  void
);

void destroyHashTable(
  Hashtable * table
);

void setFunHashTable(
  Hashtable * table,
  HashTable_keyHashFun keyHashFun,
  HashTable_keyCompFun keyCompFun,
  HashTable_dataFreeFun datFreeFun
);

Hashtable * createAndSetFunHashTable(
  HashTable_keyHashFun keyHashFun,
  HashTable_keyCompFun keyCompFun,
  HashTable_dataFreeFun datFreeFun
);

int setEntryHashTable(
  Hashentry * array,
  const size_t arraySize,
  void * key,
  void * data,
  HashTable_keyHashFun keyHashFun,
  HashTable_keyCompFun keyCompFun,
  HashTable_dataFreeFun datFreeFun
);

int growHashTable(
  Hashtable * table
);

void * getHashTable(
  Hashtable * table,
  const void * key
);

int putHashTable(
  Hashtable * table,
  void * key,
  void * data
);

#endif