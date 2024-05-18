//[runtime.cc->Runtime::Start]
bool Runtime::Start() {
    Thread* self = Thread::Current();
    self->TransitionFromRunnableToSuspended(kNative);
    
    //刚进入这个函数就设置started_成员变量为true，表示虚拟机已经启动了
    started_ = true;
    
    /*对笔者所搭建的测试环境而言，下面两个函数的返回值都为true：
        UseJitCompilation：是否启用JIT编译
        GetSaveProfilingInfo：是否保存profiling信息。  */
    if (jit_options_->UseJitCompilation() || jit_options_->GetSaveProfilingInfo()) {
        std::string error_msg;
        if (!IsZygote()) { ......}
        else if (jit_options_->UseJitCompilation()) {
            // ①加载JIT编译模块所对应的so库 
            //【章节8.3】
            if (!jit::Jit::LoadCompilerLibrary(&error_msg)) {......}
        }
    }
    
    ......
    
    {
        ScopedTrace trace2("InitNativeMethods");
        // ②初始化JNI层相关内容 
        //【章节8.4】
        InitNativeMethods();
    }
    
    //③完成和Thread类初始化相关的工作  
    //【章节8.5】
    InitThreadGroups(self);
    Thread::FinishStartup();
    
    // ④创建系统类加载器（system class loader），返回值存储到成员变量system_class_loader_里
    //【章节8.6】
    system_class_loader_ = CreateSystemClassLoader(this);

    if (is_zygote_) {
        // 下面这个函数用于设置zygote进程和存储位置相关的内容，和虚拟机关系不大。
        if (!InitZygote()) {  return false;   }
    } else {
		......
    }
    
    // 启动虚拟机里的daemon线程，本章将它与第③个关键点一起介绍 
    //【章节8.5】
    StartDaemonThreads();
    
    ......
    
    finished_starting_ = true;
    
    ......
    
    return true;
}