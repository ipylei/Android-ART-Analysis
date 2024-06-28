//接着来看Attach的代码，如下所示。
//[thread.cc->Thread::Attach]
Thread * Thread::Attach(const char * thread_name, bool as_daemon, jobject thread_group, bool create_peer) {
    Runtime * runtime = Runtime::Current();
    Thread * self; {
        MutexLock mu(nullptr,  * Locks::runtime_shutdown_lock_);
        if (runtime->IsShuttingDownLocked()) {
            ....
        } else {
            Runtime::Current()->StartThreadBirth();
            // ①关键函数之一构造函数
            self = new Thread(as_daemon);

            //②关键函数之二
            bool init_success = self->Init(runtime->GetThreadList(), runtime->GetJavaVM());
            Runtime::Current()->EndThreadBirth();

            ...
        }
    }

    //③关键函数之三
    self->InitStringEntryPoints();

    self->SetState(kNative);
    .....
    return self;
}


//【*】关键函数之一构造函数
//[thread.cc->Thread：Thread]
Thread::Thread(bool daemon): tls32_(daemon), wait_monitor_(nullptr), interrupted_(false) {
    //创建Mutex和ConditionVariable对象
    wait_mutex_ = new Mutex("a thread wait mutex");
    wait_cond_ = new ConditionVariable(...,  * wait_mutex_);

    /*tlsPtr_ 是 Thread 类内部定义的一个结构体，其数据类型名为 struct tls_ptr_sized_values。
    该结构体非常核心，涉及调用、堆栈等一些关键信息均存储在此结构体中。
    从tlsPtr_中的tls（Thread Local Storage的简写）一词可以看出，
    该结构体的对象实例是和具体某个线程所关联的。
    下面的代码全部是初始化tlsPtr_里的一些成员变量。
    这些成员变量的作用将在使用它们的时候再做介绍。*/
    tlsPtr_.instrumentation_stack = new std::deque < instrumentation::InstrumentationStackFrame > ;
    tlsPtr_.name = new std::string(kThreadNameDuringStartup);

    ......

    //注意，下面这行fill代码将留待第13章介绍RosAlloc的时候再解释
    std::fill(tlsPtr_.rosalloc_runs, tlsPtr_.rosalloc_runs + kNumRosAllocThreadLocalSizeBracketsInThread, gc::allocator::RosAlloc::GetDedicatedFullRun());
	
    for (uint32_t i = 0; i < kMaxCheckpoints; ++i) {
        tlsPtr_.checkpoint_functions[i] = nullptr;
    }
    ......
}

//如上述代码的注释可知，tlsPtr_是Thread类中非常关键的结构体，下面是它的几个主要成员变量的说明。
//[thread.h->Thread::tls_ptr_sized_values]
struct PACKED(sizeof(void * ))tls_ptr_sized_values {
    .....//本章先介绍其中一部分成员变量

    /*描述本线程所对应的栈的安全高地址。
    随着函数的调用，线程栈的栈顶（Top）会逐渐向下拓展，
    一旦栈顶地址低于此变量，将触发上文提到的Implicit Stackoverflow check。
    注意，stack_end一词很有歧义，它其实并不是用来描述栈底地址的。
    下文将详细介绍ART虚拟机中线程栈的设置情况。*/
    uint8_t * stack_end;
    
    //【10.2.4】
    ManagedStack managed_stack;

    JNIEnvExt * jni_env; //和本线程关联的Jni环境对象
    Thread * self; //指向包含本对象的Thread对象

    //下面两个成员变量也和线程栈有关。详情见下文对线程栈的介绍
    uint8_t * stack_begin;
    size_t stack_size;

    .....

    /*注意下面两个关键成员变量：
    （1）jni_entrypoints：结构体，和JNI调用有关。
            里边只有一个函数指针成员变量，名为pDlsymLookup。
            当JNI函数未注册时，这个成员变量将被调用【以】找到目标JNI函数    【参考 11.2】
			
    （2）quick_entrypoints：结构体，其成员变量全是个函数指针类型，
            其定义可参考quick_entrypoints_list.h。它包含了一些由ART虚拟机提供的某些功能，
            而我们编译得到的机器码可能会用到它们。
            生成机器码时，我们需要生成对应的调用指令以跳转到这些函数
    */
    JniEntryPoints jni_entrypoints;
    QuickEntryPoints quick_entrypoints;

