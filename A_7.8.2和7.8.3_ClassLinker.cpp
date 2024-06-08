//[class_linker.cc->ClassLinker::ClassLinker]
ClassLinker::ClassLinker(InternTable* intern_table)
          : 
          dex_lock_("ClassLinker dex lock", kDefaultMutexLevel),
          dex_cache_boot_image_class_lookup_required_(false),
          failed_dex_cache_class_lookups_(0),
          
          //class_roots_ 成员变量的类型是GcRoot<mirror::ObjectArray<mirror::Class>>
          //借助GcRoot的封装，它实际保存的信息应该是一个ObjectArray，
          //这个数组中的元素的数据类型是Class（即mirror Class类）
          class_roots_(nullptr), 
          
          array_iftable_(nullptr),
          find_array_class_cache_next_victim_(0), 
          init_done_(false),
          log_new_class_table_roots_(false),
          intern_table_(intern_table), 
          quick_resolution_trampoline_(nullptr),
          .....
          {
              
            std::fill_n(find_array_class_cache_, kFindArrayCacheSize, GcRoot<mirror::Class>(nullptr));
}







//[class_linker.cc->ClassLinker::InitFromBootImage 第一部分]
bool ClassLinker::InitFromBootImage(std::string* error_msg) {

    Runtime* const runtime = Runtime::Current();
    Thread* const self = Thread::Current();
    gc::Heap* const heap = runtime->GetHeap();
    
    //每一个 ImageSpace 对应一个art文件，所以返回的是一个ImageSpace数组
    std::vector<gc::space::ImageSpace*> spaces = heap->GetBootImageSpaces();
    image_pointer_size_ = spaces[0]->GetImageHeader().GetPointerSize();
    
    .....
    
    dex_cache_boot_image_class_lookup_required_ = true;
    
    /*runtime的GetOatFileManager返回runtime的成员变量oat_file_manager_，
    它指向一个OatFileManager对象。
    我们在7.3.2节中介绍过OatFileManager类，读者可回顾这部分内容。
    */
    std::vector<const OatFile*> oat_files = runtime->GetOatFileManager().RegisterImageOatFiles(spaces);
    
    //回顾7.3.2.1节可知，OAT文件有一个文件头信息，代码中由OatHeader类来描述。
    //下面将取出oat_files数组中第一个元素（对应文件为boot.oat）的OatHeader信息。
    //OatHeader的内容我们后续章节再介绍
    const OatHeader& default_oat_header = oat_files[0]->GetOatHeader();
    const char* image_file_location = oat_files[0]->GetOatHeader().GetStoreValueByKey(OatHeader::kImageLocationKey);
    
    //获取各个 trampoline(蹦床) 函数的地址，这部分内容以后再介绍
    //【10.1.2.4.3】 其实是来自oat文件头结构
    quick_resolution_trampoline_ = default_oat_header.GetQuickResolutionTrampoline();
	quick_imt_conflict_trampoline_ = default_oat_header.GetQuickImtConflictTrampoline();
	quick_generic_jni_trampoline_ = default_oat_header.GetQuickGenericJniTrampoline();
	quick_to_interpreter_bridge_trampoline_ = default_oat_header.GetQuickToInterpreterBridge();
    
    ......
    
    /*仔细看下面的关键代码，其执行顺序为：
     （1）GetImageRoot：返回值的类型为mirror:Object*，代码见下文介绍
     （2）然后通过down_cast宏，将返回值转换为ObjectArray<Class>*类型
     （3）然后再根据这个返回值构造一个CcRoot对象，类型参数是ObjectArray<Class> 
     
    
    ImageHeader的GetImageRoot返回的是mirror::Object*指针，
    但它实际上是一个 ObjectArray<mirror::Class>对象。
    所以，InitFromBootImage第一部分的最后通过 down_cast 宏将其向下转换成了子类类型的对象。
    最后再构造一个GcRoot对象赋值给 class_roots_。
    
    而这个数组的内容又是来自于art文件的 image_roots_ 所在的地方。
    也就说，class_roots_ 的内容保存在art文件的 image_roots_ 所在的区域。
    */
    class_roots_ = GcRoot<mirror::ObjectArray<mirror::Class>>(
                //转化为
                down_cast<mirror::ObjectArray<mirror::Class>*>(
                    spaces[0]->GetImageHeader().GetImageRoot(ImageHeader::kClassRoots)
                )
            );
            
            
            
    //[class_linker.cc->ClassLinker::InitFromBootImage 第一部分 继续 ]
    /*上文说过，class_roots_ 是一个ObjectArray<Class>数组，里边的元素类型为Class。
    也就是说，class_roots_ 保存的是一组Class对象，
    而这些Class对象就是Java语言中一些基本类（比如java.lang.Class、java.lang.String）信息的代表。
    读者想必知道，每一个Java类有一个代表它的class对象，
    该类的所有实例的getClass返回的将是同一个代表该类的【Class对象】。
    所以，对虚拟机而言，它需要准备好这些基本类的类信息，
    
    比如下面这行代码就是从 class_roots_ 数组中找到代表java.lang.Class的Class对象，
    然后将它设置为 Class 类的静态成员 java_lang_Class_
    */
    mirror::Class::SetClassClass(class_roots_.Read()->Get(kJavaLangClass));
    
    //kJavaLangClass、kJavaLangString 等都是对应class对象在 class_roots_ 中的索引，
    //它们是ClassRoot的枚举变量，包括37个基础类的枚举定义。
    mirror::String::SetClass(GetClassRoot(kJavaLangString));
    mirror::Class* java_lang_Object = GetClassRoot(kJavaLangObject);
    java_lang_Object->SetObjectSize(sizeof(mirror::Object));
    
    ......//设置其他mirror类的class信息
	
    mirror::DoubleArray::SetArrayClass(GetClassRoot(kDoubleArrayClass));
    
    ......
    
    
    
    
    //【*】InitFromBootImage 第二部分
    //[class_linker.cc->ClassLinker::InitFromBootImage 第二部分]
    
    //遍历各个.art文件对应的ImageSpace
    for (gc::space::ImageSpace* image_space : spaces) {
            // Boot class loader, use a null handle.
            std::vector<std::unique_ptr<const DexFile>> dex_files;
            if (!AddImageSpace(image_space,
                                ScopedNullHandle<mirror::ClassLoader>(),  //它表示传入一个空值ClassLoader对象（等同于nullptr）
                                /*dex_elements*/nullptr, 
                                /*dex_location*/nullptr, 
                                /*out*/&dex_files,
                                error_msg)) {
                        
                return false;      
            }
            boot_dex_files_.insert(boot_dex_files_.end(), 
                                    std::make_move_iterator(dex_files.begin()), 
                                    std::make_move_iterator(dex_files.end())
                );
    }
        
    //最后调用FinishInit完成ClassLinker的初始化。
    FinishInit(self);
    return true;
}





