#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "hashTables.h"

Hashtable * createHashTable(
  void
)
{
  Hashtable * table;

  if (NULL == (table = (Hashtable *) malloc(sizeof(Hashtable))))
    return NULL;
  table->array = NULL;
  table->usedLen = 0;
  table->allocatedLen = 0;

  setFunHashTable(table, NULL, NULL, NULL);
  return table;
}

void destroyHashTable(Hashtable * table)
{
  size_t i;

  if (NULL == table)
    return;

  for (i = 0; i < table->allocatedLen; i++) {
    if (NULL != table->array[i].key) {
      free(table->array[i].key);
      table->datFreeFun(table->array[i].data);
    }
  }

  free(table->array);
  free(table);
}

void setFunHashTable(
  Hashtable * table,
  HashTable_keyHashFun keyHashFun,
  HashTable_keyCompFun keyCompFun,
  HashTable_dataFreeFun datFreeFun
)
{
  if (NULL == keyHashFun)
    keyHashFun = (HashTable_keyHashFun) fnv1aStrHash;
  if (NULL == keyCompFun)
    keyCompFun = (HashTable_keyCompFun) strcmp;
  if (NULL == datFreeFun)
    datFreeFun = free;

  table->keyHashFun = keyHashFun;
  table->datFreeFun = datFreeFun;
}

Hashtable * createAndSetFunHashTable(
  HashTable_keyHashFun keyHashFun,
  HashTable_keyCompFun keyCompFun,
  HashTable_dataFreeFun datFreeFun
)
{
  Hashtable * table;

  if (NULL == (table = createHashTable()))
    return NULL;

  setFunHashTable(table, keyHashFun, keyCompFun, datFreeFun);
  return table;
}

int setEntryHashTable(
  Hashentry * array,
  const size_t arraySize,
  void * key,
  void * data,
  HashTable_keyHashFun keyHashFun,
  HashTable_keyCompFun keyCompFun,
  HashTable_dataFreeFun datFreeFun
)
{
  Hashentry * entry;
  size_t entryIndex, tableMask;
  uint32_t entryHash;

  entryHash = keyHashFun(key);

  tableMask = arraySize - 1;
  entryIndex = ((size_t) entryHash) & tableMask;

  for (; NULL != array[entryIndex].key; entryIndex = (entryIndex + 1) & tableMask) {
    entry = &array[entryIndex];

    if (keyCompFun(key, entry->key) == 0) {
      /* Key match, update value */
      free(key);
      datFreeFun(entry->data);
      entry->data = data;
      return 0;
    }
  }

  array[entryIndex].key = key;
  array[entryIndex].data = data;
  return 1;
}

int growHashTable(Hashtable * table)
{
  Hashentry * newArray, entry;
  size_t newSize, i;
  int ret;

  assert(NULL != table);

  newSize = GROW_HASH_TABLE_SIZE(table->allocatedLen);
  if (lb_mul_overflow(newSize, sizeof(Hashentry)))
    return -1; /* Overflow */

  if (NULL == (newArray = (Hashentry *) calloc(sizeof(Hashentry), newSize)))
    return -1;

  table->usedLen = 0;
  for (i = 0; i < table->allocatedLen; i++) {
    entry = table->array[i];

    if (NULL != entry.key) {
      /* Add entry to new array */
      ret = setEntryHashTable(
        newArray,
        newSize,
        entry.key,
        entry.data,
        table->keyHashFun,
        table->keyCompFun,
        table->datFreeFun
      );

      if (ret < 0)
        return -1;
      if (ret)
        table->usedLen++;
    }
  }

  free(table->array);
  table->array = newArray;
  table->allocatedLen = newSize;
  return 0;
}

void * getHashTable(Hashtable * table, const void * key)
{
  Hashentry * entry;
  size_t entryIndex, tableMask;
  uint32_t entryHash;

  if (!table->usedLen)
    return NULL; /* Empty table */

  entryHash = table->keyHashFun(key);

  tableMask = table->allocatedLen - 1;
  entryIndex = ((size_t) entryHash) & tableMask;

  for (; table->array[entryIndex].key != NULL; entryIndex = (entryIndex + 1) & tableMask) {
    entry = &table->array[entryIndex];

    if (table->keyCompFun(key, entry->key) == 0) {
      /* Key match, found entry */
      return entry->data;
    }
  }

  return NULL; /* Not found */
}

int putHashTable(Hashtable * table, void * key, void * data)
{
  int ret;

  assert(NULL != table);

  if (table->allocatedLen <= table->usedLen * 2) {
    /* Need expension */
    if (growHashTable(table) < 0)
      return -1;
  }

  ret = setEntryHashTable(
    table->array,
    table->allocatedLen,
    key,
    data,
    table->keyHashFun,
    table->keyCompFun,
    table->datFreeFun
  );

  if (0 < ret)
    table->usedLen++;

  return ret;
}

#undef main

