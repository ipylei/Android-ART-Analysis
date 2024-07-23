/*【10.1.2】 Runtime ArtMethod对象
如上文所述，一个ArtMethod对象代表源码中的一个Java方法。而实际上ART虚拟机也可以创建一个和源码里Java方法无关的ArtMethod对象。
这类ArtMethod对象被称为Runtime ArtMethod（代码中称之为Runtime Method）。Runtime Method的创建方法如下
*/

//[class_linker.cc->ClassLinker::CreateRuntimeMethod]
ArtMethod* ClassLinker::CreateRuntimeMethod(LinearAlloc* linear_alloc) {
    const size_t method_alignment = ArtMethod::Alignment(image_pointer_size_);
    const size_t method_size = ArtMethod::Size(image_pointer_size_);
    
    //从 linear_alloc 中分配一块内存以构造一个 ArtMethod 数组，数组个数为1。
    LengthPrefixedArray<ArtMethod>* method_array = AllocArtMethodArray(Thread::Current(), linear_alloc,  1);
    ArtMethod* method = &method_array->At(0, method_size, method_alignment);
    
    /*由于Runtime Method和源码无关，所以其 dex_method_index_ 成员变量（表示某个方法在dex文件method_ids数组中的索引）
    取值为 kDexNoIndex（0xFFFFFFFF）。
    */
    method->SetDexMethodIndex(DexFile::kDexNoIndex);
    return method;
}

/*
由上述代码可知，如果一个ArtMethod为Runtime Method的方法，那么它的 dex_method_index_ 取值为 kDexNoIndex。
那么，Runtime Method有什么用呢？
【先来了解ART虚拟机所定义的那六个 Runtime ArtMethod 对象】。
*/
//[runtime.h]
class Runtime{
    ......
    public:
        /*下面是 CalleeSaveType 枚举变量定义，它用于描述函数调用时，被调用函数需要保存哪些信息到栈上。
        其具体情况我们下文再介绍。
        */
        enum CalleeSaveType {
            kSaveAll,             //0
            kRefsOnly,            //1
            kRefsAndArgs,         //2
            kLastCalleeSaveType   //3
        };
        
    private:
        /* callee_save_methods_ 是一个数组，包含三个元素，分别对应三种 CalleeSaveType 类型。注意，
           该数组元素类型为uint64_t，但实际上它是一个Runtime ArtMethod对象的地址。此处为了兼容
           32位和64位平台，所以使用了64位长的类型。
           
           这三个Runtime Method主要用于跳转代码，为目标函数的参数做准备。
        */
        //kSaveAll、kRefsOnly、kRefsAndArgs
        uint64_t callee_save_methods_[kLastCalleeSaveType];
        
        ......
        
        /*下面是另外三个Runtime Method对象和它们的作用介绍：
            (*)resolution_method_：类似于类的解析，该函数用于解析被调用的函数到底是谁。
            
            (*)imt_conflict_method_：参考8.7.4.1节及图8-11的内容可知，
                接口方法对应的ArtMethod对象将保存在一个默认长度为kImtSize（默认值为64）的IMTable中，
                其存储的索引位由ArtMethod dex_method_index_ 对kImtSize取模而得到。
                
                由于两个不同的接口方法很可能计算得到相同的索引位，对于这种有冲突的情况，
                ART虚拟机将把这个 imt_conflict_method_ 对象放在有冲突的索引位上。
                
            (*)imt_unimplemented_method_：用于处理一个未解析的接口方法。  
        */
        ArtMethod* resolution_method_;
        ArtMethod* imt_conflict_method_;
        ArtMethod* imt_unimplemented_method_;
}


//【*1】我们先来了解上述代码中最后所列的三个Runtime Method对象。它们的设置都在 ClassLinker 的 InitWithoutImage 函数中，代码如下所示。
//[class_linker.cc->ClassLinker::InitWithoutImage]
bool ClassLinker::InitWithoutImage(std::vector<std::unique_ptr<const DexFile>> boot_class_path,
                                   std::string* error_msg) {
    ......
    //创建对应的Runtime Method对象，并赋值给runtime对应的成员变量。注意，
    //在下面的代码中，后两个函数调用的参数都是 CreateImtConflictMethod 函数的返回值。
    runtime->SetResolutionMethod(runtime->CreateResolutionMethod());
    runtime->SetImtConflictMethod(runtime->CreateImtConflictMethod(linear_alloc));
    runtime->SetImtUnimplementedMethod(runtime->CreateImtConflictMethod(linear_alloc));
    ......
}
/* 【总结】
注意　InitWithoutImage 由dex2oat在生成boot镜像时使用。
这几个Runtime Method对象的内容和地址将写入boot.art文件中。
待到zygote进程启动完整虚拟机时，它们又会被读取并设置到Runtime对象中。
从全流程来看，最初的设置就是在 InitWithoutImage 中完成的。
*/