/*
class_roots_ 是 ClassLinker 的成员变量，其类型是GcRoot<mirror::ObjectArray<mirror::Class>>。
根据前面对 ClassLinker 里 GcRoot 的介绍可知：
    该成员变量包含的信息其实是其中的 ObjectArray。
    而这个 ObjectArray 中各个元素的类型是 Class。
    
那么，这些 Class 信息来自什么地方呢？代码中所示为spaces[0]（也就是boot.art）的Image Header所指向的地方。
追根溯源，现在我们回到ImageHeader去看看到底什么地方存储了这些class的信息。
*/
//[image-inl.h->ImageHeader::GetImageRoot]
//注意，该函数的返回值的类型为 mirror::Object
template <ReadBarrierOption kReadBarrierOption>
inline mirror::Object* ImageHeader::GetImageRoot(ImageRoot image_root) const {
    /*GetImageRoots 是ImageHeader定义的成员函数，
    它将返回ImageHeader中的 image_roots_ 成员变量，其类型为uint32_t，代表某个信息在art文件中的位置。
    如笔者上文所说，这个uint32_t的值将变成一个ObjectArray<Object>数组对象，
    GetImageRoots内部通过reinterpret_cast进行数据类型转换，从而得到下面的image_roots变量。
    */
    mirror::ObjectArray<mirror::Object>* image_roots = GetImageRoots<kReadBarrierOption>();
    
    /*ImageRoot 是枚举变量，包含两个有用的值，
        一个是 kDexCaches，值为0，
        另外一个是 kClassRoots，值为1。
    下面的Get函数（模板函数，先不考虑其模板参数）为ObjectArray的成员函数，用于其中获取指定索引的元素。
    */
    return image_roots->Get<kVerifyNone, kReadBarrierOption>(static_cast<int32_t>(image_root));
}
/*
简单来说，ImageHeader 的 image_roots_ 所指向的那块区域包含两组信息：
·第一个是ObjectArray<DexCache>数组。
·第二个是ObjectArray<Class>数组。
*/





