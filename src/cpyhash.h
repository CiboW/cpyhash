#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <bits/wordsize.h>

typedef intptr_t hash_t;    //The type of hash value is same with int pointer
typedef size_t uhash_t;

#ifdef __WORDSIZE == 64
#   define SIZEOF_VOID_P 8
#else
#   define SIZEOF_VOID_P 4
#endif

#define HASHTABLE_MIN_SIZE 16
#define HASHTABLE_HIGH 0.50
#define HASHTABLE_LOW 0.10
#define HASHTABLE_REHASH_FACTOR 2.0 / (HASHTABLE_LOW + HASHTABLE_HIGH)

#define TO_PTR(ch) ((void*)(uintptr_t)ch)
#define FROM_PTR(ptr) ((uintptr_t)ptr)
#define VALUE(key) (1 + ((int)(key) - 'a'))

//The item of linked list
typedef struct _Cpy_list_item_s {
    struct _Cpy_list_item_s *next;
} _Cpy_list_item;

//A structure that include the head of linked list
typedef struct {
    _Cpy_list_item *head;
} _Cpy_list;

#define _Cpy_LIST_ITEM_NEXT(ITEM) (((_Cpy_list_item *)ITEM)->next)
#define _Cpy_LIST_HEAD(LIST) (((_Cpy_list *)LIST)->head)

//Hashtable entry
typedef struct {
    //An entry of hashtbale must be An item of linked list
    _Cpy_list_item list_item;

    hash_t key_hash;    
    void *key;
    void *value;
} _Cpy_hashtable_entry;

//Get the hash of linked list
#define BUCKETS_HEAD(LIST) \
        ((_Cpy_hashtable_entry *)_Cpy_LIST_HEAD(&(LIST)))
//Get the next entry
#define ENTRY_NEXT(ENTRY) \
        ((_Cpy_hashtable_entry *)_Cpy_LIST_ITEM_NEXT(ENTRY))  

struct _Cpy_hashtable; 
typedef struct _Cpy_hashtable _Cpy_hashtable;
typedef uhash_t (*_Cpy_hashtable_hash_func) (const void *key);
typedef int (*_Cpy_hashtable_compare_func) (const void *key1, const void *key2);
typedef void (*_Cpy_hashtable_destroy_func) (void *key);
typedef _Cpy_hashtable_entry* (*_Cpy_hashtable_get_entry_func)(_Cpy_hashtable *ht, const void *key);

typedef struct {
    void* (*malloc) (size_t size);
    void (*free) (void *ptr);
} _Cpy_hashtable_allocator;

// Hashtable
struct _Cpy_hashtable {
    size_t nentries;  //Then number of entry
    size_t nbuckets;  //The number of bucket that include the head of linked list
    _Cpy_list *buckets;

    _Cpy_hashtable_get_entry_func get_entry_func;
    _Cpy_hashtable_hash_func hash_func;
    _Cpy_hashtable_compare_func compare_func;
    _Cpy_hashtable_destroy_func key_destroy_func;
    _Cpy_hashtable_destroy_func value_destroy_func;
    _Cpy_hashtable_allocator alloc;
};

//Get the head of buckets[BUCKET] in hashtable HT
#define TABLE_HEAD(HT, BUCKET) \
        ((_Cpy_hashtable_entry *)_Cpy_LIST_HEAD(&(HT)->buckets[BUCKET]))


/* The default hash algorithm is defined here.
  If you want to use more hash functions, you can find it in "cpyhashlib.c" */

//Default hash algorithm - bit operation
hash_t _Cpy_HashPointerRaw(const void *p)
{
    /*Bottom 3 or 4 bits are likely to be 0; rotate y by 4 to avoid
       excessive hash collisions for dicts and sets. For example:
      01101010 11000011 01011110 00011000
      => 0000 01101010 11000011 01011110 0001 | 1000 0000 00000000 00000000 00000000
      => 1000 01101010 11000011 01011110 0001
    */

    size_t y = (size_t)p;
    y = (y >> 4) | (y << (8 * SIZEOF_VOID_P - 4));
    return (hash_t)y;
}

