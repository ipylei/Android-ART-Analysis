//[mem_map.h->MemMap类声明]
class MemMap {//为了方便讲解，代码的位置有所调整，下文代码均有类似处理。
public://先来看MemMap有哪些成员变量
    //每一个MemMap对象都有一个名字。
    const std::string& GetName() const { return name_;  }
    
    //针对这块映射内存的保护措施，比如是否可读、可写、可执行等。
    int GetProtect() const {return prot_;}
    
    /*MemMap类中有两组与内存映射地址与大小相关的成员，它们分别是：
      1：begin_和size_
      2：base_begin_和base_size_
        这两组成员的取值在基于文件的内存映射上可能有所不同。
        
        此处举一个例子。比如，我们想把某个文件[start,start+length]这段空间映射到内存。
        根据mmap的要求，start必须按内存页进行对齐（内存页大小一般是4KB），而length也必须是内存页大小的整数倍。
        所以，为了满足mmap的要求，base_begin_是start按内存页大小向下对齐后进行映射得到的映射内存的首地址，
        而base_size_是length按内存页大小向上对齐得到的长度。
        经过这种向下和向上对齐后处理后，文件的start以及length很可能与base_begin_和base_size_不匹配。
        所以，我们还需要两个变量来指明文件start处在映射内存里的起始位置（即base_）以及length（即size_）。
        所以，对文件映射而言，文件的[start,start+length]在映射内存中的真实位置为[base_,base_+size_] */
    uint8_t* Begin() const { return begin_;}        //file_off
    void* BaseBegin() const { return base_begin_;}  //页对齐
    size_t Size() const { return size_;}            //file_off
    size_t BaseSize() const {return base_size_;}    //页对齐

    /*ART对内存使用非常计较，很多地方都使用了内存监控技术。
    比如使用STL标准容器类，ART将监控容器里内存分配的情况。
    当然，监控内存分配是有代价的，所以ART设计了一个编译常量（constexpr）来控制是否启用内存监控。
    这部分内容留待下文统一介绍。
    下面这行代码中，Maps是 AllocationTrackingMultiMap 类型的别名，
    AllocationTrackingMultiMap 真实类型是multimap。
    简单推导即可：using AllocationTrackingMultiMap = std::multimap<Key, T, Compare, TrackingAllocator<std::pair<const Key, T>, kTag> >
                                                   = std::multimap<Key, T, Compare, std::allocator<kTag> >
    AllocationTracking 即是 TrackAllocation（跟踪分配）之意 
    
    Maps
        = AllocationTrackingMultiMap<void*, MemMap*, kAllocatorTagMaps> 
        = std::multimap<void*, MemMap*, Compare, std::allocator<kAllocatorTagMaps> >
    */
    typedef AllocationTrackingMultiMap<void*, MemMap*, kAllocatorTagMaps> Maps;
    
private:
    /*ART对线程安全的问题也是倍加小心。比如，下面这行代码在声明一个名为maps_的静态成员变量之后，
    还使用了一个GUARED_BY宏。宏里有一个参数。
    从其命名可以看出，mem_maps_lock_应该是一个互斥锁。
    所以，第一次看到这行代码的读者可能会猜测，这是想表达使用maps_时需要先拿到mem_maps_lock_锁的意思吧？
    猜得没错。更多关于GUARDED_BY宏的知识将留待下文介绍*/
    static Maps* maps_ GUARDED_BY(Locks::mem_maps_lock_);

public://现在来看MemMap有哪些重要的成员函数
    /*MapAnonymous：用于映射一块匿名内存。
    匿名内存映射就是指该内存映射不是针对文件的内存映射（详情可参考mmap的用法说明）。
    关于该函数的一些介绍如下：
    （1）先看返回值：调用成功的话将返回一个MemMap对象。调用失败的话则返回nullptr，
            error_msg用来接收调用失败的原因（字符串描述）。
    （2）输入参数中，
            name 字符串用于设置MemMap对象的name_成员变量，代表该对象的名称。
            addr 和 byte_count 代表所期望得到的内存映射地址以及该段映射内存的大小。
                简单来说，内存映射成功后将返回一个指向该段内存起始位置的内存地址指针，
                而这段映射内存的大小由byte_count指定。注意，内存映射时往往有地址对齐的要求。
                所以，期望的内存起始地址和真实得到的内存起始地址可以不同。
            prot 表示该映射内存的保护方式，
            reuse 表示是否重映射某块内存。
            use_ashmem 默认为true，表示将从android系统里特有的ashmem设备
                （一种用于进程间共享内存的虚拟设备。ashmem是anonymous shared memory的简写，表示匿名共享内存，是Android系统上常用的进程间共享内存的方法）
                中分配并映射内存。*/
    static MemMap* MapAnonymous(const char* name, uint8_t* addr,
                 size_t byte_count, int prot, bool low_4gb, bool reuse,
                 std::string* error_msg, bool use_ashmem = true);