//AddImageSpace内容比较多，针对 app image 和 boot image 还有不同的处理。本节仅先考虑boot image的情况。
//[class_linker.cc->ClassLinker::AddImageSpace]
bool ClassLinker::AddImageSpace(gc::space::ImageSpace* space,
								Handle<mirror::ClassLoader> class_loader,             //它表示传入一个空值ClassLoader对象（等同于nullptr）
								jobjectArray dex_elements,                            //nullptr
								const char* dex_location,                             //nullptr
								vector<unique_ptr<const DexFile>>* out_dex_files,     //dex_files
								string* error_msg) {
            
    /*注意调用时传入的参数：
     （1）class_loader 为一个 ScopedNullHandler 对象。显然，它等于与nullptr
     （2）dex_elements 和 dex_location 都为nullptr */
     
    //下面的判断也是和nullptr来做比较，此处的 app_image 将为false
    const bool app_image = class_loader.Get() != nullptr;
    
    const ImageHeader& header = space->GetImageHeader();
    
    //参考图7-3可知，ImageHeader::kDexCaches 保存的是 dex_caches_object 数组
    //注意它的返回值类型为mirror::Object*
    mirror::Object* dex_caches_object = header.GetImageRoot(ImageHeader::kDexCaches);
    
    .......//略过部分代码
    
    Handle<mirror::ObjectArray<mirror::DexCache>> dex_caches(hs.NewHandle(dex_caches_object->AsObjectArray<mirror::DexCache>()));
    const OatFile* oat_file = space->GetOatFile();
    
    //遍历 kDexCaches 数组
    for (int32_t i = 0; i < dex_caches->GetLength(); i++) {
        //根据art文件中的kDexCaches项，获取对应dex文件在oat文件中位置
        h_dex_cache.Assign(dex_caches->Get(i));
        std::string dex_file_location(h_dex_cache->GetLocation()->ToModifiedUtf8());
        
        ......
        
        //打开该【art文件】对应的【oat文件】。读者可回顾图7-3，oat文件中包含对应的dex文件，
        //所以下面这个 OpenOatDexFile 打开的就是这个dex文件
        std::unique_ptr<const DexFile> dex_file = OpenOatDexFile(oat_file, dex_file_location.c_str(),error_msg);
        
        ......
        
        if (app_image) {......}
        else {
				/*虚拟机保存boot class path相关信息，包括：
			   （1）dex文件信息保存到 ClassLinker 对象的 boot_class_path_ 成员中，其数据类型为vector<const DexFile*>
			   （2）根据DexFile信息构造 DexCache 对象，然后将其添加到 dex_caches_ 成员中，
					其数据类型为list<DexCacheData>。DexCacheData 为 DexCache 的辅助包装，定义于ClassLinker内部。
					DexCache在Java中也有对应类，用于存储从某个dex文件里提取出来的各种信息。
					这部分内容我们以后碰到再详述   */
				AppendToBootClassPath(*dex_file.get(), h_dex_cache);  
            }
            
        //将 dex_file 保存到 out_dex_files 数组中
        out_dex_files->push_back(std::move(dex_file));
    }
    
    ......
    
    ClassTable* class_table = nullptr;
    {
        WriterMutexLock mu(self, *Locks::classlinker_classes_lock_);
        /*上文曾说过，每一个 ClassLoader 对象对应有一个 class_table_ 成员。
        下面将判断 ClassLoader 的这个ClassTable对象是否存在，如果不存在，
        则创建一个ClassTable对象并将其与ClassLoader对象关联。
        不过，如果下面这个函数的参数为空（对本例而言，class_loader.Get就是返回nullptr），
        
        所以这里返回的class_table就是ClassLinker的 boot_class_table_ 成员，
        它用于保存boot class相关的Class对象。
        不过此次还没有往这个table中添加数据。*/
        class_table = InsertClassTableForClassLoader(class_loader.Get());
    }

    //从art文件中的 kSectionClassTable 区域提取信息
    ClassTable::ClassSet temp_set;
    const ImageSection& class_table_section = header.GetImageSection(ImageHeader::kSectionClassTable);
    
    //如果这个区域有保存信息，则将它添加到 temp_set (数据类型为 ClassSet )中
    const bool added_class_table = class_table_section.Size() > 0u;
    if (added_class_table) {
        const uint64_t start_time2 = NanoTime();
        size_t read_count = 0;
        //注意下面这行代码，调用ClassSet的构造函数
        temp_set = ClassTable::ClassSet(space->Begin() + class_table_section.Offset(), false, &read_count);
        if (!app_image) {
            dex_cache_boot_image_class_lookup_required_ = false;
        }
    }

        ......
        
    if (added_class_table) {
            WriterMutexLock mu(self, *Locks::classlinker_classes_lock_);
            //最终，来自art文件里 kSectionClassTable 中的Class信息都保存到 class_table 中了。
            class_table->AddClassSet(std::move(temp_set));
    }
    
    .....

}
/*

上述代码比较多，但核心内容其实只有两点：
·art文件头结构ImageHeader里的image_roots_是一个数组，
    该数组的第二个元素又是一个数组（ObjectArray<Class>）。
    它包含了37个基本类（由枚举值kJavaLangClass、kJavaLangString等标示）的类信息（由对应的mirror::Class对象描述）。

·但是Java基础类（严格意义上来说，是ART虚拟机 boot class，
    包括jdk相关类以及android所需要的基础类。读者可回顾图7-15）肯定不止37个（下文有笔者测试时得到的信息），
    这些其他的类信息则存储在ImageHeader的kSectionClassTable区域里，
    它包含了所有boot镜像文件里所加载的类信息。
*/