    /*ART虚拟机支持解释方式执行dex字节码。这部分功能从ART源码结构的角度来看，
    它们被封装在一个名为mterp的模块中（相关代码位于runtime/interpreter/mterp中）。
    mterp是modular interpreter的缩写，它在不同CPU平台上，利用对应的汇编指令来编写dex字节码的处理。
    使用汇编来编写代码可大幅提升执行速度。
    在dalvik虚拟机时代，mterp这个模块也有，但除了几个主流CPU平台上了有汇编实现之外，
    还存在一个用C++实现的代码，其代码很有参考价值。而在ART虚拟机的mterp模块里，
    C++实现的代码被去掉了，只留下不同CPU平台上的汇编实现。
    下面这三个变量【指向】汇编代码中interpreter处理的入口地址，其含义是：

    mterp_current_ibase：当前正在使用的interpreter处理的入口地址
    mterp_default_ibase：默认的interpreter处理入口地址
    mterp_alt_ibase：可追踪执行情况的interpreter地址。
    代码中根据情况会将mterp_current_ibase指向其他两个变量。
    比如，如果设置了跟踪每条指令解释执行的情况，则mterp_current_ibase指向mterp_alt_ibase，否则使用mterp_default_ibase */
    void * mterp_current_ibase;
    void * mterp_default_ibase;
    void * mterp_alt_ibase;
	
    .......
	
}
tlsPtr_;


//【*】关键函数之二
//[thread.cc->Thread::Init]
bool Thread::Init(ThreadList * thread_list, JavaVMExt * java_vm, JNIEnvExt * jni_env_ext) {
    tlsPtr_.pthread_self = pthread_self();
    SetUpAlternateSignalStack(); //此函数在Android平台上不做任何操作
    //设置线程栈，详情见下文分析
    if (!InitStackHwm()) {
        return false;
    }
    
    //初始化CPU，详情见下文分析
    InitCpu();
    
    //设置tlsPtr_里的 jni_entrypoints 和 quick_entrypoints 这两个成员变量
    InitTlsEntryPoints();
    
    RemoveSuspendTrigger();
    InitCardTable();
    InitTid();
    
    //将初始化解释执行模块的入口地址
    //主要功能就是设置Thread对象里tlsPtr_结构体中的mterp_default_ibase、mterp_alt_ibase和mterp_current_ibase三个成员
    interpreter::InitInterpreterTls(this);  

    //下面这段代码将当前这个Thread对象设置到本线程的本地存储空间中去
     # ifdef __ANDROID__
    //__get_tls是bionc里定义的一个函数，属于Android平台特有的，其目的和非Android平台
    //所使用的pthread_setspecific一样
    __get_tls()[TLS_SLOT_ART_THREAD_SELF] = this;
     # else
        CHECK_PTHREAD_CALL(pthread_setspecific, (Thread::pthread_key_self_, this), "attach self");
     # endif
     # endif
    
    tls32_.thin_lock_thread_id = thread_list->AllocThreadId(this);
    
    //每一个线程将关联一个JNIEnvExt对象，它存储在 tlsPtr_jni_env 变量中。
    //对主线程而言，JNIEnvExt对象由Runtime创建并传给代表主线程的Thread对象，也就是此处分析的Thread对象
    if (jni_env_ext != nullptr) {
        tlsPtr_.jni_env = jni_env_ext;
    } else { 
        //如果外界不传入JNIEnvExt对象，则自己创建一个
        tlsPtr_.jni_env = JNIEnvExt::Create(this, java_vm);
        ......
    }
    
    //Runtime对象中有一个thread_list_成员变量，其类型是ThreadList，用于存储虚拟机所创建的Thread对象。
    thread_list->Register(this);
    return true;
}