    /* MapFile：将某个文件映射到内存，其中：
          （1）filename 代表文件名，它也会用作MemMap的name_成员。
          （2）start 和 byte_count 代表文件中[start,start+byte_count]这部分空间需映射到内存*/
    static MemMap* MapFile(size_t byte_count,int prot, int flags, 
            int fd,off_t start, bool low_4gb, const char* filename, std::string* error_msg) {
            return MapFileAtAddress(nullptr,....);//最终调用这个函数完成内存映射
        }
    //也可以直接调用下面这个函数完成对文件的内存映射
    static MemMap* MapFileAtAddress(uint8_t* addr, size_t byte_count, .....std::string* error_msg);


    //REQUIRES宏和上文介绍的 GUARED_BY 类似，详情见下文介绍
    ~MemMap() REQUIRES(!Locks::mem_maps_lock_);
    static void Init() REQUIRES(!Locks::mem_maps_lock_);
    static void Shutdown() REQUIRES(!Locks::mem_maps_lock_);


    private:
        //构造函数
        MemMap(const std::string& name,uint8_t* begin,size_t size,
            void* base_begin,size_t base_size,int prot,
            bool reuse,size_t redzone_size = 0) REQUIRES(!Locks::mem_maps_lock_);
            
        //该函数内部将调用mmap来完成实际的内存映射操作，读者可自行查看其代码
        static void* MapInternal(void* addr, size_t length,int prot, int flags,int fd, 
                off_t offset, bool low_4gb);
        .....;
    };
    
    
    
//上述MemMap代码中，我们提到一个展示ART对内存使用情况非常值得注意的证据，即AllocationTrackingMultiMap。为什么这么说呢？来看代码。    
//type_static_if.h
/*先看type_static_if.h：
 （1）它定义了一个包含三个模板参数的模板类 TypeStaticIf，其第一个模板参数为非数据类型的模板参数，名为condition。
 （2）type_static_if.h为condition为false的情况定义了一个偏特化的 TypeStaticIf 类。
    偏特化的TypeStaticIf类依然是一个模板类（只包含A和B两个模板参数）。
    
 （3）TypeStaticIf类定义了一个名为type的成员变量。它的数据类型由condition决定。
    即，condition为true时，type的类型为类型A，否则为类型B。
*/

//TypeStaticIf类，包含三个模板参数。type为成员变量，类型为A
template <bool condition, typename A, typename B>
struct TypeStaticIf { typedef A type; };

//偏特化condition为false后得到的新的模板类。此时，type的类型为B
template <typename A, typename B>
struct TypeStaticIf<false, A,  B> { typedef B type; };




//allocator.h->TrackingAllocator
template<class T, AllocatorTag kTag>
using TrackingAllocator = typename TypeStaticIf<
    kEnableTrackingAllocator,   //这里值为false
    TrackingAllocatorImpl<T, kTag>, 
    std::allocator<T>
    >::type;  //取出type成员，这里为std::allocator<T>