//最后，我们介绍下从art文件的 kSectionClassTable 这块区域是如何构造出对应的 ClassSet 对象的。
//如前节的介绍可知，ClassSet 是 HashSet 的类型别名
//[hash_set.h::HashSet构造函数]
template <class T, 
          class EmptyFn = DefaultEmptyFn<T>, 
          class HashFn = std::hash<T>,
          class Pred = std::equal_to<T>, 
          class Alloc = std::allocator<T>
          >
HashSet(    const uint8_t* ptr,          //space->Begin() + class_table_section.Offset()
            bool make_copy_of_data,      //false
            size_t* read_count           //size_t read_count = 0;
        ) {
        //注意参数，ptr 是 kSectionClassTable 区域的起始位置
        uint64_t temp;
        size_t offset = 0;
        
        offset = ReadFromBytes(ptr, offset, &temp);//
        num_elements_ = static_cast<uint64_t>(temp);//此次插入多少个元素
        
        offset = ReadFromBytes(ptr, offset, &temp);
        num_buckets_ = static_cast<uint64_t>(temp);//最大可能有多少个元素
        
        offset = ReadFromBytes(ptr, offset, &temp);
        elements_until_expand_ = static_cast<uint64_t>(temp);
        
        offset = ReadFromBytes(ptr, offset, &min_load_factor_);
        offset = ReadFromBytes(ptr, offset, &max_load_factor_);
        
        if (!make_copy_of_data) {
            owns_data_ = false;
            //对 ClassSet 而言，T的类型为mirror::Class。data_的类型是T*，代表一个数组
            data_ = const_cast<T*>(reinterpret_cast<const T*>(ptr + offset));
            offset += sizeof(*data_) * num_buckets_;
        } else {
            ......
        }
        *read_count = offset;
}
/*
上面这段代码其实描述了 kSectionClassTable 区域内容的组织方式，
前面几个字节描述诸如当前插入多少个元素（由HashSet成员变量 num_elements_ 表示）、
最大有多少个元素（对应 num_buckets_ 成员变量）之类等信息。

后面就是各个元素的内容。注意，元素必须为定长大小。
*/



