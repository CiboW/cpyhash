# cpyhash

#### 介绍
这个项目克隆了cpython中的hashtable，并做了简化修改，只需引入一个头文件即可使用与cpython中几乎一致的hashtable。



#### Hashtable的结构

![cpython-hashtable结构](https://images.gitee.com/uploads/images/2020/1228/101235_cb10adcf_5034884.jpeg "cpython-hashtable.jpg")

* `entry`: hashtable中的一个条目
* `bucket`: 包含一个指向链表的头指针

* `*buckets`: `bucket`数组的指针

通过位运算将`entry`分配到不同的`bucket`中，并插入对应的链表。

```C
size_t index = key_hash & (ht->nbuckets - 1);	//index必定小于ht->nbuckets
```



#### 默认hash算法

1. **位运算**

   ```c
   uhash_t _Cpy_HashPointerRaw(const void *p)
   {
       /*将指针p赋值给y，直接通过位运算对y进行hash
         最后三四位可能是0，将y向右位移4位。但是后4位也不直接舍弃，
         将y左移(总位数-4)位，也就是把后四位移到前四位，再和右移的结果做或运算。例：
         01101010 11000011 01011110 00011000
         变换：0000 01101010 11000011 01011110 0001 | 1000 0000 00000000 00000000 00000000
         结果为：1000 01101010 11000011 01011110 0001
         
         这样可以避免过多的hash冲突
       */
   
       size_t y = (size_t)p;
       y = (y >> 4) | (y << (8 * SIZEOF_VOID_P - 4));
       return (uhash_t)y;
   }
   ```

2. **转换key为字符**