//【10.1.2.1】　CreateResolutionMethod
//resolution_method_ 由 Runtime CreateResolutionMethod 函数创建，代码如下所示。
//[runtime.cc->Runtime::CreateResolutionMethod]
ArtMethod* Runtime::CreateResolutionMethod() {
    auto* method = GetClassLinker()->CreateRuntimeMethod(GetLinearAlloc());
    
    //If分支用于dex2oat的情况。
    if (IsAotCompiler()) {
        ......
    }
    else {
        // resolution_method_ 的机器码入口地址为 art_quick_resolution_trampoline，它对应的
        //跳转目标为 artQuickResolutionTrampoline 函数。
        method->SetEntryPointFromQuickCompiledCode(GetQuickResolutionStub());
    }
    return method;
}


//【10.1.2.2】　ImtConflictMethod 和 ImtUnimplementedMethod
//接着来看 imt_conflict_method_ 和 imt_unimplemented_method_ ，它们都由Runtime CreateImtConflictMethod 来设置。代码如下所示。
//[runtime.cc->Runtime::CreateImtConflictMethod]
ArtMethod* Runtime::CreateImtConflictMethod(LinearAlloc* linear_alloc) {
    ClassLinker* const class_linker = GetClassLinker();
    //先创建一个Runtime Method对象
    ArtMethod* method = class_linker->CreateRuntimeMethod(linear_alloc);
    
    const size_t pointer_size = GetInstructionSetPointerSize(instruction_set_);
    if (IsAotCompiler()) {
        ......
    }
    else {
        /*设置机器码入口为跳转代码 art_quick_imt_conflict_trampoline，这个跳转代码的目标需联合 ImtConflictTable 来确认。  */
        method->SetEntryPointFromQuickCompiledCode(GetQuickImtConflictStub());
    }
    //创建一个 ImtConflictTable 对象，并将这个对象的地址，赋值给 method 的jni机器码 入口
    method->SetImtConflictTable(class_linker->CreateImtConflictTable(0u, linear_alloc), pointer_size);
    return method;
}
/*【总结】
也就是说，对 imt_conflict_method_ 和 imt_unimplemented_method_这两个Runtime Method对象而言：
    ·它们的【机器码】入口地址都是 art_quick_imt_conflict_trampoline。
    ·它们的【jni机器码】入口地址实际上是一个 ImtConflictTable 对象。
*/




//【10.1.2.3】　CalleeSavedMethod
//【*2】接下来了解Runtime类中另外三个Runtime Method，它们和 CalleeSaveType 有关。创建它们的代码如下。
//[runtime.cc->Runtime::CreateCalleeSaveMethod]
ArtMethod* Runtime::CreateCalleeSaveMethod() {
    auto* method = GetClassLinker()->CreateRuntimeMethod(GetLinearAlloc());
    
    size_t pointer_size = GetInstructionSetPointerSize(instruction_set_);
    method->SetEntryPointFromQuickCompiledCodePtrSize(nullptr, pointer_size);
    return method;
}
/*【总结】
CalleeSaveType相关的三个Runtime Method对象的【机器码】入口地址为nullptr，说明这三个对象并不关联任何跳转代码或跳转目标。
那它们的作用是什么呢？原来，这三个Runtime Method对象主要用于其他跳转代码针对不同情况的参数传递、栈回溯等工作。
这部分内容将在10.1.3节中会做详细介绍。
*/



//蹦床
//【10.1.2.4】　Trampoline code汇总
/*
ART虚拟机中有很多Trampoline code，主要有两大类。
    ·一类是针对jni方法的Trampoline code，它们封装在 JniEntryPoints 结构体中，只包含一个 pDlsymLookup 函数指针。
    ·一类是针对非jni方法的Trampoline code，它们封装在 QuickEntryPoints 结构体中，一共包含132个函数指针。
*/


