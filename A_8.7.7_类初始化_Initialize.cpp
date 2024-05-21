//[class_linker.cc->ClassLinker::InitializeClass]
bool ClassLinker::InitializeClass(Thread* self, 
                                  Handle<mirror::Class> klass,
                                  bool can_init_statics, 
                                  bool can_init_parents) {
                                      
    ...... //多个线程也可能同时触发目标类的初始化工作，如果这个类已经初始化了，则直接返回

    //判断是否能初始化目标类。因为该函数可以在dex2oat编译进程中调用，在编译进程中，某些情况下
    //无须初始化类。这部分内容我们不关注，读者以后碰到相关代码时可回顾此处的处理。
    if (!CanWeInitializeClass(klass.Get(), can_init_statics, can_init_parents)) { 
        return false; 
    }
    
    { //
        .....
        if (!klass->IsVerified()) {      //如果类还没有校验，则校验它
            VerifyClass(self, klass);
            ......
        }
        
        ......
        
        /*下面这个函数将对klass做一些检查，大体功能包括：
          （1）如果klass是接口类，则直接返回，不做任何检查。
          （2）如果klass和它的基类superclass是由两个不同的ClassLoader加载的，则需要对比检
              查klass VTable和superclass VTable中对应项的两个ArtMethod是否有相同的签名
              信息，即两个成员方法的返回值类型、输入参数的个数以及类型是否一致。
          （3）如果klass有Iftable，则还需要检查klass IfTable中所实现的接口类的函数与对应
              接口类里定义的接口函数是否有一样的签名信息。是否开展检查的前提条件也是klass和接
              口类由不同的ClassLoader加载。如果检查失败，则会创建java.lang.LinkageError
              错误信息。  */
        if (!ValidateSuperClassDescriptors(klass)) {.....}
        
        ......
        //设置执行类初始化操作的线程ID以及类状态为 kStatusInitializing
        klass->SetClinitThreadId(self->GetTid());
        mirror::Class::SetStatus(klass, mirror::Class::kStatusInitializing, self);
    }
    
    //根据JLS规范，klass如果是接口类的话，则不需要初始化接口类的基类（其实就是Object）
    if (!klass->IsInterface() && klass->HasSuperClass()) {
        mirror::Class* super_class = klass->GetSuperClass();
        if (!super_class->IsInitialized()) {
            ......
            
            Handle<mirror::Class> handle_scope_super(hs.NewHandle(super_class));
            bool super_initialized = InitializeClass(self, handle_scope_super, can_init_statics, true);
        
            ......      //基类初始化失败的处理
        
        }
    }
    
    //初始化klass所实现的那些接口类
    if (!klass->IsInterface()) {
        size_t num_direct_interfaces = klass->NumDirectInterfaces();
        if (UNLIKELY(num_direct_interfaces > 0)) {
            MutableHandle<mirror::Class> handle_scope_iface(....);
            for (size_t i = 0; i < num_direct_interfaces; i++) {
                //handle_scope_iface代表一个接口类对象
                handle_scope_iface.Assign(mirror::Class::GetDirectInterface(self, klass, i));
                
                //检查接口类对象是否设置了 kAccRecursivelyInitialized 标记位。
                //这个标记位表示这个接口类已初始化过了。该标志位是ART虚拟机内部处理类初始化时的一种优化手段
                if (handle_scope_iface->HasBeenRecursivelyInitialized()) {continue; }
                //初始化接口类，并递归初始化接口类的父接口类
                bool iface_initialized = InitializeDefaultInterfaceRecursive(self, handle_scope_iface,......);
                if (!iface_initialized) {  
                    return false;  
                }
            }
        }
    }
    
    /*到此，klass的父类及接口类都已经初始化了。接下来要初始化klass中的静态成员变量。读者可回
      顾图8-7 class_def结构体，其最后一个成员变量为 static_values_off，它代表该类静态成员
      变量初始值存储的位置。找到这个位置，即可取出对应静态成员变量的初值。 */
    const size_t num_static_fields = klass->NumStaticFields();
    if (num_static_fields > 0) {
        
        //找到 klass 对应的 ClassDef 信息以及对应的DexFile对象
        const DexFile::ClassDef* dex_class_def = klass->GetClassDef();
        const DexFile& dex_file = klass->GetDexFile();
        
        StackHandleScope<3> hs(self);
        Handle<mirror::ClassLoader> class_loader(hs.NewHandle(klass->GetClassLoader()));
        
        //找到对应的DexCache对象
        Handle<mirror::DexCache> dex_cache(hs.NewHandle(klass->GetDexCache()));
        .....
        
        //遍历ClassDef中代表 static_values_off 的区域
        EncodedStaticFieldValueIterator value_it(dex_file, &dex_cache, &class_loader, this, *dex_class_def);
        
        //提取出class_data_item
        const uint8_t* class_data = dex_file.GetClassData(*dex_class_def);
        //遍历class_data_item
        ClassDataItemIterator field_it(dex_file, class_data);
        
        if (value_it.HasNext()) {
            for ( ; value_it.HasNext(); value_it.Next(), field_it.Next()) {
                //找到对应的ArtField成员。下文会介绍 ResolveField 函数
                //【8.7.8】
                ArtField* field = ResolveField(dex_file, field_it.GetMemberIndex(), dex_cache, class_loader, true);
                //设置该ArtField的初值，内部将调用Class的SetFieldXXX相关函数，
                //它会在Class对象中存储对应静态成员变量内容的位置（其值为ArtField的 offset_ ）上设置初值。
                value_it.ReadValueToField<...>(field);
            }
     }
    }
    
    //找到类的"<clinit>"函数，并执行它
    ArtMethod* clinit = klass->FindClassInitializer(image_pointer_size_);
    if (clinit != nullptr) {
        JValue result;
        clinit->Invoke(self, nullptr, 0, &result, "V");
    }
    
    bool success = true;
    {
        if (.....) {
            ......
        }
        else {      //初始化正常
            .....
            //设置类状态为 kStatusInitialized
            mirror::Class::SetStatus(klass, mirror::Class::kStatusInitialized,self);
                //下面这个函数设置klass静态成员方法ArtMethod的 trampoline 入口地址。它和
                //Java方法的执行有关，这部分内容我们留待后文再来介绍。
                FixupStaticTrampolines(klass.Get());
            }
    }
    return success;
}




//ClassLinker中另外一个常用于初始化类的函数为 EnsureInitialized ，它的代码非常简单。
//[class_linker.cc->ClassLinker::EnsureInitialized]
bool ClassLinker::EnsureInitialized(Thread* self, 
                                    Handle<mirror::Class> c,
                                    bool can_init_fields, 
                                    bool can_init_parents) {
    if (c->IsInitialized()) {
        EnsureSkipAccessChecksMethods(c);
        return true;
    }
    const bool success = InitializeClass(self, c, can_init_fields, can_init_parents);
    .....
    return success;
}