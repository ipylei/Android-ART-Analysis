//8.7.8.1　Resolve相关函数


//8.7.8.1.1　ResolveType  【寻找类，并保存到 DexCache 对象中】
//[class_linker.h->ClassLinker::ResolveType]
mirror::Class* ClassLinker::ResolveType(const DexFile& dex_file,
                                        uint16_t type_idx,
                                        Handle<mirror::DexCache> dex_cache,
                                        Handle<mirror::ClassLoader> class_loader);
                                        
                                        
//[class_linker.cc->ClassLinker::ResolveType]
mirror::Class* ClassLinker::ResolveType(const DexFile& dex_file,
                                       uint16_t type_idx,......) {
    /*dex_cache 的类型为 mirror::DexCache，这里直接回顾本章上文对DexCache类的介绍。
      它包含如下几个关键成员变量：
      （1）dex_file_(类型为uint64_t)：实际为DexFile*，指向该对象关联的那个Dex文件。
      （2）resolved_fields_(uint64_t)：实际为ArtField*，指向ArtField数组，成员的数据类
          型为ArtField。该数组存储了一个Dex文件中定义的所有类的成员变量。另外，只有那些经解
          析后得到的ArtField对象才会存到这个数组里。该字段和Dex文件里的field_ids数组有关。
      （3）resolved_methods_(uint64_t)：实际为ArtMethod*，指向ArtMethod数组，成员的
          数据类型为ArtMethod。该数组存储了一个Dex文件中定义的所有类的成员函数。另外，只有
          那些经解析后得到的ArtMethod对象才会存到这个数组里。该字段和Dex文件里的
          method_ids数组有关。
      （4）resolved_string_(uint64_t)：实际为GCRoot<String>*，指向GcRoot<String>数
          组，包括该dex文件里使用的字符串信息数组。String是mirror::String。该字段和Dex
          文件的string_ids数组有关
      （5）resolved_classes_(uint64_t)：实际为GCRoot<Class>*，指向GcRoot<Class>数组，成
          员的数据类型为GcRoot<Class>，存储该dex文件里使用的数据类型信息数组。该字段和Dex
          文件里的type_ids数组有关    */

    //从dex_cache里找到是否已经缓存过 type_idx 所代表的那个Class信息
    mirror::Class* resolved = dex_cache->GetResolvedType(type_idx);
    
    //如果没有缓存过，则需要找到并存起来
    if (resolved == nullptr) {            
        Thread* self = Thread::Current();
        //找到这个type的字符串描述
        const char* descriptor = dex_file.StringByTypeIdx(type_idx);
        //搜索这个字符串对应的类是否存在。FindClass在第7章中曾简单介绍过，后文还会详细讨论它
        resolved = FindClass(self, descriptor, class_loader);
        if (resolved != nullptr) {      
            //类信息保存到DexCache对象中
            dex_cache->SetResolvedType(type_idx, resolved);
        } else {
            ...... //抛NoClassDefFoundError异常
            ThrowNoClassDefFoundError("Failed resolution of: %s", descriptor);
        }
    }
    return resolved;
}




//8.7.8.1.2 ResolveMethod 【寻找目标方法，并保存到DexCache对象中】
//[class_linker.h->ClassLinker::ResolveMethod]
ArtMethod* ClassLinker::ResolveMethod(const DexFile& dex_file,
                                     uint32_t method_idx, 
                                     Handle<mirror::DexCache> dex_cache,
                                     Handle<mirror::ClassLoader> class_loader, 
                                     ArtMethod* referrer,
                                     InvokeType type);

