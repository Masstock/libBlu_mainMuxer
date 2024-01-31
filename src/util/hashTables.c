#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "hashTables.h"

void setFunHashTable(
  Hashtable * table,
  HashTable_keyHashFun key_hash_fun,
  HashTable_keyCompFun key_comp_fun,
  HashTable_datFreeFun dat_free_fun
)
{
  assert(NULL != table);

  if (NULL == key_hash_fun)
    key_hash_fun = (HashTable_keyHashFun) fnv1aStrHash;
  if (NULL == key_comp_fun)
    key_comp_fun = (HashTable_keyCompFun) strcmp;
  if (NULL == dat_free_fun)
    dat_free_fun = free;

  table->key_hash_fun = key_hash_fun;
  table->dat_free_fun = dat_free_fun;
  table->initialized  = true;
}

static int _setEntryHashTable(
  Hashentry * array,
  const size_t array_size,
  void * entry_key,
  void * entry_data,
  HashTable_keyHashFun key_hash_fun,
  HashTable_keyCompFun key_comp_fun,
  HashTable_datFreeFun dat_free_fun
)
{
  uint32_t entry_hash = key_hash_fun(entry_key);
  size_t table_mask   = array_size - 1;
  size_t entry_idx    = ((size_t) entry_hash) & table_mask;

  for (; NULL != array[entry_idx].key; entry_idx = (entry_idx + 1) & table_mask) {
    Hashentry * entry = &array[entry_idx];
    if (key_comp_fun(entry_key, entry->key) == 0) {
      /* Key match, update value */
      free(entry_key);
      dat_free_fun(entry->data);
      entry->data = entry_data;
      return 0;
    }
  }

  array[entry_idx].key  = entry_key;
  array[entry_idx].data = entry_data;
  return 1;
}

#define HASH_TABLE_INITIAL_SIZE  32

static int _growHashTable(
  Hashtable * table
)
{
  size_t new_size = GROW_ALLOCATION(
    table->array_allocated_len,
    HASH_TABLE_INITIAL_SIZE
  );
  if (lb_mul_overflow_size_t(new_size, sizeof(Hashentry)))
    return -1; /* Overflow */

  Hashentry * new_array = (Hashentry *) calloc(new_size, sizeof(Hashentry));
  if (NULL == new_array)
    return -1;

  size_t array_used_len = 0;
  for (size_t i = 0; i < table->array_allocated_len; i++) {
    const Hashentry * entry = &table->array[i];
    if (NULL != entry->key) {
      /* Add entry to new array */
      int ret = _setEntryHashTable(
        new_array,
        new_size,
        entry->key,
        entry->data,
        table->key_hash_fun,
        table->key_comp_fun,
        table->dat_free_fun
      );

      if (ret < 0)
        return -1;
      if (ret)
        array_used_len++;
    }
  }

  free(table->array);
  table->array               = new_array;
  table->array_used_len      = array_used_len;
  table->array_allocated_len = new_size;
  return 0;
}

void * getHashTable(
  Hashtable * table,
  const void * entry_key
)
{
  if (!table->array_used_len)
    return NULL; /* Empty table */

  uint32_t entry_hash = table->key_hash_fun(entry_key);
  size_t table_mask   = table->array_allocated_len - 1;
  size_t entry_idx    = ((size_t) entry_hash) & table_mask;

  for (; table->array[entry_idx].key != NULL; entry_idx = (entry_idx + 1) & table_mask) {
    Hashentry * entry = &table->array[entry_idx];
    if (table->key_comp_fun(entry_key, entry->key) == 0)
      return entry->data; // Key match
  }

  return NULL; // Not found
}

int putHashTable(
  Hashtable * table,
  void * entry_key,
  void * entry_data
)
{
  assert(NULL != table);

  if (!table->initialized)
    setFunHashTable(table, NULL, NULL, NULL);
  if (table->array_allocated_len <= table->array_used_len * 2) {
    /* Need expension */
    if (_growHashTable(table) < 0)
      return -1;
  }

  int ret = _setEntryHashTable(
    table->array,
    table->array_allocated_len,
    entry_key,
    entry_data,
    table->key_hash_fun,
    table->key_comp_fun,
    table->dat_free_fun
  );

  if (0 < ret)
    table->array_used_len++;
  return ret;
}