//接下来看ClassLinker初始化阶段的最后一个函数，代码如下所示。
//[class_linker.cc->ClassLinker::FinishInit]
void ClassLinker::FinishInit(Thread* self) {
    // GetClassRoot 功能读者应该不再陌生了，它用于从 class_roots_ 数组中找到指定的Class对象。
    //下面返回的是java.lang.reference类的Class对象
    mirror::Class* java_lang_ref_Reference = GetClassRoot(kJavaLangRefReference);
    
    //class_roots_只保存了37个基础类的class对象，其他boot class则需要通过
    //FindSystemClass 函数来寻找。这个函数也是非常重要的函数，下文将简单介绍它。
    mirror::Class* java_lang_ref_FinalizerReference = FindSystemClass(self, "Ljava/lang/ref/FinalizerReference;");

    //从类信息里取出代表成员变量信息的ArtField对象。
    ArtField* pendingNext = java_lang_ref_Reference->GetInstanceField(0);
    ArtField* queue = java_lang_ref_Reference->GetInstanceField(1);
    ArtField* queueNext = java_lang_ref_Reference->GetInstanceField(2);
    
    //做一些校验检查，比如函数名是否一样、数据类型是否匹配等
    CHECK_STREQ(queueNext->GetName(), "queueNext");
    CHECK_STREQ(queueNext->GetTypeDescriptor(), "Ljava/lang/ref/Reference;");
    
    ......
    
    init_done_ = true; //设置init_done_为true，表示ClassLinker对象初始化完毕
}






//FindSystemClass是非常常用的操作，本节先简单看下其代码，之后还会详细讨论它。
//[class_linker-inl.h->ClassLinker::FindSystemClass]
inline mirror::Class* ClassLinker::FindSystemClass(Thread* self, const char* descriptor) {
    /*descriptor 是这个类的描述名。由于是寻找system class（系统类，其实就是boot class），
    所以下面的函数没有指定Class Loader */
    return FindClass(self, descriptor, ScopedNullHandle<mirror::ClassLoader>());
}





//[class_linker-inl.h->ClassLinker::FindClass]
mirror::Class* ClassLinker::FindClass(Thread* self, 
                                      const char* descriptor, 
                                      Handle<mirror::ClassLoader> class_loader) {
    .....
    
    if (descriptor[1] == '\0') {
    /*只有基础数据类型（如int、short）的类名描述才为1个字符。
    FindPrimitiveClass内部调用 GetClassRoot ，
    传入枚举值kPrimitiveByte、kPrimitiveInt即可获取如byte、int等基础数据类型的类信息*/
        return FindPrimitiveClass(descriptor[0]);
    }
    
    //如果是非基础类，则先根据描述名计算hash值
    const size_t hash = ComputeModifiedUtf8Hash(descriptor);
    //从class_loader对应的 ClassTable 中查找指定Class。如果class_loader为空，则从 boot_class_table_ 中寻找。
    mirror::Class* klass = LookupClass(self, descriptor, hash, class_loader.Get());
    
    if (klass != nullptr) {//确保这个类已经完成解析等相关工作
        return EnsureResolved(self, descriptor, klass);
    }
	
    ......//其他更为复杂的情况，后续章节再详述 【8.7.8】
	
}



