//【7.6.1.1.1】　SpaceBitmap的创建
//SpaceBitmap对象可由其静态的Create函数来创建，代码如下所示。
/*

内存块
	heap_begin
	heap_capacity(字节)
【
 	obj0  ptr
	obj1  ptr
	obj2  ptr
	obj3  ptr
	
	... 
	
	obj  ptr
	...
】

位图
	bitmap_begin
	bitmap_size(字节)
【
	1      index 0
	1
	1
	1
    1
	1
	1
	1
	
	1       index 1
	1
	1
	1
	1
	1
	1
	1
	...
】

*/



//[space_bitmap.cc->SpaceBitmap<kAlignment>::Create]
template<size_t kAlignment>
SpaceBitmap<kAlignment>* SpaceBitmap<kAlignment>::Create(const std::string& name, 
														 uint8_t* heap_begin, 
														 size_t heap_capacity) {
    /*
	heap_begin 和 heap_capacity 代表【位图】【所对应】的内存块的基地址以及该内存块的大小（参考图7-12中上下两个方框）。
	
	omputeBitmapSize 将 heap_capacity 根据对齐大小（模板参数kAlignment表示）进行计算
	得到【位图】本身所需的字节数（即图7-12中间方框所需的字节数）。
	*/
    const size_t bitmap_size = ComputeBitmapSize(heap_capacity);
    std::string error_msg;
	
    //SpaceBitmap使用MemMap来创建存储位图存储空间所需的内存，bitmap_size为该位图存储空间的长度（以字节计算）
    std::unique_ptr<MemMap> mem_map(MemMap::MapAnonymous(name.c_str(), 
														 nullptr, 
														 bitmap_size,
														 PROT_READ | PROT_WRITE, 
														 false, 
														 false, 
														 &error_msg));
    //根据MemMap对象来构造一个SpaceBitmap对象
    return CreateFromMemMap(name, mem_map.release(), heap_begin, heap_capacity);
}



//接着看 CreateFromMemMap 函数，非常简单。
//[space_bitmap.cc->SpaceBitmap<kAlignment>::Create]
template<size_t kAlignment>
SpaceBitmap<kAlignment>* SpaceBitmap<kAlignment>::CreateFromMemMap(const std::string& name, 
																	MemMap* mem_map, 
																	uint8_t* heap_begin, 
																	size_t heap_capacity) {
    //位图存储空间的起始地址，即图7-12里中间方框的pbitmap
    uintptr_t* bitmap_begin = reinterpret_cast<uintptr_t*>(mem_map->Begin());
    const size_t bitmap_size = ComputeBitmapSize(heap_capacity);
    /*
	调用SpaceBitmap的构造函数，它保存了位图功能所需的基本信息
		（如位图存储空间地址、位图存储空间长度、对应内存的基地址等），
	还保存了MemMap对象以及一个名称。
	*/
    return new SpaceBitmap(name, mem_map, bitmap_begin, bitmap_size, heap_begin);
}



//SpaceBitmap实例化了两个支持不同对齐大小的类，如下所示。
//[space_bitmap.h]
//kObjectAlignment 为8个字节，kLargeObjectAlignment 为4KB
typedef SpaceBitmap<kObjectAlignment> ContinuousSpaceBitmap;
typedef SpaceBitmap<kLargeObjectAlignment> LargeObjectBitmap;



//【7.6.1.1.2】　存储对象的地址值到位图中
//现在来看看如何将一个对象的地址存储到位图中，代码如下所示。
//[space_bitmap.h->SpaceBitmap::Set]
bool Set(const mirror::Object* obj) ALWAYS_INLINE {
    return Modify<true>(obj);//obj是一个内存地址。Modify 本身是又是一个模板函数
}

//[space_bitmap-inl.h->SpaceBitmap::Modify]
template<size_t kAlignment> template<bool kSetBit>
inline bool SpaceBitmap<kAlignment>::Modify(const mirror::Object* obj) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(obj);
    //offset是 obj 与 heap_begin_（内存基地址）的偏移量
    const uintptr_t offset = addr - heap_begin_;
	
    //先计算这个【偏移量】落在哪个字节中
    const size_t index = OffsetToIndex(offset);
    //再计算这个【偏移量】落在字节的哪个比特位上
    const uintptr_t mask = OffsetToMask(offset);
	
    //用index取出位图对应的字节（注意，位图存储空间是以字节为单位的，而不是以比特位为单位）
    uintptr_t* address = &bitmap_begin_[index];
    uintptr_t old_word = *address;
	//kSetBit为true的话，表示往位图中存储某个地址
    if (kSetBit) {
		//如果该比特位已经设置了，则不再设置；为0表示没有设置 (未设置:0 & 1 = 0； 已设置:1 & 1 = 1)
        if ((old_word & mask) == 0) {
            *address = old_word | mask;//设置mask比特位
        }
    } 
	else {
        //取消mask比特位，这相当于从位图中去除对应位置所保存的地址
        *address = old_word & ~mask;
    }
    return (old_word & mask) != 0;
}





//【7.6.1.1.3】　遍历位图
//除了存储和移除信息外，SpaceBitmap还可以遍历内存，来看代码，如下所示。
//[space_bitmap.cc->SpaceBitmap::Walk]
template<size_t kAlignment>
void SpaceBitmap<kAlignment>::Walk(ObjectCallback* callback, void* arg) {
    //注意这个函数的参数，callback 是回调函数，每次从位图中确定一个对象的地址后将回调它
    uintptr_t end = OffsetToIndex(HeapLimit() - heap_begin_ - 1);
    uintptr_t* bitmap_begin = bitmap_begin_;
	
	//遍历【位图结束】的字节
    for (uintptr_t i = 0; i <= end; ++i) {
		//取出当前字节，转为整型？
        uintptr_t w = bitmap_begin[i];
		//表明里面肯定至少有1 bit有对应的对象
        if (w != 0) {
			//IndexToOffset(index) = static_cast<T>(index * kAlignment * kBitsPerIntPtrT);
			//static constexpr int kBitsPerIntPtrT = sizeof(intptr_t) * kBitsPerByte;
			//static constexpr size_t kBitsPerByte = 8;
			//kAlignment模板参数表示内存地址对齐长度
			
			//所以，综上， 这里代表位图第i个字节第0bit位代表的内存地址
            uintptr_t ptr_base = IndexToOffset(i) + heap_begin_;
            do {
                const size_t shift = CTZ(w);//w中末尾为0的个数，也就是第一个值为1的索引位
                //计算该索引位所存储的对象地址值，注意下面代码行中计算对象地址的公式
                mirror::Object* obj = reinterpret_cast<mirror::Object*>(ptr_base + shift * kAlignment);
                (*callback)(obj, arg);//回调callback，arg是传入的参数
                w ^= (static_cast<uintptr_t>(1)) << shift;//清除该索引位的1，继续循环
			} 
			while (w != 0);
		}
    }
}


w = w ^ 1 << shift

若  w = 5  0000 0101，则shift = 0
所以w = 5 ^ 1 << 0 = 5 ^ 1 = 4

若  w = 4  0000 0100，则shift = 2
所以w = 4 ^ 1 << 2 = 4 ^ 4 =0