//【10.1.2.4.1】　Thread类里的相关成员变量
//以上两组Trampoline code（代码中对应为函数指针）包含在Thread类的tlsPtr_相关的成员变量中，代码如下所示。
//[thread.h->Thread]
class Thread{
    ......
    struct PACKED(sizeof(void*)) tls_ptr_sized_values {
        ......
        JniEntryPoints jni_entrypoints;
        QuickEntryPoints quick_entrypoints;
        ......
    } tlsPtr_;
}
/*
这两组Trampoline code都由汇编代码实现，对x86平台而言：
    ·JniEntryPoints对应的Trampoline code在 jni_entrypoints_x86.S 里实现。
    ·QuickEntryPoints对应的Trampoline code在 quick_entrypoints_x86.S 里实现。
所有Trampoline code都是一段汇编代码编写的函数，这段汇编代码函数内部一般会跳转到一个由更高级的编程语言（C++）实现的函数。
*/



//【10.1.2.4.2】　oat文件里的Trampoline code
/*
由9.6.1节介绍的InitOatCode函数可知，boot.oat中Trampoline code区域所包含的跳转代码并不是
上面提到的包含在 JniEntryPoints 和 QuickEntryPoints 结构体里对应的成员变量，
而是一段跳转到 JniEntryPoints 和 QuickEntryPoints 对应成员变量的跳转代码。下面是这五个变量的总结。


·jni_dlsym_lookup_offset_：包含一段跳转到 JniEntryPoints pDlsymLookup 函数指针所指向的函数。而这个pDlsymLookup函数指针正好指向art_jni_dlsym_lookup_stub跳转代码。
·quick_generic_jni_trampoline_offset_：包含一段跳转到  QuickEntryPoints pQuickGenericJniTrampoline 函数指针所指向的函数，也就是 art_quick_generic_jni_trampoline 跳转代码。
·quick_imt_conflict_trampoline_offset_：包含一段跳转到 QuickEntryPoints pQuickImtConflictTrampoline 函数指针所指向的函数，也就是 art_quick_imt_conflict_trampoline 跳转代码。
·quick_resolution_trampoline_offset_：包含一段跳转到   QuickEntryPoints pQuickResolutionTrampoline 函数指针所指向的函数，也就是 art_quick_resolution_trampoline 跳转代码。
·quick_to_interpreter_bridge_offset_：包含一段跳转到   QuickEntryPoints pQuickToInterpreterBridge 函数指针所指向的函数，也就是 art_quick_to_interpreter_bridge 跳转代码。

为了区分两种不同的跳转代码，笔者称：
 ·JniEntryPoints 和 QuickEntryPoints 成员变量所指向的跳转代码为直接跳转代码。
 ·而oat文件里的Trampoline code为间接跳转代码。间接跳转代码的目标是跳转到直接跳转代码。
那么，oat文件里的间接跳转代码在哪里被使用呢？来看下文。
*/


//【10.1.2.4.3】　ClassLinker类里的相关成员变量
/*
ClassLinker中也有四个和trampoline cod有关的成员变量。它们分别是
    quick_resolution_trampoline_
    quick_imt_conflict_trampoline_
    quick_generic_jni_trampoline_
    quick_to_interpreter_bridge_trampoline_
这四个变量指向间接跳转代码。下文将详细介绍它们
*/
//ClassLinker中这四个成员变量来自oat文件头结构，相关代码如下所示。
//[class_linker.cc->ClassLinker::InitFromBootImage]
bool ClassLinker::InitFromBootImage(std::string* error_msg) {
    ......
    //Oat文件头结构
    const OatHeader& default_oat_header = oat_files[0]->GetOatHeader();
    const char* image_file_location = oat_files[0]->GetOatHeader().GetStoreValueByKey(OatHeader::kImageLocationKey);
    quick_resolution_trampoline_ =   default_oat_header.GetQuickResolutionTrampoline();
    quick_imt_conflict_trampoline_ = default_oat_header.GetQuickImtConflictTrampoline();
    quick_generic_jni_trampoline_ =  default_oat_header.GetQuickGenericJniTrampoline();
    quick_to_interpreter_bridge_trampoline_ = default_oat_header.GetQuickToInterpreterBridge();
    ......
}