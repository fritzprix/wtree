# wtree
> wtree is special case of binary heap for managing heap memory. binary heap (max heap) is a efficient tool for sorting element in specific parameter. and binary search tree itself is also an good at keeping node in-order for a paramter. and I was fascinated to this point. so wtree is an implementation of the idea. wtree can keep nodes in-order for both two parameter (here address & size) and I see this as good charateristics for heap. heap has to search for valid memory chunk which satisfies requested size and if heap knows which chunk is largest one and can access it fast if this is possible heap can make decision whether to map additional segment from system or to respond the request. heap also has to free memory fast and efficiently(minimize fragmentation and keep chunk as large as possible). binary search tree keeps its nodes in-order by chunk address. so heap can easily find its sibling chunks with amortized logN and if the nodes is consecutive in address they can be merged and this property provides additional benefit by keeping tree depth within reasonable level while minimizing fragmentation within heap. 


## Detail


## License
```
BSD-2
```