/*
·当 kEnableTrackingAllocator 为true时，type的类型为TypeStaticIf模板参数A，
    即上文代码里的 TrackingAllocatorImpl<T，kTag>。
·当 kEnaleTrackingAllocator 为false时，type的类型为TypeStaticIf模板参数B，
    即上文代码里的 std::allocator<T>。
    
    
  所以： TrackingAllocator =  TrackingAllocatorImpl<T, kTag> 
                               or 
                              std::allocator<T>   【*】
*/
    


//接着来看 AllocationTrackingMultiMap 的定义。
//allocator.h->AllocationTrackingMultiMap
template<class Key, class T, AllocatorTag kTag, class Compare = std::less<Key>>
using AllocationTrackingMultiMap 
= std::multimap<Key, T, Compare, 
    TrackingAllocator<std::pair<const Key, T>, kTag>
    >;
                                        
    
//最后，我们来看一下Maps的声明。    
//mem_map.h->MemMap中Maps的声明
typedef AllocationTrackingMultiMap<void*,MemMap*,kAllocatorTagMaps> Maps                                        
        





//[mem_map.h->MemMap声明]
class MemMap {
    /*GUARDED_BY宏，它针对变量，表示操作这个变量（读或写）前需要用某个互斥锁对象来保护。
    而用来保护这个变量的互斥锁对象作为GUARDED_BY宏的参数。
    注意，下面这行代码只是声明maps_这个变量，
    而它的定义（mem_map.cc中）也就是给maps_赋初值的地方则不受此限制。
    其余给maps_赋值或者操作它的地方则需要用mem_maps_lock_锁来保护*/
        static Maps* maps_ GUARDED_BY(Locks::mem_maps_lock_);
        
        
    /*RQUIRES宏，它针对函数，表示调用这个函数前需要（或者不需要）某个锁来保护，
        比如下面DumpMaps和DumpMapsLocked函数：
        
     （1）DumpMaps 使用REQUIRES(!Locks::mem_maps_lock_)：表示调用DumpMaps之前不要锁住mem_maps_lock_，
            DumpMaps内部会lock这个对象，并且DumpMaps退出前一定要释放mem_maps_lock_。
            
     （2）DumpMapsLocked 
            使用
            REQUIRES(Locks::mem_maps_lock_)：表示DumpMapsLocked调用前必须拿到（也就是调用者）mem_maps_lock_锁，
                并且DumpMapsLocked内部不允许释放mem_maps_lock_锁
            RQUIRES宏表明某个函数是否需要独占某个互斥对象：
               （1）如果需要，那么函数进来前（on entry）和退出（on exit）后，这个互斥对象都不能释放。
                注意退出后的含义，它是指该函数返回后，这个互斥对象还是被锁住的，即函数内部不会释放同步锁。
               （2）如果不需要，那么函数进来前和退出后，这个锁都不再需要。
                    另外，REQUIRE_SHARED宏用于表示共享某个互斥对象*/

}




//mem_map.cc->MemMap::DumpMaps
void MemMap::DumpMaps(std::ostream& os, bool terse) {
    /*MutexLock是ART封装的互斥锁工具类，此处笔者不拟详细介绍，其底层使用的是pthread相关功能。
        由于DumpMaps内部会锁住mem_maps_lock_对象，那么调用者就无须获取该锁了。
        另外，该锁会在mu对象析构的时候释放，即DumpMaps返回前，该锁会被释放。*/
    MutexLock mu(Thread::Current(), *Locks::mem_maps_lock_);
    //读者可以自行阅读该函数，其内部是不会使用mem_maps_lock_锁的。
    DumpMapsLocked(os, terse);
}



//mem_map.cc->使用maps_的地方
//定义maps_的地方，这种情况下Annotalysis不会进行检查。与之类似的还有类的构造和
//析构函数
MemMap::Maps* MemMap::maps_ = nullptr;
//其他使用
void MemMap::Init() {
    MutexLock mu(Thread::Current(), *Locks::mem_maps_lock_);
    if (maps_ == nullptr) {//Init中使用了mem_maps_lock_锁
        maps_ = new Maps;
    }
}