//[class_linker.cc->ClassLinker::ResolveMethod]
template <ClassLinker::ResolveMode kResolveMode>
ArtMethod* ClassLinker::ResolveMethod(const DexFile& dex_file,
                                     uint32_t method_idx, 
                                     Handle<mirror::DexCache> dex_cache,
                                     Handle<mirror::ClassLoader> class_loader, 
                                     ArtMethod* referrer,
                                     InvokeType type){
    //和ResolveType类似，首先判断dex_cache中是否已经解析过这个方法了。
    ArtMethod* resolved = dex_cache->GetResolvedMethod(method_idx, image_pointer_size_);
    if (resolved != nullptr && !resolved->IsRuntimeMethod()) {
        if (kResolveMode == ClassLinker::kForceICCECheck) {
            //Java有诸如1.5、1.6这样的版本，在早期Java版本里，有些信息和现在的版本有差异，
            //此处将检查是否有信息不兼容的地方（即check incompatible class change），
            //如果检查失败，则会设置一个IncompatibleClassChangeError异常，笔者此处不拟讨论。
            if (resolved->CheckIncompatibleClassChange(type)) {
                    ......      //设置异常
                return nullptr;
            }
        }
        return resolved;
    }
    
    // 如果dex_cache里并未缓存，则先解析该方法所在类的类型（由 method_id.class_idx_ 表示）。
    const DexFile::MethodId& method_id = dex_file.GetMethodId(method_idx);
    mirror::Class* klass = ResolveType(dex_file, method_id.class_idx_, dex_cache, class_loader);
    if (klass == nullptr) {
      DCHECK(Thread::Current()->IsExceptionPending());
      return nullptr;
    }
   
    switch (type) { //type是指该函数的调用类型
        case kDirect:
        case kStatic:
            /*FindDirectMethod 是mirror Class的成员函数，有三个同名函数。在此处调用的函数中，
              将沿着klass向上（即搜索klass的父类、祖父类等）搜索类所声明的direct方法，然后比较
              这些方法的method_idx是否和输入的method_idx一样。如果一样，则认为找到目标函数。注
              意，使用这种方法的时候需要注意比较method_idx是否相等时只有在二者保存在同一个DexCache
              对象时才有意义。显然，这种一种优化搜索方法。   */
            resolved = klass->FindDirectMethod(dex_cache.Get(), method_idx,image_pointer_size_);
            break;
        case kInterface:
            if (UNLIKELY(!klass->IsInterface())) { 
                return nullptr; 
            }
            else {      
                //如果调用方式是kInterface，则搜索klass及祖父类中的virtual方法以及所实现的接口类里的成员方法。
                resolved = klass->FindInterfaceMethod(dex_cache.Get(), method_idx, image_pointer_size_);
            }
            break;
            
        ...... //其他处理
        break;
        
        default: 
            UNREACHABLE();
    }
    
    //如果通过method_idx未找到对应的ArtMethod对象，则尝试通过函数名及签名信息再次搜索。
    //通过签名信息来查找匹配函数的话就不会受制于同一个DexCache对象的要求，但比较字符串的
    //速度会慢于上面所采用的比较整型变量method_idx的处理方式。
    if (resolved == nullptr) {
        //name是指函数名
        const char* name = dex_file.StringDataByIdx(method_id.name_idx_);
        //signature包含了函数的签名信息，就是函数参数及返回值的类型信息
        const Signature signature = dex_file.GetMethodSignature(method_id);
        switch (type) {
            case kDirect:
            case kStatic:      //调用另外一个FindDirectMethod，主要参数是signature
                resolved = klass->FindDirectMethod(name, signature, image_pointer_size_);
            break;
            ......
        }
    }
    if (LIKELY(resolved != nullptr && !resolved->CheckIncompatibleClassChange(type))) {
        //如果找到这个方法，则将其存到 dex_cache 对象中，以method_idx为索引，存储在它的 resolved_methods_ 成员中
        dex_cache->SetResolvedMethod(method_idx, resolved, image_pointer_size_);
        return resolved;
    } else { 
        ....../*其他处理 */ 
    }
}

                                     
                                     
                                     
                                     
//8.7.8.1.3　ResolveString                                     
//[class_linker.cc->ClassLinker::ResolveString]
mirror::String* ClassLinker::ResolveString(const DexFile& dex_file,
                                           uint32_t string_idx, 
                                           Handle<mirror::DexCache> dex_cache) {
    //先看看dex_cache是否缓存过了
    mirror::String* resolved = dex_cache->GetResolvedString(string_idx);
    if (resolved != nullptr) {  
        return resolved; 
    }
    uint32_t utf16_length;
    //解析这个string_idx，得到一个字符串
    const char* utf8_data = dex_file.StringDataAndUtf16LengthByIdx(string_idx, &utf16_length);
    //将其存储到 class_linker实例中的 intern_table_ 中，返回一个mirror String对象
    mirror::String* string = intern_table_->InternStrong(utf16_length,utf8_data);
    dex_cache->SetResolvedString(string_idx, string);      //存储到dex_cache里
    return string;
}


/*到此，类的Resolve相关函数介绍到此，总而言之，dex文件里使用的type_id、method_id、string_id等都是索引。
这些索引所指向的信息需要分别被找到。而解析的目的就是如下所示。
·根据 type_id 找到它对应的 mirror Class 对象。
·根据 method_id 找到对应的 ArtMethod 对象。
·根据 string_id 找到对应的 mirror String 对象。最后，所有解析出来的信息都将存在该dex文件对应的一个DexCache对象中。
其中，解析type id和method id的时候可能触发目标类的加载和链接过程。这是通过下节将介绍的 FindClass 来完成的。
*/



