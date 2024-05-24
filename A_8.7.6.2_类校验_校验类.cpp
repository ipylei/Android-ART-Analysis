//现在来看类的校验，代码位于VerifyClass中，如下所示。
//[method_verifier.cc->MethodVerifier::VerifyClass]
MethodVerifier::FailureKind MethodVerifier::VerifyClass(Thread* self,
                                                        mirror::Class* klass,
                                                        ......) {      //待校验的类由kclass表示
                                                        
    //如果该类已经被校验过，则直接返回校验成功
    if (klass->IsVerified()) {  
        return kNoFailure; 
    }
    bool early_failure = false;
    std::string failure_message;
    
    //获取该class所在的Dex文件信息及该类在Dex文件里的class_def信息
    const DexFile& dex_file = klass->GetDexFile();
    const DexFile::ClassDef* class_def = klass->GetClassDef();
    
    //获取该类的基类对象
    mirror::Class* super = klass->GetSuperClass();
    
    std::string temp;
    //下面这个判断语句表示kclass没有基类，而它又不是Java Object类。显然，这是违背Java语言规范的
    //Java中，只有Object类才没有基类
    if (super == nullptr && strcmp("Ljava/lang/Object;", klass->GetDescriptor(&temp)) != 0) {
        early_failure = true;
        failure_message = " that has no super class";
    } else if (super != nullptr && super->IsFinal()) {
        //如果基类有派生类的话，基类不能为final
        early_failure = true;
    } else if (class_def == nullptr) { //该类在Dex文件里没有class_def信息
        early_failure = true;
    }
    
    
    if (early_failure) {
        ......
        if (callbacks != nullptr) {
        //callbacks的类型是 CompilerCallbacks，dex字节码转机器码的时候会用上它
        ClassReference ref(&dex_file, klass->GetDexClassDefIndex());
        callbacks->ClassRejected(ref);
        }
        return kHardFailure; //返回校验错误
    }
    
    StackHandleScope<2> hs(self);
    Handle<mirror::DexCache> dex_cache(hs.NewHandle(klass->GetDexCache()));
    Handle<mirror::ClassLoader> class_loader(hs.NewHandle(klass->GetClassLoader()));
    //进一步校验
    return VerifyClass(self,&dex_file,dex_cache,......);
}


//[method_verifier.cc->MethodVerifier::VerifyClass]
MethodVerifier::FailureKind MethodVerifier::VerifyClass(Thread* self,
                                                        const DexFile* dex_file,
                                                        Handle<mirror::DexCache> dex_cache,
                                                        Handle<mirror::ClassLoader> class_loader,
                                                        const DexFile::ClassDef* class_def,
                                                        CompilerCallbacks* callbacks,
                                                        bool allow_soft_failures,
                                                        LogSeverity log_level,
                                                        std::string* error) {
                                                            
    if ((class_def->access_flags_ & (kAccAbstract | kAccFinal)) == (kAccAbstract | kAccFinal)) {
        return kHardFailure; //类不能同时是final又是abstract
    }

    const uint8_t* class_data = dex_file->GetClassData(*class_def);
    if (class_data == nullptr) { 
        return kNoFailure; 
    }
    //创建 ClassDataItemIterator 迭代器对象，通过它可以获取目标类的 class_data_item 里的内容
    ClassDataItemIterator it(*dex_file, class_data);
    //不校验类的成员变量
    while (it.HasNextStaticField() || it.HasNextInstanceField()) {
        it.Next();
    }
    ClassLinker* linker = Runtime::Current()->GetClassLinker();
    //对本类所定义的Java方法进行校验。VerifyMethods在上一节已经介绍过了
      // Direct methods.
    MethodVerifier::FailureData data1 = VerifyMethods<true>(self,
                                                              linker,
                                                              dex_file,
                                                              class_def,
                                                              &it,
                                                              dex_cache,
                                                              class_loader,
                                                              callbacks,
                                                              allow_soft_failures,
                                                              log_level,
                                                              false /* need precise constants */,
                                                              error);
    //校验本类中的 virtual_methods 数组
    // Virtual methods.
    MethodVerifier::FailureData data2 = VerifyMethods<false>(self,
                                                           linker,
                                                           dex_file,
                                                           class_def,
                                                           &it,
                                                           dex_cache,
                                                           class_loader,
                                                           callbacks,
                                                           allow_soft_failures,
                                                           log_level,
                                                           false /* need precise constants */,
                                                           error);
                                                       
    //将校验结果合并到data1的结果中
    data1.Merge(data2);
    
    //校验结果通过合并后的data1的成员来判断
    if (data1.kind == kNoFailure) { 
        return kNoFailure; 
    }
    else { 
        return data1.kind;
    }
}