/*
·dex2oat编译这个Java native方法后将会生成一段机器码。ArtMethod对象的机器码入口地址会指向这段生成的机器码。
    【这段机器码本身会跳转到这个ArtMethod对象的JNI机器码入口地址。】
    如果这个JNI方法没有注册过（即这个native方法还未和Native层对应的函数相关联），这个JNI机器码入口地址是 art_jni_dlsym_lookup_stub。
    否则，JNI机器码入口地址指向Native层对应的函数。

·如果dex2oat没有编译过这个Java native方法，则ArtMethod对象的机器码入口地址为跳转代码 art_quick_generic_jni_trampoline 【10.1.1 LinkCode】。
    同样，如果这个JNI方法没有注册过，则JNI机器码入口地址为跳转代码 art_jni_dlsym_lookup_stub。
    否则，JNI机器码入口地址指向Native层对应的函数。
*/



//【11.2.1】　art_jni_dlsym_lookup_stub
//由上文可知，如果JNI方法没有注册，则需要先关联JNI方法和它的目标Native函数。该工作由 art_jni_dlsym_lookup_stub 来实现。
//[jni_entrypoints_x86.S->art_jni_dlsym_lookup_stub]
DEFINE_FUNCTION art_jni_dlsym_lookup_stub
    subl LITERAL(8), %esp
    pushl %fs:THREAD_SELF_OFFSET
    //调用 artFindNativeMethod，如果找到对应的Native函数，则返回该函数对应的函数指针
    call SYMBOL(artFindNativeMethod)     // (Thread*)
    addl LITERAL(12), %esp
    testl %eax, %eax                     //判断返回值是否为空，如果不为空，则表示找到了 Native 对应的函数
    jz .Lno_native_code_found
    jmp *%eax                            //【* 重点】以jmp的方式跳转到【Native】对应的函数
.Lno_native_code_found:                  //没找到目标函数
    ret
END_FUNCTION art_jni_dlsym_lookup_stub



//[jni_entrypoints.cc->artFindNativeMethod]
extern "C" void* artFindNativeMethod(Thread* self) {
    Locks::mutator_lock_->AssertNotHeld(self);        // We come here as Native.
    ScopedObjectAccess soa(self);

    ArtMethod* method = self->GetCurrentMethod(nullptr);
    
    //调用JavaVMExt的FindCodeForNativeMethod，搜索目标函数
    //即通过【Java native方法】 去寻找 【Native层方法】
    void* native_code = soa.Vm()->FindCodeForNativeMethod(method);
    
    if (native_code == nullptr) { 
        return nullptr;
    }
    else {
        /*如果存在满足条件的目标函数，则更新 ArtMethod 对象的JNI机器码入口地址，
          此后再调用这个Java native方法，则无须借助 art_jni_dlsym_lookup_stub。  
        */
        //【A_8.2_初识JNI相关辅助类】
        method->RegisterNative(native_code, false);
        return native_code;
    }
}





//【11.2.2】　art_quick_generic_jni_trampoline
//[quick_entrypoints_x86.S->art_quick_generic_jni_trampoline]
DEFINE_FUNCTION art_quick_generic_jni_trampoline {
    /*该宏功能类似SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME宏（读者可回顾10.1.3节）。最后
      还会将EAX寄存器的值压入栈中。注意，EAX寄存器里保存的是代表Java native方法的ArtMethod
      对象。  */
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_EAX
    /*保存栈顶的位置（由寄存器ESP指明）到EBP寄存器中。结合上面的宏可知，此时栈顶存储的是Art-
      Method对象，所以EBP将指向目标Java native方法对应的ArtMethod对象 */
    movl %esp, %ebp
        ......
    /*下面两条指令用于向低地址拓展栈，一共扩展了5128字节（不考虑对齐的问题）。这部分空间是用来
      准备HandleScope和Native函数所需参数的。注意，此时还不知道实际需要多少栈空间，所以先暂时分配5128字节。   
    */
    subl LITERAL(5120), %esp
    subl LITERAL(8), %esp
    pushl %ebp                                //为调用下面的 artQuickGenericJniTrampoline 函数准备参数
    pushl %fs:THREAD_SELF_OFFSET              //获取代表当前调用线程的Thread对象
    call SYMBOL(artQuickGenericJniTrampoline) //调用artQuickGenericJniTrampoline函数。
    
    //artQuickGenericJniTrampoline 函数返回值通过EAX和EDX两个寄存器返回
    //如果EAX的值为0，则表示有异常发生
    test %eax, %eax
    
    jz .Lexception_in_native //有异常发生，转到对应位置去处理
    //如果EAX的值不为0，则EAX的内容就是目标Native函数的地址，而EDX则指向该函数对应参数的栈
    //空间位置。下面的指令将把EDX的值赋给ESP寄存器，此后，ESP所指的栈空间存储的就是Native函数所需的参数
    movl %edx, %esp
    call *%eax //【* 重点】调用Native函数
    
    //下面的指令将为调用artQuickGenericJniEndTrampoline函数做准备
    subl LITERAL(20), %esp
    fstpl (%esp)
    pushl %edx
    pushl %eax
    pushl %fs:THREAD_SELF_OFFSET
    call SYMBOL(artQuickGenericJniEndTrampoline)
    //再次判断是否有异常发生
    mov %fs:THREAD_EXCEPTION_OFFSET, %ebx
    testl %ebx, %ebx
    jnz .Lexception_in_native
    movl %ebp, %esp
    addl LITERAL(4 + 4 * 8), %esp

    POP ecx
    ......
    ret //Java native方法返回

    .Lexception_in_native: //异常处理
    movl %fs:THREAD_TOP_QUICK_FRAME_OFFSET, %esp
    call .Lexception_call
.Lexception_call:
    DELIVER_PENDING_EXCEPTION //我们在10.6节介绍过这个宏
END_FUNCTION art_quick_generic_jni_trampoline
}



