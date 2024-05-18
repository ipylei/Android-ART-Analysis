//[java_vm_ext.cc->JavaVMExt]
JavaVMExt::JavaVMExt(Runtime* runtime, const RuntimeArgumentMap& runtime_options)
        : 
        runtime_(runtime),
        check_jni_abort_hook_(nullptr),
        
        ......
        
        libraries_(new Libraries),
        unchecked_functions_(&gJniInvokeInterface),
        
        ...... {
            
        /*functions是JavaVMExt基类JavaVM第一个成员变量，类型为JNIInvokeInterface*，
        而unchecked_functions_是JavaVMExt的成员变量，数据类型也是JNIInvokeInterface*。
        
        二者的作用略有区别。
         （1）如果不启用jni检查的话，他们指向同一个JNIInvokeInterface对象。
         （2）如果启用jni检查的话，这两个成员变量将指向不同的JNIInvokeInterface对象。
         其中，unchecked_functions_代表无需jni检查的对象，
         而functions_代表需要jni检查的对象。
         
        当functions_做完jni检查完后，它会调用unchecked_functions_对应的函数。
         不过，此时这两个成员变量初始都会指向一个全局的JNIInovkeInterface对象，
         即上面初始化列表中的gJniInvokeInterface  */
        functions = unchecked_functions_;
    
        //判断是否启用checkJni功能
        SetCheckJniEnabled(runtime_options.Exists(RuntimeArgumentMap::CheckJni));
}



//gJniInvokeInterface 代表无需jni检查的JNIInovkeInterface对象，其成员变量的取值情况如下面的代码所示。
//[java_vm_ext.cc->gJniInvokeInterface]
const JNIInvokeInterface gJniInvokeInterface = {
    nullptr,  nullptr, nullptr,
    //下面这些函数是JIT类的静态成员函数，所以前面有"JII::"修饰
    JII::DestroyJavaVM,
    JII::AttachCurrentThread,
    JII::DetachCurrentThread,
    JII::GetEnv,
    JII::AttachCurrentThreadAsDaemon
};

//如果JavaVMExt构造函数中在最后调用的SetCheckJniEnabled里启用checkJni的话，会是什么情况呢？来看代码。
//[java_vm_ext.cc->JavaVMExt::SetCheckJniEnabled]
bool JavaVMExt::SetCheckJniEnabled(bool enabled) {
    bool old_check_jni = check_jni_;
    check_jni_ = enabled;
    
    //如果启用了checkJni，则functions将被设置为 gCheckInvokeInterface
    functions = enabled ? GetCheckJniInvokeInterface() : unchecked_functions_;
    MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
    
    //设置runtime中所有线程，启动或关闭checkJni的功能
    runtime_->GetThreadList()->ForEach(ThreadEnableCheckJni, &check_jni_);
    
    return old_check_jni;
}

//GetCheckJniInvokeInterface 函数返回的是 gCheckInvokeInterface 对象。
//[java_vm_ext.cc->gCheckInvokeInterface]
const JNIInvokeInterface gCheckInvokeInterface = {
    nullptr, nullptr,nullptr,
    CheckJII::DestroyJavaVM, 
    CheckJII::AttachCurrentThread,
    CheckJII::DetachCurrentThread, 
    CheckJII::GetEnv,
    CheckJII::AttachCurrentThreadAsDaemon
};


//我们简单看其中的 CheckJII::AttachCurrentThread 函数，代码非常简单。
//[check_jni.cc->AttachCurrentThread]
static jint AttachCurrentThread(JavaVM* vm, JNIEnv** p_env, void* thr_args) {
        ScopedCheck sc(kFlag_Invocation, __FUNCTION__);
        JniValueType args[3] = {{.v = vm}, {.p = p_env}, {.p = thr_args}};
        
        //jni检查的一项，本章不介绍
        sc.CheckNonHeap(reinterpret_cast<JavaVMExt*>(vm), true, "vpp", args);
        JniValueType result;
        
        //BaseVm将获取JavaVMExt的unchecked_functions对象，然后调用的它的AttachCurrentThread函数
        result.i = BaseVm(vm)->AttachCurrentThread(vm, p_env, thr_args);
        
        //对调用结果进行检查
        sc.CheckNonHeap(reinterpret_cast<JavaVMExt*>(vm), false, "i", &result);
        return result.i;
}