//8.7.8.1.4 ResolveField 【寻找成员，并保存到 DexCache 对象中】
//最后来看一下ResolveField，它也比较简单。 
//[class_linker.cc->ClassLinker::ResolveField]
ArtField* ClassLinker::ResolveField(const DexFile& dex_file,
                                    uint32_t field_idx,Handle<mirror::DexCache> dex_cache,
                                    Handle<mirror::ClassLoader> class_loader,
                                    bool is_static) {
    //如果已经解析过该成员变量，则返回
    ArtField* resolved = dex_cache->GetResolvedField(field_idx, image_pointer_size_);
    if (resolved != nullptr) { 
        return resolved;
    }
    
    const DexFile::FieldId& field_id = dex_file.GetFieldId(field_idx);
    Thread* const self = Thread::Current();
    StackHandleScope<1> hs(self);
    //先找到该成员变量对应的Class对象
    Handle<mirror::Class> klass(hs.NewHandle(ResolveType(dex_file, field_id.class_idx_, dex_cache, class_loader)));
    .....
    //下面这段代码用于从Class对象ifields_或sfields_中找到对应成员变量的ArtField对象。
    //注意，在搜索过程中，会向上遍历Class派生关系树上的基类。
    if (is_static) {
        resolved = mirror::Class::FindStaticField(self, klass, dex_cache.Get(), field_idx);
    } else {
        resolved = klass->FindInstanceField(dex_cache.Get(), field_idx);
    }
    
    ......
    
    //保存到DexCache resolved_fields_成员变量中
    dex_cache->SetResolvedField(field_idx, resolved, image_pointer_size_);
    return resolved;
}




//【8.7.8.2 FindClass】
//[class_linker.cc->ClassLinker::FindClass]
mirror::Class* ClassLinker::FindClass(Thread* self,
                                      const char* descriptor, 
                                      Handle<mirror::ClassLoader> class_loader) {
    self->AssertNoPendingException();
    //如果字符串只有一个字符，则将搜索基础数据类对应的Class对象
    if (descriptor[1] == '\0') { 
        return FindPrimitiveClass(descriptor[0]); 
    }
    
    //搜索引用类型对应的类对象，首先根据字符串名计算hash值
    const size_t hash = ComputeModifiedUtf8Hash(descriptor);
    
    //从ClassLoader对应的ClassTable中根据hash值搜索目标类
	//【A_7.8.2和87.8.3】
    mirror::Class* klass = LookupClass(self, descriptor, hash, class_loader.Get());
    
    //如果目标类已经存在，则确保它的状态大于或等于kStatusResoved。
    //EnsureResolved并不会调用上文提到的实际加载或链接类的函数，它只是等待其他线程完成这个工作
    if (klass != nullptr) {
        return EnsureResolved(self, descriptor, klass); 
    }
    
    //如果搜索的是数组类，则创建对应的数组类类对象。下文将介绍这个函数
    if (descriptor[0] == '[') {
        return CreateArrayClass(self, descriptor, hash, class_loader);
    } 
    else if (class_loader.Get() == nullptr) {
        /*对bootstrap类而言，它们是由虚拟机加载的，所以没有对应的ClassLoader。
          下面的 FindInClassPath 函数返回的 ClassPathEntry 是类型别名，其定义如下：
                typedef pair<const DexFile*,const DexFile::ClassDef*> ClassPathEntry
                
          FindInClassPath 将从 boot_class_path 里对应的文件中找到目标类所在的Dex文件和对应的ClassDef信息，
          然后调用 DefineClass 来加载目标类  */
        ClassPathEntry pair = FindInClassPath(descriptor, hash, boot_class_path_);
        if (pair.second != nullptr) {      
            //DefineClass 已经介绍过了
            return DefineClass(self, descriptor,hash, ScopedNullHandle<mirror::ClassLoader>(), *pair.first,  *pair.second);
        } 
        ......
    } 
    else {
        ScopedObjectAccessUnchecked soa(self);
        mirror::Class* cp_klass;
        //如果是非bootstrap类，则需要触发 ClassLoader 进行类加载。
        //该函数请读者在学完8.7.9节关于ClassLoader的内容后自行研究
        if (FindClassInPathClassLoader(soa, self, descriptor, hash, class_loader, &cp_klass)) {
            if (cp_klass != nullptr) { 
                return cp_klass;   
            }
        }
        
        ......
        /*如果通过 ClassLoader 加载目标类失败，则下面的代码将转入Java层去执行 ClassLoader 的类加载。
        
        根据代码中的注释所言，类加载失败需要抛出异常，而上面的 FindClassInPathClassLoader 并不会添加异常信息，
          相反，它还会去掉其执行过程中其他函数因处理失败（比如DefineClass）而添加的异常信息。
          所以，接下来的代码将进入Java层去 ClassLoader 对象的 loadClass 函数，虽然目标类最终也会加载失败，
          但相关异常信息就能添加，同时整个调用的堆栈信息也能正确反映出来。
          所以，这种处理方式应是ART虚拟机里为提升运行速度所做的优化处理吧  */
        ......
        
        {
            ......
            result.reset(soa.Env()->CallObjectMethod(class_loader_object.get(),
                                                     WellKnownClasses::java_lang_ClassLoader_loadClass, 
                                                     class_name_object.get()
                                                     ));
        }
        ...... //相关处理，略过。
    }
}



