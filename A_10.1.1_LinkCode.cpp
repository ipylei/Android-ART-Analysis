//[class_linker.cc->ClassLinker::LinkCode]
void ClassLinker::LinkCode(ArtMethod* method, 
                           const OatFile::OatClass* oat_class,
                           uint32_t class_def_method_index
                           ) {
    Runtime* const runtime = Runtime::Current();
    ......
    /*在下面的代码中：
     （1）oat_class 的类型为 OatFile::OatClass，其内容由oat文件中OatClass区域相应位置处的信息构成。
     （2）oat_method 的类型为 OatFile::OatMethod，其内容由oat文件中 OatMethod 区域对应的 OatQuickMethodHeader 信息构成。
   */
    if (oat_class != nullptr) {
        /*获取该Java方法对应的OatMethod信息。如果它没有被编译过，则返回的OatMethod对象的 code_offset_ 取值为0。
          OatMethod.code_offset_ 指向对应机器码在oat文件中的位置。其值为0就表示该方法不存在机器码。  
        */
        const OatFile::OatMethod oat_method = oat_class->GetOatMethod(class_def_method_index);
            
        /*设置ArtMethod ptr_sized_fields_.entry_point_from_quick_compiled_code_ 为Oat文件区域OatQuickMethodHeader的code_。
          读者可回顾第9章图9-41"oat和art文件的关系"。code_处存储的就是该方法编译得到的机器码。
          注意，为节省篇幅，笔者以后用机器码入口地址来指代 entry_point_from_quick_compiled_code_ 成员变量。
        */
        oat_method.LinkMethod(method);
    }
    
    //获取ArtMethod对象的机器码入口地址
    const void* quick_code = method->GetEntryPointFromQuickCompiledCode();
    
    /*在 ShouldUseInterpreterEntrypoint 函数中，如果机器码入口地址为空（该方法没有经过编译），或者虚拟机进入了调试状态，
    则必须使用解释执行的模式。这种情况下，该函数返回值 enter_interpreter 为 true。 
    */
    bool enter_interpreter = ShouldUseInterpreterEntrypoint(method, quick_code);
    
    //蹦床
    
    //1.静态、非构造函数        
    if (method->IsStatic() && !method->IsConstructor()) {
    /*如果method为静态且不是类初始化"<clinit>"（它是类的静态构造方法）方法，
      则设置【机器码】入口地址为 【art_quick_resolution_trampoline】。
      根据9.5.4.4.1节的介绍可知。该地址对应的是一段跳转代码，跳转的目标是 【artQuickResolutionTrampoline】 函数。
        //注：qpoints->pQuickResolutionTrampoline = art_quick_resolution_trampoline;
      它是一个特殊的函数，和类的解析有关。注意，虽然在LinkCode中（该函数是在类初始化之前被调用的）设置的跳转目标为 artQuickResolutionTrampoline，
      但ClassLinker在初始化类的 InitializeClass 函数的最后会通过调用 FixupStaticTrampolines 来尝试更新此处所设置的跳转地址为正确的地址。
    */
      method->SetEntryPointFromQuickCompiledCode(GetQuickResolutionStub());
    } 
    
    //2.【首先一定不是JNI方法，即非JNI方法】，然后才有下面的判断。 (若为JNI方法则一定为false)
    //【机器码】入口地址为空（该方法没有经过编译），或者虚拟机进入了调试状态
    //【10.2.1、10.2.2、10.2.3】详细介绍了 art_quick_to_interpreter_bridge
    else if (enter_interpreter) {
        /*enter_interpreter 的取值来自 ShouldUseInterpreterEntrypoint，一般而言，如果该
          方法没有对应的机器码，或者在调试运行模式下，则 enter_interpreter 为 true。
          对应的【机器码】入口地址为 art_quick_to_interpreter_bridge 跳转代码，
          其跳转的目标为 artQuickToInterpreterBridge 函数。
        */
        method->SetEntryPointFromQuickCompiledCode(GetQuickToInterpreterBridge());
    }
    
    //3.JNI方法 且 不存在机器码，那么设置【机器码】入口
    //【详情参考 11.2】
    else if (method->IsNative() && quick_code == nullptr) {
        /*如果method为jni方法，并且不存在机器码，
        则设置【机器码】入口地址为跳转代码 art_quick_generic_jni_trampoline，
        它的跳转目标为 artQuickGenericJniTrampoline 函数。
        */
        method->SetEntryPointFromQuickCompiledCode(GetQuickGenericJniStub());
    } 
    
    //4.？是JNI方法，且有机器码的情况呢？
    //(都有机器码了，当然不用管啊! 上面就是在设置【机器码】入口地址)     
    //详情参考【11.2】 这段机器码本身会跳转到这个ArtMethod对象的JNI机器码入口地址。
    //还可以参考【9.5.3】 Jni方法编译
    
    
    //5.？非JNI方法，但有机器码的情况？
    //(都有机器码了，当然不用管啊! 上面就是在设置【机器码】入口地址)     
    //可以参考【9.5.4】 dexto机器码
    
    
    //.JNI方法的情况，设置【jni机器码】入口
    //【11.2】如果这个JNI方法没有注册过（即这个native方法还未和Native层对应的函数相关联），这个JNI机器码入口地址是 art_jni_dlsym_lookup_stub。
    //否则，JNI机器码入口地址指向Native层对应的函数。
    if (method->IsNative()) {
        /*如果为jni方法，则调用ArtMethod 的 UnregisterNative 函数，
          其内部主要设置 ArtMethod tls_ptr_sized_.entry_point_from_jni_ 成员变量为跳转代码 art_jni_dlsym_lookup_stub ，
          跳转目标为 artFindNativeMethod 函数。
          为简单起见，笔者以后用jni机器码入口地址指代 entry_point_from_jni_ 成员变量。这部分内容和JNI有关，我们以后再讨论它。
        */
        method->UnregisterNative();
        
        if (enter_interpreter || quick_code == nullptr) {
            const void* entry_point = method->GetEntryPointFromQuickCompiledCode();
            DCHECK(IsQuickGenericJniStub(entry_point) || IsQuickResolutionStub(entry_point));
      }
    }
}


/*  总结

非JNI方法
    有机器码：机器码执行模式 (机器码入口)
    无机器码：解释执行模式： (机器码入口更新为：art_quick_to_interpreter_bridge)


JNI方法
    有机器码：机器码入口为(一串汇编代码，本身会跳转到JNI机器码入口地址)
                => jni机器码入口
                    > 已注册：为 Native 层函数
                    > 未注册：为 art_jni_dlsym_lookup_stub
                        1.bl artFindNativeMethod(注册Native层函数；返回Native层函数)
                        2.执行【Native层函数】
                    
              
    无机器码：机器码入口设置为(art_quick_generic_jni_trampoline 即通用蹦床)
                    1.bl artQuickGenericJniTrampoline(寻找并返回Native层函数地址，同时未注册的情况下还进行注册)
                    2.执行【Native层函数】
    
    【所以】：
        有机器码：机器码入口 -> JNI机器码入口 -> Native层函数地址
        无机器码：机器码入口 -> Native层函数地址
*/


//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/util/Slog.java
//日志库：import android.util.Slog;
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/LoadedApk.java#mApplicationInfo
//用法：Slog.v(ActivityThread.TAG, "Class path: " + zip + ", JNI path: " + libraryPath);
