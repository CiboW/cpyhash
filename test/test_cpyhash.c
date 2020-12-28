#include <stdlib.h>
#include <assert.h>
#include "../src/cpyhash.h"
#include "../src/cpyhashlib.h"

static void
test_hashtable()
{
    _Cpy_hashtable *table = _Cpy_hashtable_new(hash_char,
                                               _Cpy_hashtable_compare_direct);
    if (table == NULL) {
        return;
    }

    // Using an newly allocated table must not crash
    assert(table->nentries == 0);
    assert(table->nbuckets > 0);
    assert(_Cpy_hashtable_get(table, TO_PTR('x')) == NULL);

    // Test _Cpy_hashtable_set()
    char key;
    for (key='a'; key <= 'z'; key++) {
        int value = VALUE(key);
        if (_Cpy_hashtable_set(table, TO_PTR(key), TO_PTR(value)) < 0) {
            _Cpy_hashtable_destroy(table);
            return;
        }
    }
    assert(table->nentries == 26);
    assert(table->nbuckets > table->nentries);

    // Test _Cpy_hashtable_get_entry()
    for (key='a'; key <= 'z'; key++) {
        _Cpy_hashtable_entry *entry = _Cpy_hashtable_get_entry(table, TO_PTR(key));
        assert(entry != NULL);
        assert(entry->key == TO_PTR(key));
        assert(entry->value == TO_PTR(VALUE(key)));
    }

    // Test _Cpy_hashtable_get()
    for (key='a'; key <= 'z'; key++) {
        void *value_ptr = _Cpy_hashtable_get(table, TO_PTR(key));
        assert((int)FROM_PTR(value_ptr) == VALUE(key));
    }

    // Test _Cpy_hashtable_steal()
    key = 'p';
    void *value_ptr = _Cpy_hashtable_steal(table, TO_PTR(key));
    assert((int)FROM_PTR(value_ptr) == VALUE(key));
    assert(table->nentries == 25);
    assert(_Cpy_hashtable_get_entry(table, TO_PTR(key)) == NULL);

    // Test _Cpy_hashtable_foreach()
    int count = 0;
    int res = _Cpy_hashtable_foreach(table, hashtable_cb, &count);
    assert(res == 0);
    assert(count == 25);

    // Test _Cpy_hashtable_clear()
    _Cpy_hashtable_clear(table);
    assert(table->nentries == 0);
    assert(table->nbuckets > 0);
    assert(_Cpy_hashtable_get(table, TO_PTR('x')) == NULL);

    _Cpy_hashtable_destroy(table);
}

void main(){
    test_hashtable();
}