uhash_t _Cpy_hashtable_hash_ptr(const void *key)
{
    return (uhash_t) _Cpy_HashPointerRaw(key);
}

//Default hash algorithm - convert key to char
static uhash_t
hash_char(const void *key)
{
    char ch = (char)FROM_PTR(key);
    return ch;
}

/* End: default hash algorithm */

static void
_Cpy_list_init(_Cpy_list *list)
{
    list->head = NULL;
}

//Insert an item at the front of linked list
static void
_Cpy_list_prepend(_Cpy_list *list, _Cpy_list_item *item)
{
    item->next = list->head;
    list->head = item;
}

static void
_Cpy_list_remove(_Cpy_list *list, _Cpy_list_item *previous,
                 _Cpy_list_item *item)
{
    if (previous != NULL)
        previous->next = item->next;
    else
        list->head = item->next;
}

int _Cpy_hashtable_compare_direct(const void *key1, const void *key2)
{
    return (key1 == key2);
}

//The length of buckets array must be a power of 2.
static size_t round_size(size_t s)
{
    size_t i;
    if (s < HASHTABLE_MIN_SIZE)
        return HASHTABLE_MIN_SIZE;
    i = 1;
    while (i < s)
        i <<= 1;
    return i;
}

//Return the memory length of hashtable
size_t _Cpy_hashtable_size(const _Cpy_hashtable *ht)
{
    size_t size = sizeof(_Cpy_hashtable);
    /* buckets */
    size += ht->nbuckets * sizeof(_Cpy_hashtable_entry *);
    /* entries */
    size += ht->nentries * sizeof(_Cpy_hashtable_entry);
    return size;
}

/* Get an entry.
   Return NULL if the key does not exist. */
static inline _Cpy_hashtable_entry *
_Cpy_hashtable_get_entry(_Cpy_hashtable *ht, const void *key)
{
    return ht->get_entry_func(ht, key);
}

//Default 'get' fucntion which can get an entry by compare_func.
_Cpy_hashtable_entry *
_Cpy_hashtable_get_entry_generic(_Cpy_hashtable *ht, const void *key)
{
    uhash_t key_hash = ht->hash_func(key);
    size_t index = key_hash & (ht->nbuckets - 1);   //Specify bucket index by bit operation, the index must be less than ht->nbuckets
    _Cpy_hashtable_entry *entry = TABLE_HEAD(ht, index);
    while (1) {
        if (entry == NULL) {
            return NULL;
        }
        if (entry->key_hash == key_hash && ht->compare_func(key, entry->key)) {
            break;
        }
        entry = ENTRY_NEXT(entry);
    }
    return entry;
}

//Get an entry by comparing key instead of compare_func
static _Cpy_hashtable_entry *
_Cpy_hashtable_get_entry_ptr(_Cpy_hashtable *ht, const void *key)
{
    uhash_t key_hash = _Cpy_hashtable_hash_ptr(key);
    size_t index = key_hash & (ht->nbuckets - 1);  //Specify bucket index by bit operation, the index must be less than ht->nbuckets
    _Cpy_hashtable_entry *entry = TABLE_HEAD(ht, index);
    while (1) {
        if (entry == NULL) {
            return NULL;
        }
        if (entry->key == key) {
            break;
        }
        entry = ENTRY_NEXT(entry);
    }
    return entry;
}

static int
hashtable_rehash(_Cpy_hashtable *ht)
{
    size_t new_size = round_size((size_t)(ht->nentries * HASHTABLE_REHASH_FACTOR));
    if (new_size == ht->nbuckets) {
        return 0;
    }

    size_t buckets_size = new_size * sizeof(ht->buckets[0]);
    _Cpy_list *new_buckets = ht->alloc.malloc(buckets_size);
    if (new_buckets == NULL) {
        return -1;
    }
    memset(new_buckets, 0, buckets_size);

    for (size_t bucket = 0; bucket < ht->nbuckets; bucket++) {
        _Cpy_hashtable_entry *entry = BUCKETS_HEAD(ht->buckets[bucket]);
        while (entry != NULL) {
            assert(ht->hash_func(entry->key) == entry->key_hash);
            _Cpy_hashtable_entry *next = ENTRY_NEXT(entry);
            size_t entry_index = entry->key_hash & (new_size - 1);

            _Cpy_list_prepend(&new_buckets[entry_index], (_Cpy_list_item*)entry);

            entry = next;
        }
    }

    ht->alloc.free(ht->buckets);
    ht->nbuckets = new_size;
    ht->buckets = new_buckets;
    return 0;
}