//8.7.8.2.1 FindPrimitiveClass 用于返回代表基础数据类型的mirror Class对象
//[class_linker.cc->ClassLinker::FindPrimitiveClass]
mirror::Class* ClassLinker::FindPrimitiveClass(char type) {
    switch (type) {
        case 'B': 
            //从 class_roots_ 中找到对应的类。7.8.3节曾介绍过它
            return GetClassRoot(kPrimitiveByte);
            
        ......            //其他基础数据类型的处理
        
        case 'Z':
            return GetClassRoot(kPrimitiveBoolean);
        case 'V':
            return GetClassRoot(kPrimitiveVoid);
        default:
            break;
    }
    return nullptr;
}




//马上来看ART虚拟机中如何创建代表基础数据类型的mirror Class对象的。代码如下所示。
//[class_linker.cc->ClassLinker::CreatePrimitiveClass]
mirror::Class* ClassLinker::CreatePrimitiveClass(Thread* self,
                                                 Primitive::Type type) {
    /*创建指定大小的Class对象。对基础数据类型而言，它不包含embedded table，也没有静态成员变量。
    所以，PrimitiveClassSize 内部调用的代码就是 ComputeClassSize(false, 0, 0, 0, 0, 0, 0, pointer_size);    */
    mirror::Class* klass = AllocClass(self, mirror::Class::PrimitiveClassSize(image_pointer_size_));
    //
    return InitializePrimitiveClass(klass, type);
}


//InitializePrimitiveClass 的代码很简单，如下所示。
//[class_linker.cc->ClassLinker::InitializePrimitiveClass]
mirror::Class* ClassLinker::InitializePrimitiveClass(mirror::Class* primitive_class, 
                                                     Primitive::Type type) {
    Thread* self = Thread::Current();
    
    StackHandleScope<1> hs(self);
    Handle<mirror::Class> h_class(hs.NewHandle(primitive_class));
    
    ......
    
    h_class->SetAccessFlags(kAccPublic | kAccFinal | kAccAbstract);
    h_class->SetPrimitiveType(type);
    
    //直接设置状态为 kStatusInitialized
    mirror::Class::SetStatus(h_class, mirror::Class::kStatusInitialized, self);
    const char* descriptor = Primitive::Descriptor(type);
    
    //插入对应的ClassTable中
    mirror::Class* existing = InsertClass(descriptor, h_class.Get(), ComputeModifiedUtf8Hash(descriptor));
    return h_class.Get();
}



