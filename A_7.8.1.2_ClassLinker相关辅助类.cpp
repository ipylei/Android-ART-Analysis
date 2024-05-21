//7.8.1.2.1
//首先来认识ObjectReference类，只要看它的类声明相关代码即可。
//[object_reference.h->ObjectReference]
template<bool kPoisonReferences, class MirrorType>
class  ObjectReference {
    /*ObjectReference是模板类。模板参数中的kPoisonReferences是bool变量，
    而 MirrorType 则是代表数据类型的模板参数，比如图7-20中的那些类。*/
public:
    friend class Object;//声明友元类
    
    //代表一个引用，指向一个mirror对象，但具体如何解析这个 reference_，则和 kPoisonReferences 的取值有关
    uint32_t reference_;
    
    //将reference_还原成对象
    MirrorType* AsMirrorPtr() const {  return UnCompress();  }
    
    //保存一个mirror对象，传入的参数是一个指向mirror对象的指针。
    //Assgien内部将这个指针转换成reference_
    void Assign(MirrorType* other) { reference_ = Compress(other); }
    
    //清除自己所保存的mirror对象
    void Clear() {     reference_ = 0;   }
    
    //判断是否持有一个非空的mirror对象
    bool IsNull() const { return reference_ == 0;  }

    uint32_t AsVRegValue() const { return reference_;  }

protected:
    //构造函数
    ObjectReference<kPoisonReferences, MirrorType>(MirrorType* mirror_ptr): reference_(Compress(mirror_ptr)) { }
        
    //指针转换成 reference_ 的方法，很简单
    static uint32_t Compress(MirrorType* mirror_ptr) {
        uintptr_t as_bits = reinterpret_cast<uintptr_t>(mirror_ptr);
        //kPoisonReferences 为true的话，存的是负数
        return static_cast<uint32_t>(kPoisonReferences ? -as_bits : as_bits);
    }
    
    //从 reference_ 还原为mirror对象的指针
    MirrorType* UnCompress() const {
        uintptr_t as_bits = kPoisonReferences ? -reference_ : reference_;
        return reinterpret_cast<MirrorType*>(as_bits);
    }
}




//地址转换方式有两种，所以 ObjectReference 又派生出两个模板类，来看代码。
//[object_reference.h->HeapReference和CompressedReference]
// kPoisonHeapReferences 是一个编译常量，默认为false
template<class MirrorType>
class HeapReference : public ObjectReference<kPoisonHeapReferences,MirrorType>

template<class MirrorType>
class CompressedReference : public mirror::ObjectReference<false,MirrorType>

//HeapReference和CompressedReference都是由ObjectReference派生，
//并且第一个模板参数kPoisonReferences的取值都是false。在后续代码中我们将看到使用它们的地方。




/*7.8.1.2.2
除了ObjectReference外，ART中还有一个GcRoot，其相关类的信息如图7-21所示。
一个GcRoot对象通过它的 root_ 成员变量包含一个mirror对象。
当然，这个mirror对象不是直接引用的，而是借助了上述的CompressedReference对象。
当我们要从这个GcRoot对象中读取这个mirror对象的时候，就需要使用GcRoot的Read函数。
这个函数比较复杂，下文将简单介绍它，但本节不会分析其实现。

RootVisitor 是一个类，配合GcRoot的VisitRoot函数使用。见下文VisitRoot的函数声明
*/
//[gc_root.h->GcRoot]
template<class MirrorType>
class GcRoot {
public:
    //当我们要从这个GcRoot对象中读取这个mirror对象的时候，就需要使用GcRoot的Read函数
    //Read是模板类的模板函数，实现比较复杂。先不考虑其实现，该函数的返回是一个mirror对象
    template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
    MirrorType* Read(GcRootSource* gc_root_source = nullptr) const;
    
    //使用外界传入的visitor来访问root_
    void VisitRoot(RootVisitor* visitor, const RootInfo& info) const{
        mirror::CompressedReference<mirror::Object>* roots[1] = { &root_ };
        visitor->VisitRoots(roots, 1u, info);
    }
    ......
private:
    //GcRoot借助CompressedReference对象来间接持有一个mirror对象
    mutable mirror::CompressedReference<mirror::Object> root_;
};




//7.8.1.2.3　HandleScope等
class ValueObject -> class Handle<T> -> class MutableHandle
                                     -> class ScopedNullHandle

class HandleScope -> class StackHandleScope                      


//[handle.h->Handle的声明]
template<class T>
class Handle : public ValueObject {
public:
    //通过reference_间接持有一个mirror对象
    StackReference<mirror::Object>* reference_;

    //构造函数，初始化字段 reference_
    Handle() : reference_(nullptr) {  }
    
    //复制构造函数，初始化字段 reference_
    Handle(const Handle<T>& handle) : reference_(handle.reference_) { }
    
    //重载的赋值运算符
    Handle<T>& operator=(const Handle<T>& handle) {
        reference_ = handle.reference_;
        return *this;
    }
    
    //重载了几个操作符，关键实现是Get函数
    T& operator*() const {return *Get();}
    T* operator->() const {return Get();}
    
    //Get函数非常简单，通过AsMirrorPtr直接得到mirror对象的指针
    T* Get() const { 
        return down_cast<T*>(reference_->AsMirrorPtr()); 
   }
}
//Handle比较简单，其实是对 StackReference 类的进一步包装。






//接着来了解下StackHandleScope类，其类声明的代码如下所示。
//[handle_scope.h-> StackHandleScope 类声明]
template<size_t kNumReferences>
class PACKED(4) StackHandleScope FINAL : public HandleScope {
    //StackHandleScope是一个模板类，Stack表示这个对象及它所创建的Handle对象都位于内存栈上。
    //模板参数 kNumReferences 表示这个StackHandleScope能创建多少个Handle对象。
    //注意，它的第一个参数为 Thread 对象，这表明StackHandleScope是和调用线程有关的
public:
    //构造函数
    explicit StackHandleScope(Thread* self, mirror::Object* fill_value = nullptr);
    
    //创建一个 MutableHandle 对象
    template<class T>
    MutableHandle<T> NewHandle(T* object);
    
    ......
    
    //为第i个 Handle 对象设置一个mirror对象
    void SetReference(size_t i, mirror::Object* object);
    
    Thread* Self() const { return self_; }
    
private:
    //StackReference数组，可存储 kNumReferences 个元素
    StackReference<mirror::Object> storage_[kNumReferences];
    
    Thread* const self_;
    size_t pos_;//storage_数组中当前有多少个元素，pos_不能超过kNumReferences
};


//现在通过一段示例点看看如何使用StackHandleScope。
//[StackHandleScope参考示例-来自class_linker.cc]
//创建一个StackHandleScope，模板参数为2，表示这个封装类可以容纳两个对象
StackHandleScope<2> hs(Thread::Current());

//调用hs的NewHandle，创建一个对象
Handle<mirror::DexCache> dex_cache(hs.NewHandle(referrer->GetDexCache()));

//创建第二个对象
Handle<mirror::ClassLoader> class_loader(hs.NewHandle(referrer->GetClassLoader()));

//外界使用StackHandleScope的方法大致就是如此。
//当然StackHandleScope内部还是有很多操作的，这一部分内容本节先不讨论。