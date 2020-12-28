# cpyhash

#### Description
This project cloned the hashtable in Cpython and simplified it. It only needs to include a header file to use the hashtable which is almost the same as that in Cpython.



#### The structure of hashtable

![cpython-hashtable-struct](https://images.gitee.com/uploads/images/2020/1228/101235_cb10adcf_5034884.jpeg "cpython-hashtable.jpg")

* `entry`: The entry of hashtable must be a item of linked list
* `bucket`: The bucket include the head of linked list

* `*buckets`: The pointer of `bucket` array

Assign `entry` to different bucket through bit operation, and insert into linked list.

```
size_t index = key_hash & (ht->nbuckets - 1);	//index must be less than ht->nbuckets
```



#### The default hash algorithm

1. **Bit operation**

```C
//Default hash algorithm - bit operation
uhash_t _Cpy_HashPointerRaw(const void *p)
{
    /*Bottom 3 or 4 bits are likely to be 0; rotate y by 4 to avoid
       excessive hash collisions for dicts and sets. For example:
      01101010 11000011 01011110 00011000
      => 0000 01101010 11000011 01011110 0001 | 1000 0000 00000000 00000000 00000000
      => 1000 01101010 11000011 01011110 0001
    */

    size_t y = (size_t)p;
    y = (y >> 4) | (y << (8 * SIZEOF_VOID_P - 4));
    return (uhash_t)y;
}
```

2. **Convert key to a char**