//8.7.8.2.2 CreateArrayClass  数组类的创建
//[class_linker.cc->ClassLinker::CreateArrayClass]
mirror::Class* ClassLinker::CreateArrayClass(Thread* self,
                                            const char* descriptor, 
                                            size_t hash,
                                            Handle<mirror::ClassLoader> class_loader) {
    StackHandleScope<2> hs(self);
    //先找component type对应的类。假设descriptor为"[[[Ljava/lang/String;"，
    //则 descriptor + 1将为"[[Ljava/lang/String;"
    MutableHandle<mirror::Class> component_type(hs.NewHandle(FindClass(self, descriptor + 1, class_loader)));
    if (component_type.Get() == nullptr) { ......}
    //不允许创建void[]这样的数组
    if (UNLIKELY(component_type->IsPrimitiveVoid())) { 
        return nullptr; 
    }
    
    //如果传入的class_loader 和 component type 的 class loader 不相同，
    //则需要使用component type的 ClassLoader 加载目标数组类。
    if (class_loader.Get() != component_type->GetClassLoader()) {
        mirror::Class* new_class = LookupClass(self, descriptor, hash, component_type->GetClassLoader());
        //如果找到目标类对象了，则返回
        if (new_class != nullptr) {
            return new_class; 
        }
    }
    
    //【*】如果目标类对象不存在，则需要手动构造一个
    auto new_class = hs.NewHandle<mirror::Class>(nullptr);
    //对于一些常用数组类，系统提前创建好了。比如Class[]、Object[]、String[]、 char[]、int[]、long[]。
    if (UNLIKELY(!init_done_)) {
        if (strcmp(descriptor, "[Ljava/lang/Class;") == 0) {
            new_class.Assign(GetClassRoot(kClassArrayClass));
        } else if (strcmp(descriptor, "[Ljava/lang/Object;") == 0) {
            new_class.Assign(GetClassRoot(kObjectArrayClass));
        } ......
    }
    
    //手动创建。Array::ClassSize 也是调用 ComputeClassSize 函数，
    //调用方法为：ComputeClassSize(true, 11, 0, 0, 0, 0, 0, pointer_size) 即包含IMTable和VTable的内容，
    //但不包括静态成员变量。
    if (new_class.Get() == nullptr) {
        new_class.Assign( AllocClass(self, mirror::Array::ClassSize(image_pointer_size_)) );
        ......
        //设置Class的 component_type_ 成员。只有Class代表数组类时，该成员变量才存在
        new_class->SetComponentType(component_type.Get());
    }
    
    ObjectLock<mirror::Class> lock(self, new_class);
    mirror::Class* java_lang_Object = GetClassRoot(kJavaLangObject);
    //设置Class super_class_ 成员
    new_class->SetSuperClass(java_lang_Object);
    //设置Class vtable_ 成员
    new_class->SetVTable(java_lang_Object->GetVTable());
    //其他设置
    new_class->SetPrimitiveType(Primitive::kPrimNot);
    new_class->SetClassLoader(component_type->GetClassLoader());

    //kClassFlagNoReferenceFields 标志表示目标类不包含引用类型的变量，更多关于类的标志位的知识留待13.8.3节再详细介绍
    if (component_type->IsPrimitive()) {
        new_class->SetClassFlags(mirror::kClassFlagNoReferenceFields);
    } else { //kClassFlagObjectArray表示数组元素的类型为引用类型
        new_class->SetClassFlags(mirror::kClassFlagObjectArray);
    }
    
    //设置状态为 kStatusLoaded
    mirror::Class::SetStatus(new_class, mirror::Class::kStatusLoaded, self);
    {
        ArtMethod* imt[mirror::Class::kImtSize];
        std::fill_n(imt, arraysize(imt),Runtime::Current()->GetImtUnimplementedMethod());
        //填充Class的 embeded_imtable 和 embeded_vtable_信息，同时vtable_设置为空
        new_class->PopulateEmbeddedImtAndVTable(imt, image_pointer_size_);
    }
    
    //类状态更新为 kStatusInitialized
    mirror::Class::SetStatus(..., mirror::Class::kStatusInitialized,...);
    //array_iftable_ 为 ClassLinker的成员变量，类型为IfTable，它包含Cloneable和Seria-lizable两个接口类信息。
    //下面将设置目标数组类的 iftable_ 成员
    {
        mirror::IfTable* array_iftable = array_iftable_.Read();
        new_class->SetIfTable(array_iftable);
    }
    
    //设置类访问标记，数组类时抽象的，final、非接口类型。先获取component type的访问标记
    int access_flags = new_class->GetComponentType()->GetAccessFlags();
    access_flags &= kAccJavaFlagsMask;
    access_flags |= kAccAbstract | kAccFinal;
    access_flags &= ~kAccInterface;
    new_class->SetAccessFlags(access_flags);
    
    //插入component type类的ClassLoader的ClassTable中
    mirror::Class* existing = InsertClass(descriptor, new_class.Get(), hash);
    if (existing == nullptr) {
        jit::Jit::NewTypeLoadedIfUsingJit(new_class.Get());
        return new_class.Get();
    }
    return existing;
}





//最后，我们看一下ClassLinker中 array_iftable_ 的来历。
//这个函数里将先手动构造所需的系统基础类，然后再编译系统基础类对应的dex文件。
//[class_linker.cc->ClassLinker::InitWithoutImage]
bool ClassLinker::InitWithoutImage(vector<unique_ptr<const DexFile>>
                                   boot_class_path,
                                   string* error_msg) {
    ...... //
    auto java_lang_Cloneable = hs.NewHandle(FindSystemClass(self,"Ljava/lang/Cloneable;"));
    auto java_io_Serializable = hs.NewHandle(FindSystemClass(self,"Ljava/io/Serializable;"));
    array_iftable_.Read()->SetInterface(0, java_lang_Cloneable.Get());
    array_iftable_.Read()->SetInterface(1, java_io_Serializable.Get());
    ......
}