//7.5.2.2.1　InitStackHwm
//[pthread_create.cpp->pthread_create]
int pthread_create(pthread_t* thread_out, pthread_attr_t const* attr, void* (*start_routine)(void*), void* arg) {
    ErrnoRestorer errno_restorer;
    ....
    void* child_stack = NULL;//child_stack的值在下面这个函数中被设置
    
    int result = __allocate_thread(&thread_attr, &thread, &child_stack);
    
    ....
    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | ....
    ....
    
    /*线程的创建是通过clone系统调用来实现的，其中child_stack是这个线程的栈底。
    因为Linux系统上线程栈由高地址往低地址拓展。
    所以此处的child_stack是某块内存的高地址，也就是它指向线程栈的栈底
    （栈底是不动的，出栈和入栈是在栈顶）。
    另外，请读者特别注意，分配内存的时候，
    比如malloc或mmap等函数调用返回的是却是某块内存的低地址。
    下面的clone调用中使用的child_stack来自__allocate_thread函数。*/
    int rc = clone(__pthread_start, child_stack, flags, thread, &(thread->tid), tls, &(thread->tid));
    .....
    return 0;
}






//【*】关键函数之三
//7.5.2.3　InitStringEntryPoints函数介绍
//接着来看Thread Attach中的第三个关键函数InitStringEntryPoints。
//通过函数名我们可判断它也是初始化某些EntryPoints，而且可能是和字符串相关某些函数，来看代码。
//[thread.cc->Thread::InitStringEntryPoints]
void Thread::InitStringEntryPoints() {
    ScopedObjectAccess soa(this);
    //设置tlsPtr_对象里的quick_entrypoints中的某些成员。不过设置的方法却和前面直接指定目标函数名完全不一样
    QuickEntryPoints* qpoints = &tlsPtr_.quick_entrypoints;
    
    /*来看 pNewEmptyString 的赋值，它的值来自：
      （1）WellKnownClasses::java_lang_StringFactory_newEmptyString，这是一个静态变量，
                类型为jmethodID（熟悉JNI的读者可能还记得，
                    在JNI层中，jmethodID用于表示一个Java方法，与之类似的是jfieldID，表示一个Java类中的成员）。
                 jmethodID的真实数据类型并无标准规定，不同虚拟机实现会使用不同的数据类型。
                待会我们会看到ART虚拟机里的jmethodID到底是什么。
                此处的jmethodID所表示的Java方法是java.lang.StringFactor.newEmptyString函数
      （2）调用 ScopedObjectAccess 的 DecodeMethod 函数，输入参数为jmethodID。
                ScopedObjectAccess 是一个辅助类，ART中还有其他与之相关的辅助类。
                笔者不拟介绍其细节。后文碰到使用它的地方，我们再做相关介绍。
      （3）然后通过reinterpret_case将 DecodeMethod 的返回值进行类型转换。
    */
    qpoints->pNewEmptyString = reinterpret_cast<void(*)()>(
            soa.DecodeMethod(WellKnownClasses::java_lang_StringFactory_newEmptyString));
    qpoints->pNewStringFromBytes_B = reinterpret_cast<void(*)()>(
        soa.DecodeMethod(WellKnownClasses::java_lang_StringFactory_newStringFromBytes_B));
    .......
}

//先来看ScopedObjectAccess的DecodeMethod函数，其代码如下。
//[scoped_thread_state_change.h->ScopedObjectAccessAlreadyRunnable::DecodeMethod]
//声明在class内部，即是一个inline函数
//详情参考8.2.2
ArtMethod* DecodeMethod(jmethodID mid) const {
    Locks::mutator_lock_->AssertSharedHeld(Self());
    DCHECK(IsRunnable());
    //数据类型转换，将输入的jmethodID转换成ArtMethod*。也就是说，一个jmethodID
    //实际上指向的是一个ArtMethod对象。上文介绍FaultManager的时候，我们曾经说过
    //ArtMethod是一个Java函数经过编译后得到的generated code在代码中的表示。
    return reinterpret_cast<ArtMethod*>(mid);
}