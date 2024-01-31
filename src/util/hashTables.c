#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "hashTables.h"

static bool _isEmptyKey(
  HashentryKey key,
  bool numeric_key
)
{
  if (!numeric_key)
    return NULL == key.ptr;
  return !key.num.non_empty;
}

static int _compareKeys(
  HashentryKey left,
  HashentryKey right,
  HashTable_keyCompFun key_comp_fun,
  bool numeric_key
)
{
  if (!numeric_key) {
    assert(NULL != key_comp_fun);
    return key_comp_fun(left.ptr, right.ptr);
  }

  if (left.num.val < right.num.val)
    return -1;
  if (left.num.val > right.num.val)
    return 1;
  return 0;
}

static uint32_t _hashKey(
  HashentryKey key,
  HashTable_keyHashFun key_hash_fun,
  bool numeric_key
)
{
  if (numeric_key) {
    assert(key.num.non_empty);
    return key.num.val;
  }

  assert(NULL != key_hash_fun);
  return key_hash_fun(key.ptr);
}

static void doNotFreeFunHashTable(
  void * ptr
)
{
  (void) ptr;
}

void setFunHashTable(
  Hashtable * table,
  HashTable_keyHashFun key_hash_fun,
  HashTable_keyCompFun key_comp_fun,
  HashTable_freeFun key_free_fun,
  HashTable_freeFun dat_free_fun
)
{
  assert(NULL != table);
  assert((NULL == key_hash_fun) ^ !table->numeric_key);
  assert((NULL == key_comp_fun) ^ !table->numeric_key);
  assert((NULL == key_free_fun) ^ !table->numeric_key);

  if (NULL == key_hash_fun && !table->numeric_key)
    key_hash_fun = (HashTable_keyHashFun) fnv1aStrHash;
  if (NULL == key_comp_fun && !table->numeric_key)
    key_comp_fun = (HashTable_keyCompFun) strcmp;
  if (NULL == key_free_fun && !table->numeric_key)
    key_free_fun = doNotFreeFunHashTable;
  if (NULL == dat_free_fun)
    dat_free_fun = doNotFreeFunHashTable;

  table->key_hash_fun = key_hash_fun;
  table->key_comp_fun = key_comp_fun;
  table->key_free_fun = key_free_fun;
  table->dat_free_fun = dat_free_fun;
  table->initialized  = true;
}

void cleanHashTable(
  Hashtable table
)
{
  for (size_t i = 0; i < table.array_allocated_len; i++) {
    const Hashentry entry = table.array[i];
    if (!_isEmptyKey(entry.key, table.numeric_key)) {
      if (!table.numeric_key)
        table.key_free_fun(entry.key.ptr);
      table.dat_free_fun(entry.data);
    }
  }
  free(table.array);
}

static int _setEntryHashTable(
  Hashtable * table,
  HashentryKey entry_key,
  void * entry_data
)
{
  assert((NULL != table->key_hash_fun) ^ table->numeric_key);
  assert((NULL != table->key_comp_fun) ^ table->numeric_key);
  assert((NULL != table->key_free_fun) ^ table->numeric_key);
  assert(NULL != table->dat_free_fun);

  uint32_t entry_hash = _hashKey(entry_key, table->key_hash_fun, table->numeric_key);
  size_t table_mask   = table->array_allocated_len - 1;
  size_t entry_idx    = ((size_t) entry_hash) & table_mask;

  for (
    ; !_isEmptyKey(table->array[entry_idx].key, table->numeric_key);
    entry_idx = (entry_idx + 1) & table_mask
  ) {
    Hashentry * entry = &table->array[entry_idx];

    if (
      0 == _compareKeys(
        entry_key,
        entry->key,
        table->key_comp_fun,
        table->numeric_key
      )
    ) {
      /* Key match, update value */
      if (!table->numeric_key)
        table->key_free_fun(entry_key.ptr);
      table->dat_free_fun(entry->data);
      entry->data = entry_data;
      return 0;
    }
  }

  table->array[entry_idx].key  = entry_key;
  table->array[entry_idx].data = entry_data;
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

  Hashentry * old_array = table->array;
  size_t old_size = table->array_allocated_len;

  table->array = new_array;
  table->array_allocated_len = new_size;

  size_t array_used_len = 0;
  for (size_t i = 0; i < old_size; i++) {
    const Hashentry * entry = &old_array[i];

    if (!_isEmptyKey(entry->key, table->numeric_key)) {
      /* Add entry to new array */
      int ret = _setEntryHashTable(
        table,
        entry->key,
        entry->data
      );

      if (ret < 0)
        return -1;
      if (ret)
        array_used_len++;
    }
  }

  table->array_used_len = array_used_len;
  free(old_array);

  return 0;
}

static void * _getHashTable(
  Hashtable * table,
  const HashentryKey entry_key
)
{
  if (!table->array_used_len)
    return NULL; /* Empty table */

  uint32_t entry_hash = _hashKey(entry_key, table->key_hash_fun, table->numeric_key);
  size_t table_mask   = table->array_allocated_len - 1;
  size_t entry_idx    = ((size_t) entry_hash) & table_mask;

  for (
    ; !_isEmptyKey(table->array[entry_idx].key, table->numeric_key);
    entry_idx = (entry_idx + 1) & table_mask
  ) {
    Hashentry * entry = &table->array[entry_idx];
    if (
      0 == _compareKeys(
        entry_key,
        entry->key,
        table->key_comp_fun,
        table->numeric_key
      )
    ) {
      return entry->data; // Key match
    }
  }

  return NULL; // Not found
}

void * getHashTable(
  Hashtable * table,
  const void * entry_key
)
{
  assert(!table->numeric_key);

  return _getHashTable(
    table,
    (HashentryKey) {.ptr = (void *) entry_key}
  );
}

void * getNumHashTable(
  Hashtable * table,
  uint32_t entry_key
)
{
  assert(table->numeric_key);

  return _getHashTable(
    table,
    (HashentryKey) {.num = {.val = entry_key, .non_empty = true}}
  );
}

static int _putHashTable(
  Hashtable * table,
  HashentryKey entry_key,
  void * entry_data
)
{
  assert(NULL != table);


  if (!table->initialized)
    setFunHashTable(table, NULL, NULL, NULL, NULL);
  if (table->array_allocated_len <= table->array_used_len * 2) {
    /* Need expension */
    if (_growHashTable(table) < 0)
      return -1;
  }

  int ret = _setEntryHashTable(
    table,
    entry_key,
    entry_data
  );

  if (0 < ret)
    table->array_used_len++;
  return ret;
}

int putHashTable(
  Hashtable * table,
  void * entry_key,
  void * entry_data
)
{
  assert(!table->numeric_key);

  return _putHashTable(
    table,
    (HashentryKey) {.ptr = entry_key},
    entry_data
  );
}

int putNumHashTable(
  Hashtable * table,
  uint32_t entry_key,
  void * entry_data
)
{
  assert(table->numeric_key);

  return _putHashTable(
    table,
    (HashentryKey) {.num = {.val = entry_key, .non_empty = true}},
    entry_data
  );
}