//从class_loader对应的 ClassTable 中查找指定Class
mirror::Class* ClassLinker::LookupClass(Thread* self,
                                        const char* descriptor,
                                        size_t hash,
                                        mirror::ClassLoader* class_loader) {
  {
    ReaderMutexLock mu(self, *Locks::classlinker_classes_lock_);
    
    //return class_loader == nullptr ? &boot_class_table_ : class_loader->GetClassTable();
    ClassTable* const class_table = ClassTableForClassLoader(class_loader);
    
    if (class_table != nullptr) {
      mirror::Class* result = class_table->Lookup(descriptor, hash);
      if (result != nullptr) {
        return result;
      }
    }
  }
  
  if (class_loader != nullptr || !dex_cache_boot_image_class_lookup_required_) {
    return nullptr;
  }
  
  // Lookup failed but need to search dex_caches_.
  mirror::Class* result = LookupClassFromBootImage(descriptor);
  if (result != nullptr) {
    result = InsertClass(descriptor, result, hash);
  } else {
    // Searching the image dex files/caches failed, we don't want to get into this situation
    // often as map searches are faster, so after kMaxFailedDexCacheLookups move all image
    // classes into the class table.
    constexpr uint32_t kMaxFailedDexCacheLookups = 1000;
    if (++failed_dex_cache_class_lookups_ > kMaxFailedDexCacheLookups) {
      AddBootImageClassesToClassTable();
    }
  }
  return result;
}



...
...
...
...



//ClassLinker部分功能函数介绍
//7.8.4.2.1　RegisterClassLoader
//当我们创建一个新的 ClassLoader 对象的时候，需要注册到ClassLinker对象中，代码如下所示。
//[class_linker.cc->ClassLinker::RegisterClassLoader]
void ClassLinker::RegisterClassLoader(mirror::ClassLoader* class_loader) {
    Thread* const self = Thread::Current();
    
    //构造一个ClassLoaderData对象，它是一个简单包装类
    ClassLoaderData data;
    
    //创建一个Weak Gloabl Reference对象。这部分和JNI有关，我们后续再介绍
    data.weak_root = self->GetJniEnv()->vm->AddWeakGlobalRef(self, class_loader);
    //创建一个ClassTable对象
    data.class_table = new ClassTable; 
    //给这个ClassLoader对象设置一个ClassTable对象，用于保存这个ClassLoader所加载的类
    class_loader->SetClassTable(data.class_table);
    
    //设置一个内存资源分配器
    data.allocator = Runtime::Current()->CreateLinearAlloc();
    class_loader->SetAllocator(data.allocator);
    
    //这个ClassLoaderData对象保存到ClassLinker的 class_loaders_ 容器中
    class_loaders_.push_back(data);
}




//7.8.4.2.2　FindArrayClass
//搜索某个数组类对应的Class对象时，将调用下面这个函数。
//[class_linker-inl.h->ClassLinker::FindArrayClass]
inline mirror::Class* ClassLinker::FindArrayClass(Thread* self, mirror::Class** element_class) {
    //先判断ClassLinker是否缓存过这个数组类的类信息
    for (size_t i = 0; i < kFindArrayCacheSize; ++i) {
        mirror::Class* array_class = find_array_class_cache_[i].Read();
        if (array_class != nullptr && array_class->GetComponentType() == *element_class) { 
            return array_class;  //找到符合要求的类了
        }
    }
    
    //如果缓存中没有这个数组类的类信息，则通过 FindClass 进行搜索
    std::string descriptor = "[";
    std::string temp;
    descriptor += (*element_class)->GetDescriptor(&temp);
    StackHandleScope<2> hs(Thread::Current());
    Handle<mirror::ClassLoader> class_loader(hs.NewHandle((*element_class)->GetClassLoader()));
    HandleWrapper<mirror::Class> h_element_class(hs.NewHandleWrapper(element_class));
    mirror::Class* array_class = FindClass(self, descriptor.c_str(), class_loader);
    
    //如果搜索到目标数组类的信息，则将其缓存起来。
    if (array_class != nullptr) {
        size_t victim_index = find_array_class_cache_next_victim_;
        //保存到 find_array_class_cache_ 数组中，由于它最多只能保存16个数组类类信息，
        //所以，之前保存的类信息将被替代。
        //find_array_class_cache_next_victim_ 成员指示下一个要存入缓存数组里的索引，
        //原索引位置上的类信息将被新的类信息替代
        find_array_class_cache_[victim_index] = GcRoot<mirror::Class>(array_class);
        find_array_class_cache_next_victim_ = (victim_index + 1) % kFindArrayCacheSize;
    } else {
			
			......
	
	}


    return array_class;
}