/* Remove a key and its associated value without calling key and value destroy
   functions.

   Return the removed value if the key was found.
   Return NULL if the key was not found. */
void*
_Cpy_hashtable_steal(_Cpy_hashtable *ht, const void *key)
{
    uhash_t key_hash = ht->hash_func(key);
    size_t index = key_hash & (ht->nbuckets - 1);

    _Cpy_hashtable_entry *entry = TABLE_HEAD(ht, index);
    _Cpy_hashtable_entry *previous = NULL;
    while (1) {
        if (entry == NULL) {
            return NULL;
        }
        if (entry->key_hash == key_hash && ht->compare_func(key, entry->key)) {
            break;
        }
        previous = entry;
        entry = ENTRY_NEXT(entry);
    }

    _Cpy_list_remove(&ht->buckets[index], (_Cpy_list_item *)previous,
                     (_Cpy_list_item *)entry);
    ht->nentries--;

    void *value = entry->value;
    ht->alloc.free(entry);

    //If there are too many buckets, should rehash them
    if ((float)ht->nentries / (float)ht->nbuckets < HASHTABLE_LOW) {
        hashtable_rehash(ht);
    }
    return value;
}

/* Add a new entry to the hash. The key must not be present in the hash table.
   Return 0 on success, -1 on memory error. */
int
_Cpy_hashtable_set(_Cpy_hashtable *ht, const void *key, void *value)
{
    _Cpy_hashtable_entry *entry;

#ifndef NDEBUG
    entry = ht->get_entry_func(ht, key);
    assert(entry == NULL);
#endif

    entry = ht->alloc.malloc(sizeof(_Cpy_hashtable_entry));
    if (entry == NULL) {
        return -1;
    }

    entry->key_hash = ht->hash_func(key);
    entry->key = (void *)key;
    entry->value = value;

    ht->nentries++;
    //If the entry is too centralized, hash again
    if ((float)ht->nentries / (float)ht->nbuckets > HASHTABLE_HIGH) {
        if (hashtable_rehash(ht) < 0) {
            ht->nentries--;
            ht->alloc.free(entry);
            return -1;
        }
    }

    size_t index = entry->key_hash & (ht->nbuckets - 1);
    _Cpy_list_prepend(&ht->buckets[index], (_Cpy_list_item*)entry);
    return 0;
}

/* Get value from an entry.
   Return NULL if the entry is not found.

   Use _Cpy_hashtable_get_entry() to distinguish entry value equal to NULL
   and entry not found. */
void*
_Cpy_hashtable_get(_Cpy_hashtable *ht, const void *key)
{
    _Cpy_hashtable_entry *entry = ht->get_entry_func(ht, key);
    if (entry != NULL) {
        return entry->value;
    }
    else {
        return NULL;
    }
}

/* Call func() on each entry of the hashtable.
   Iteration stops if func() result is non-zero, in this case it's the result
   of the call. Otherwise, the function returns 0 */
typedef int (*_Cpy_hashtable_foreach_func) (_Cpy_hashtable *ht,
                                           const void *key, const void *value,
                                           void *user_data);
int
_Cpy_hashtable_foreach(_Cpy_hashtable *ht,
                      _Cpy_hashtable_foreach_func func,  //The default func is hashtable_cb.
                      void *user_data)
{
    for (size_t hv = 0; hv < ht->nbuckets; hv++) {
        _Cpy_hashtable_entry *entry = TABLE_HEAD(ht, hv);
        while (entry != NULL) {
            int res = func(ht, entry->key, entry->value, user_data);
            if (res) {
                return res;
            }
            entry = ENTRY_NEXT(entry);
        }
    }
    return 0;
}