//【11.2.2.1】　artQuickGenericJniTrampoline  
//artQuickGenericJniTrampoline的代码如下所示。

//[quick_trampoline_entrypoints.cc->artQuickGenericJniTrampoline]
extern "C" TwoWordReturn artQuickGenericJniTrampoline(Thread* self, ArtMethod** sp)  {
    
    /*注意该函数的参数sp，其类型是ArtMethod**。从上面的汇编指令可知，这个位置的栈上放置的是
      EBP寄存器的值，EBP寄存器的值又是之前ESP的栈顶位置，而ESP的栈顶位置保存的又是目标ArtMethod对象的地址。
      所以，此处sp的数据类型是ArtMethod**。该函数的返回值TwoWordReturn是一个64位长的整数。
      在x86平台上，当函数返回值为64位长时，其高32字节内容存储在EDX寄存器中，而低32字节内容存储在EAX中。
    */
    ArtMethod* called = *sp;      //called类型为ArtMethod*。
    uint32_t shorty_len = 0;
    const char* shorty = called->GetShorty(&shorty_len);
    
    /*下面这四行代码将根据Java native函数的签名信息计算Native函数所需的栈空间以及准备参数。
      注意，如果native方法里包含引用类型参数的话，ART虚拟机将用到HandleScope结构体（这一块
      内容请读者回顾9.5.3.1.4节）。另外，这段代码执行完后，sp的值将发生变化。
    */
    
    BuildGenericJniFrameVisitor visitor(self, called->IsStatic(), shorty, shorty_len, &sp);
    visitor.VisitArguments();
    visitor.FinalizeHandleScope(self);

    //SetTopOfStack 内部将调用 tlsPtr_.managed_stack.SetTopQuickFrame
    self->SetTopOfStack(sp);
    self->VerifyStack();

    uint32_t cookie;
    if (called->IsSynchronized()) {
        ......
    }
    else { 
        //调用JniMethodStart，我们在9.5.3节中也介绍过它。cookie的含义和JNI对Local引用对象的管理有关。
        cookie = JniMethodStart(self);
    }
    
    //下面这两行代码的含义是将cookie的值存储到栈上。注意，此时sp的取值不再是本函数进来时所传递的值。
    uint32_t* sp32 = reinterpret_cast<uint32_t*>(sp);
    *(sp32 - 1) = cookie; //注意cookie所存储的位置。

    /*获取ArtMethod对象的【JNI机器码】入口。如果机器码入口地址为 art_jni_dlsym_lookup_stub，
      则说明该Java native方法还未和目标Native函数绑定。这时将调用 artFindNativeMethod 函数来查找目标Native函数。 
            > 注意：artFindNativeMethod 内部会调用 vm->FindCodeForNativeMethod(method);
    */
    void* nativeCode = called->GetEntryPointFromJni();
    if (nativeCode == GetJniDlsymLookupStub()) {
#if defined(__arm__) || defined(__aarch64__)
    nativeCode = artFindNativeMethod();
#else
    /*这里要特别注意，上文介绍 art_jni_dlsym_lookup_stub 函数的内容时，
      我们发现如果调用 stub 函数的话能直接转到对应Native函数去执行（假设存在目标Native函数）。
      但此处并没有调用 stub 函数，而是判断JNI机器码入口地址是否指向stub函数，如果是，则直接调用 artFindNativeMethod 获取目标Native函数的地址。  
    */
    nativeCode = artFindNativeMethod(self);  //获取目标函数地址
#endif
    }
    
    /*在 art_quick_generic_jni_trampoline 汇编代码中，我们分配了一块多达 5128 字节的栈空间。
      显然，我们不需要这么大的栈空间。
      所以，visitor.GetBottomOfUsedArea 将返回针对此次Native函数调用所需栈空间的位置。而nativeCode则为Native函数的地址。
      从artQuickGenericJniTrampoline 返回后：
          EDX寄存器保存了栈顶位置。汇编代码中通过 mov %ebp,%esp更新ESP
          EAX寄存器保存了【Native函数】地址，汇编代码中通过 call *(%eax)执行Native函数
    */
    return GetTwoWordSuccessValue(
        reinterpret_cast<uintptr_t>(visitor.GetBottomOfUsedArea()), 
        reinterpret_cast<uintptr_t>(nativeCode)
    );