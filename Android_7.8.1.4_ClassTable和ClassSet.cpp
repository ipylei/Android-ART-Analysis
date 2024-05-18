//ClassTable是一个容器类，它被ClassLoader用于管理该ClassLoader所加载的类。马上来看下它的声明。
//[class_table.h]
class ClassTable {
public:
    /*内部类，它重载了()函数调用运算符，
    如果返回值为bool变量，则用于比较两个mirror class的类描述符是否相同
    如果返回值为uint32_t，则用于计算描述符的hash值
    */
    class ClassDescriptorHashEquals {
        public:
            //重载函数调用运算符
            uint32_t operator()(const GcRoot<mirror::Class>& root) const;
            bool operator()(const GcRoot<mirror::Class>& a, const GcRoot<mirror::Class>& b) const ;
            bool operator()(const GcRoot<mirror::Class>& a, const char* descriptor) const;
            uint32_t operator()(const char* descriptor) const;
    };
    
    //内部类，用于清空某个位置上的元素。
    class GcRootEmptyFn {
    public:
        void MakeEmpty(GcRoot<mirror::Class>& item) const {
            item = GcRoot<mirror::Class>();
        }
        bool IsEmpty(const GcRoot<mirror::Class>& item) const {
            return item.IsNull();
        }
    };
    
    /*创建类型别名 ClassSet。其真实类型是 HashSet，
    它是ART自定义的容器类，有5个模板参数，从左至右分别为
        容器中元素的数据类型，
        元素从容器中被移除时所调用的处理类，
        生成hash值的辅助类，
        比较两个hash值是否相等的辅助类，
        以及Allocator类
    */
    typedef HashSet<
                GcRoot<mirror::Class>,        //容器中元素的数据类型
                GcRootEmptyFn,                //元素从容器中被移除时所调用的处理类
                ClassDescriptorHashEquals,    //生成hash值的辅助类
                ClassDescriptorHashEquals,    //比较两个hash值是否相等的辅助类
                TrackingAllocator<GcRoot<mirror::Class>, kAllocatorTagClassTable>   //Allocator类
            >
            ClassSet;

    ClassTable();

    // 判断此ClassTable是否包含某个class对象
    bool Contains(mirror::Class* klass);
    
    ......
    
    //查找该ClassTable中是否包含有指定类描述符或hash值的mirror class对象
    mirror::Class* Lookup(const char* descriptor, size_t hash);
    mirror::Class* LookupByDescriptor(mirror::Class* klass);
    
    void Insert(mirror::Class* klass);  //保存一个class对象
    bool Remove(const char* descriptor);//移除指定描述符的class对象
    void AddClassSet(ClassSet&& set);   //将set容器里的内容保存到自己的容器中
    
private:
    mutable ReaderWriterMutex lock_;
    //ClassTable内部使用两个vector来作为实际的元素存储容器，classes_ 的元素类型为 ClassSet
    std::vector<ClassSet> classes_ GUARDED_BY(lock_);
    std::vector<GcRoot<mirror::Object>> strong_roots_;
};


//本节简单了解一下ClassTable中Insert函数的实现，代码如下所示。
//[class_table.cc->ClassTable::Insert]
void ClassTable::Insert(mirror::Class* klass) {
    WriterMutexLock mu(Thread::Current(), lock_);
    
    //classes_的类型为vector，其back()返回vector中最后一个ClassSet元素，
    //然后调用ClassSet的Insert函数，其实就是 HashSet 的Insert函数
    classes_.back().Insert(GcRoot<mirror::Class>(klass));
}

//[hash_set.h->HashSet::Insert]
void Insert(const T& element) {
    /*hashfn_ 为 ClassSet 类型别名定义时传入的第三个模板参数。
    此处将调用ClassDescriptorHashEquals的operator()(const GcRoot<mirror::Class>& root)操作符重载函数，
    返回的是一个uint32_t类型的hash值。此处不讨论 InsertWithHash 的实现，
    它与ART自定义容器类 HashSet 的实现有关，感兴趣的读者可自行阅读。*/
    InsertWithHash(element, hashfn_(element));
}