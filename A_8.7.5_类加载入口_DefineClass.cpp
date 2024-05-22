//从dex文件中加载某个类
//[class_linker.cc->ClassLinker::DefineClass]
mirror::Class* ClassLinker::DefineClass(Thread* self,
                                        const char* descriptor,
                                        size_t hash,
                                        Handle<mirror::ClassLoader> class_loader,
                                        const DexFile& dex_file, 
                                        const DexFile::ClassDef& dex_class_def) {
                
    /*注意这个函数的参数和返回值，其中，输入参数：
    
    descriptor：目标类的字符串描述。这里请读者注意，在JLS规范中，类名描述规则和我们日常
    编码时所看到的诸如"java.lang.Class"这样的类名一样。而在JVM规范中，则使用诸如"Ljava/lang/Class;"这样的类名描述。
    从JLS类名到JVM类名的转换可由runtime/utils.cc的 DotToDescriptor(const char* class_name)函数来完成。
    ART虚拟机内部使用JVM类名居多。
    dex_file：该类所在的dex文件对象。
    dex_class_def：目标类在dex文件中对应的ClassDef信息。
    该函数的输出参数为代表目标类的Class对象    */
    
    StackHandleScope<3> hs(self);
    auto klass = hs.NewHandle<mirror::Class>(nullptr);
        
    /*如果是下面这些基础类，则直接从 class_roots_ 中获取对应的类信息。注意，这种情况只在初始化未
      完成阶段存在（init_done_为false）。读者可回顾7.8.2节对 class_roots_ 变量的介绍。  */
    if (UNLIKELY(!init_done_)) {
        if (strcmp(descriptor, "Ljava/lang/Object;") == 0) {
            klass.Assign(GetClassRoot(kJavaLangObject));
        } else if (strcmp(descriptor, "Ljava/lang/Class;") == 0) {
            klass.Assign(GetClassRoot(kJavaLangClass));
        } ......
    }
    
    //分配一个class对象，注意 SizeOfClassWithoutEmbeddedTables 函数返回所需内存大小。现在
    //所分配的这个Class对象还不包含IMTable、VTable。
    if (klass.Get() == nullptr) {
        klass.Assign(AllocClass(self, SizeOfClassWithoutEmbeddedTables(dex_file, dex_class_def)));
    }
    
    ......
    
    //注册DexFile对象
    mirror::DexCache* dex_cache = RegisterDexFile(dex_file, class_loader.Get());
    ......
    klass->SetDexCache(dex_cache);
    
    //调用SetupClass
	//【8.7.2】
    SetupClass(dex_file, dex_class_def, klass, class_loader.Get());
    
    ......

    ObjectLock<mirror::Class> lock(self, klass);
    klass->SetClinitThreadId(self->GetTid());
    
    //插入ClassLoader对应的ClassTable中。注意，不同的线程可以同时调用DefineClass来加载同一个类。这种线程同步直接的关系要处理好。
    mirror::Class* existing = InsertClass(descriptor, klass.Get(), hash);
    if (existing != nullptr) {
        //existing不为空，则表示有别的线程已经在加载目标类了，下面的 EnsureResolved
        //函数将进入等待状态，直到该目标类状态变为超过 kStatusResolved 或 出错。
        return EnsureResolved(self, descriptor, existing);
    }
    
    //没有其他线程在处理目标类，接下来将由本线程处理。上文已经介绍过下面这些重要函数了
	//【8.7.3 类加载_相关函数1】
    LoadClass(self, dex_file, dex_class_def, klass);
    
    ......
    
	//【8.7.3 类加载_相关函数2】
    if (!LoadSuperAndInterfaces(klass, dex_file)) { return nullptr; }
    auto interfaces = hs.NewHandle<mirror::ObjectArray<mirror::Class>>(nullptr);
    MutableHandle<mirror::Class> h_new_class = hs.NewHandle<mirror::Class>(nullptr);
    
	//【8.7.4 链接类 LinkClass】
    if (!LinkClass(self, descriptor, klass, interfaces, &h_new_class)) {...}
    
    //LinkClass成功后，返回的 h_new_class 的状态为kStatusResolved。
    Dbg::PostClassPrepare(h_new_class.Get());
    jit::Jit::NewTypeLoadedIfUsingJit(h_new_class.Get());
    return h_new_class.Get();
}