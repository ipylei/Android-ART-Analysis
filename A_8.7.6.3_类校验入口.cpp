//[class_linker.cc->ClassLinker::VerifyClass]
void ClassLinker::VerifyClass(Thread* self, Handle<mirror::Class> klass,
                                LogSeverity log_level) {
    {                          
        ...... //可能有另外一个线程正在处理类的校验，此处省略的代码将处理这种情况判断该类是否
        
        //已经通过校验（类状态大于或等于kStatusVerified）
        if (klass->IsVerified()) {
            /* Verify一个类是需要代价的（比如执行上两节代码所示MethodVerifier的相关函数是
               需要花费时间），但付出这个代价会带来一定好处。在ART虚拟机中，如果某个类校验通
               过的话，后续执行该类的方法时将跳过所谓的访问检查（Access Check）。Access Check
               的具体内容将在后续章节介绍。此处举一个简单例子，比如访问检查将判断外部调用者是
               否调用了某个类的私有函数。显然，Access Check将影响函数执行的时间。
               下面的这个 EnsureSkipAccessChecksMethods 将做两件事情：
              （1）为klass methods_ 数组里的ArtMethod对象设置 kAccSkipAccessChecks 标志位
              （2）为klass设置kAccVerificationAttempted标志位。这个标记位表示该类已经尝试过校验了，无须再次校验。
                    这些标志位的作用我们以后碰见具体代码时再讲解。  
                */
                EnsureSkipAccessChecksMethods(klass);
                return;
        }
        
        //如果类状态大于等于 kStatusRetryVerificationAtRuntime 并且当前进程是 dex2oat
        //(Is-AotCompiler用于判断当前进程是否为编译进程)，则直接返回。类的校验将留待真实的虚拟机进程来完成。
        if (klass->IsCompileTimeVerified() &&
            Runtime::Current()->IsAotCompiler()) {  
            return;  
        }

        if (klass->GetStatus() == mirror::Class::kStatusResolved) {
            //设置类状态为kStatusVerifying，表明klass正处于类校验阶段。
            mirror::Class::SetStatus(klass, mirror::Class::kStatusVerifying, self);
        } else { 
            //如果类的当前状态不是kStatusResolved，则表明该类在 dex2oat 时已经做过校验，
            //但校验结果是 kStatusRetryVerificationAtRuntime。所以此处需要在完整虚拟机环境下再做校验。
            mirror::Class::SetStatus(klass, mirror::Class::kStatusVerifyingAtRuntime, self);
        }
            
            
        /*IsVerificationEnabled 用于返回虚拟机是否开启了类校验的功能，它和verify_mode.h中定义
          的枚举变量VerifyMode有关。该枚举变量有三种取值：
          （1）kNone：不做校验。
          （2）kEnable：标准校验流程。其中，在dex2oat过程中会尝试做预校验（preverifying）。
          （3）kSoftFail：强制为软校验失败。这种情况下，指令码在解释执行的时候会进行access check。
                这部分内容在dex2oat一章中有所体现，以后我们会提到。
          从上述内容可知，在ART里，类校验的相关知识绝不仅仅只包含JLS规范中提到的那些诸如检查
          字节码是否合法之类的部分，它还和dex2oat编译与Java方法如何执行等内容密切相关。
        */
        if (!Runtime::Current()->IsVerificationEnabled()) {
            mirror::Class::SetStatus(klass, mirror::Class::kStatusVerified, self);
            EnsureSkipAccessChecksMethods(klass);
            return;
        }
    }
    
    
    
    //先校验父类。AttemptSuperTypeVerification 内部也会调用VerifyClass。
    ......
    //请读者自行阅读它
    MutableHandle<mirror::Class> supertype(hs.NewHandle(klass->GetSuperClass()));
    if (supertype.Get() != nullptr && !AttemptSupertypeVerification(self, klass, supertype)) {    
        return;    
    }

    //如果父类校验通过，并且klass不是接口类的话，我们还要对klass所实现的接口类进行校验。
    //校验接口类是从Java 1.8开始，接口类支持定义有默认实现的接口函数，默认函数包含实际的内容，所以需要校验。
    if ((supertype.Get() == nullptr || supertype->IsVerified()) && !klass->IsInterface()) {
        int32_t iftable_count = klass->GetIfTableCount();
        MutableHandle<mirror::Class> iface(hs.NewHandle<mirror::Class>(nullptr));
        //遍历IfTable，获取其中的接口类。
        for (int32_t i = 0; i < iftable_count; i++) {
            iface.Assign(klass->GetIfTable()->GetInterface(i));
            //接口类没有默认接口函数，或者已经校验通过，则略过
            if (LIKELY(!iface->HasDefaultMethods() || iface->IsVerified())) {
                continue;
            } else if (UNLIKELY(!AttemptSupertypeVerification(self, klass, iface))) {
                //接口类校验失败。直接返回
                return;      
            }  else if (UNLIKELY(!iface->IsVerified())) {
                //如果接口类校验后得到的状态为kStatusVerifyingAtRuntime，则跳出循环
                supertype.Assign(iface.Get());
                break;
            }
        }
    }
    
    const DexFile& dex_file = *klass->GetDexCache()->GetDexFile();
    mirror::Class::Status oat_file_class_status(mirror::Class::kStatusNotReady);
        
    /*下面这个函数其实只是用来判断klass是否已经在 dex2oat 阶段做过 预校验 了。这需要结合该类编译
      结果来决定（包含在OatFile中的类状态）。除此之外，如果我们正在编译系统镜像时（即在dex2oat
      进程中编译包含在Boot Image的类），则该函数也返回 false。
      
      preverified如果为false，则将调用MethodVerifier::VerifyClass来做具体的校验工作。
      VerifyClassUsingOatFile 还包含其他几种返回false的情况，请读者自行阅读。
    */
    bool preverified = VerifyClassUsingOatFile(dex_file, klass.Get(), oat_file_class_status);
    
    verifier::MethodVerifier::FailureKind verifier_failure = verifier::MethodVerifier::kNoFailure;
    //没有 预校验 的处理
    if (!preverified) {            
        Runtime* runtime = Runtime::Current();
        //调用MethodVerifier::VerifyClass来完成具体的校验工作
        verifier_failure = verifier::MethodVerifier::VerifyClass(self, 
                                                                 klass.Get(), 
                                                                 runtime->GetCompilerCallbacks(),
                                                                 ...);
    }
    ......
    
    if (preverified || verifier_failure != verifier::MethodVerifier::kHardFailure) {
        ......
        if (verifier_failure == verifier::MethodVerifier::kNoFailure) {
            //自己和基类（或者接口类）的校验结果都正常，则类状态设置为kStatusVerified
            if (supertype.Get() == nullptr || supertype->IsVerified()) {
            mirror::Class::SetStatus(klass, mirror::Class::kStatusVerified, self);
            } else {
                ......
            }
        
        } 
        else {
            //对应校验结果为 kSoftFail 的情况
            if (Runtime::Current()->IsAotCompiler()) { 
                //如果是dex2oat中出现这种情况，则设置类的状态为 kStatusRetryVerificationAtRuntime
                mirror::Class::SetStatus(klass, mirror::Class::kStatusRetryVerificationAtRuntime, self);
            } else {
                /*设置类状态为kStatusVerified，并且设置类标记位 kAccVerificationAttempted。
                  注意，我们在上面代码中介绍过 EnsureSkipAccessChecksMethods 函数。这个函数将
                     （1）为klass methods_数组里的ArtMethod对象设置 kAccSkipAccessChecks 标志位
                     （2）为klass设置 kAccVerificationAttempted 标志位。
                     
                  而下面的代码只设置了（2），没有设置（1）。所以，虽然类状态为 kStatusVerified，
                  但在执行其方法时可能还要做 Access Check   */
                mirror::Class::SetStatus(klass, mirror::Class::kStatusVerified, self);
                klass->SetVerificationAttempted();
            }
        }
    } else {
        ......  
    }
    
    ....... //其他处理，略过
                                                   
}