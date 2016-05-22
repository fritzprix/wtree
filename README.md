# wtree
> wtree is special case of binary heap for managing heap memory. binary heap (max heap) is a efficient algorithm for sorting element in specific order and so binary search tree does. I was fascinated to this point and wtree is an implementation effort of the idea. wtree can keep nodes in-order for two parameters at the same time. (here address & size) and I see this as good charateristics for heap. At first, heap has to search for valid memory chunk which satisfies requested size and allocate more memory segment from the system if requested chunk is not available. I believe this kind of task has been involved to fair amount of linear search or kind of but wtree keeps largest chunk on its top and makes this kind of task happen withing almost constant time.  and second, heap also has to free memory fast and efficiently(minimize fragmentation and keep chunk as large as possible). wtree behaves like binary search tree to keep its nodes in-order by chunk address. so heap can easily find its neighbored chunks within amortized logN time and if the nodes is consecutive in address they can be merged easily. this characteristics provides additional benefit by keeping tree depth within reasonable level while minimizing fragmentation within heap. it also improve cache hit rate(locality) and overall memory usage efficiency.


## Detail

## Dependencies
```
clang-3.6
python 2.7
pip
gnu make utility
jemalloc(for benchmark)
```

## How to build
> simple use
```
 > make config 
 > make all
```
> benchmark will consume 2GB of RAM and your CPUs at full load 
```
> make test
> ./yamalloc
```


## License
```
BSD-2
```