static int
hashtable_cb(_Cpy_hashtable *table,
             const void *key_ptr, const void *value_ptr,
             void *user_data)
{
    int *count = (int *)user_data;
    char key = (char)FROM_PTR(key_ptr);
    int value = (int)FROM_PTR(value_ptr);
    assert(value == VALUE(key));
    *count += 1;
    return 0;
}

//Create a hashtable
_Cpy_hashtable *
_Cpy_hashtable_new_full(_Cpy_hashtable_hash_func hash_func,
                       _Cpy_hashtable_compare_func compare_func,
                       _Cpy_hashtable_destroy_func key_destroy_func,
                       _Cpy_hashtable_destroy_func value_destroy_func,
                       _Cpy_hashtable_allocator *allocator)
{
    _Cpy_hashtable_allocator alloc;
    if (allocator == NULL) {
        alloc.malloc = malloc;
        alloc.free = free;
    }
    else {
        alloc = *allocator;
    }

    _Cpy_hashtable *ht = (_Cpy_hashtable *)alloc.malloc(sizeof(_Cpy_hashtable));
    if (ht == NULL) {
        return ht;
    }

    ht->nbuckets = HASHTABLE_MIN_SIZE;
    ht->nentries = 0;

    size_t buckets_size = ht->nbuckets * sizeof(ht->buckets[0]);
    ht->buckets = alloc.malloc(buckets_size);
    if (ht->buckets == NULL) {
        alloc.free(ht);
        return NULL;
    }
    memset(ht->buckets, 0, buckets_size);   //initialize bucket by 0

    ht->get_entry_func = _Cpy_hashtable_get_entry_generic;
    ht->hash_func = hash_func;
    ht->compare_func = compare_func;
    ht->key_destroy_func = key_destroy_func;
    ht->value_destroy_func = value_destroy_func;
    ht->alloc = alloc;
    if (ht->hash_func == _Cpy_hashtable_hash_ptr
        && ht->compare_func == _Cpy_hashtable_compare_direct)
    {
        ht->get_entry_func = _Cpy_hashtable_get_entry_ptr;
    }
    return ht;
}

//Create a hashtable
_Cpy_hashtable *
_Cpy_hashtable_new(_Cpy_hashtable_hash_func hash_func,
                  _Cpy_hashtable_compare_func compare_func)
{
    return _Cpy_hashtable_new_full(hash_func, compare_func,
                                  NULL, NULL, NULL);
}

static void
_Cpy_hashtable_destroy_entry(_Cpy_hashtable *ht, _Cpy_hashtable_entry *entry)
{
    if (ht->key_destroy_func) {
        ht->key_destroy_func(entry->key);
    }
    if (ht->value_destroy_func) {
        ht->value_destroy_func(entry->value);
    }
    ht->alloc.free(entry);
}

void
_Cpy_hashtable_clear(_Cpy_hashtable *ht)
{
    for (size_t i=0; i < ht->nbuckets; i++) {
        _Cpy_hashtable_entry *entry = TABLE_HEAD(ht, i);
        while (entry != NULL) {
            _Cpy_hashtable_entry *next = ENTRY_NEXT(entry);
            _Cpy_hashtable_destroy_entry(ht, entry);
            entry = next;
        }
        _Cpy_list_init(&ht->buckets[i]);
    }
    ht->nentries = 0;
    // Ignore failure: clear function is not expected to fail
    // because of a memory allocation failure.
    (void)hashtable_rehash(ht);
}


void
_Cpy_hashtable_destroy(_Cpy_hashtable *ht)
{
    for (size_t i = 0; i < ht->nbuckets; i++) {
        _Cpy_hashtable_entry *entry = TABLE_HEAD(ht, i);
        while (entry) {
            _Cpy_hashtable_entry *entry_next = ENTRY_NEXT(entry);
            _Cpy_hashtable_destroy_entry(ht, entry);
            entry = entry_next;
        }
    }

    ht->alloc.free(ht->buckets);
    ht->alloc.free(ht);
}