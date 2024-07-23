//8.7.6.1　MethodVerifier::VerifyMethods
//首先是对Java成员方法的校验。入口函数是VerifyMethods，代码如下所示。
//[method_verifier.cc->MethodVerifier::VerifyMethods]
template <bool kDirect>
MethodVerifier::FailureData MethodVerifier::VerifyMethods(Thread* self,
                                                        ClassLinker* linker,
                                                        const DexFile* dex_file,
                                                        const DexFile::ClassDef* class_def,
                                                        ClassDataItemIterator* it,
                                                        ...) {
    /* 注意这个函数的参数和返回值：
       （1）该函数为模板函数。结合图8-7，如果模板参数kDirect为true，则校验的将是目标类中
            direct_methods 的内容，否则为 virtual_methods 的内容。
       （2）class_def 的类型为DexFile::ClassDef。它是Dex文件里的一部分，class_def中最重
           要的信息存储在 class_data_item 中，而 class_data_item 的内容可借助迭代器it来获取。
       （3）self 代表当前调用的线程对象。dex_file 是目标类所在的Dex文件。
       （4）返回值的类型为 FailureData 。它是 MethodVerifier 定义的内部类，其内部有一个名为kind
        的成员变量，类型为枚举 FailureKind ，取值有如下三种情况：
            a) kNoFailure，表示校验无错。
            b) kSoftFailure，表示校验软错误，该错误发生在从dex字节码转换为机器码时所做的校验过程
               中。编译过程由 dex2oat 进程完成。dex2oat 是一个简单的，仅用于编译的虚拟机进程，它包
               含了前文提到的诸如heap、runtime等重要模块，但编译过程不会将所有相关类都加载到虚拟
               机里，所以可能会出现编译过程中校验失败的情况。kSoftFailure失败并没有关系，这个类
               在后续真正使用之时，虚拟机还会再次进行校验。
            c) kHardFailure，表示校验错误，该错误表明校验失败。
    */
    MethodVerifier::FailureData failure_data;
    int64_t previous_method_idx = -1;
    
    /*同上，如果kDirect为true，则遍历class_data_item信息里的direct_methods数组，
      否则遍历其中的virtual_methods数组（代表虚成员函数）。*/
    while (HasNextMethod<kDirect>(it)) {
        self->AllowThreadSuspension();
        uint32_t method_idx = it->GetMemberIndex();
        ......
        previous_method_idx = method_idx;
        
        /*InvokeType 是枚举变量，和dex指令码里函数调用的方式有关：取值包括：
            （1）kStatic：对应invoke-static相关指令，调用类的静态方法。
            （2）kDirect：对应invoke-direct相关指令，指调用那些非静态方法。包括两类，一类是
             private修饰的成员函数，另外一类则是指类的构造函数。符合kStatic和kDirect调用类型
             函数属于上文所述的direct methods（位于类的direct_methods数组中）。
            （3）kVirtual：对应invoke-virtual相关指令，指调用非private、static或final修饰
            的成员函数（注意，不包括调用构造函数）。
            （4）kSuper：对应invoke-super相关指令，指在子类的函数中通过super来调用直接父类的
             函数。注意，dex官方文档只是说invoke-super用于调用直接父类的virtual函数。但笔者测试
             发现，必须写成"super.父类函数"的形式才能生成invoke-super指令。
            （5）kInterface：对应invoke-interface相关指令，指调用接口中定义的函数。
             以上枚举变量的解释可参考第3章最后所列举的谷歌官方文档。
             下面代码中的GetMethodInvokeType是迭代器ClassDataItemIterator的成员函数，它将
             返回当前所遍历的成员函数的调用方式（InvokeType）。注意，该函数返回kSuper的逻辑和官
             方文档对kSuper的描述并不一致。按照该函数的逻辑，它永远也不可能返回kSuper。笔者在模拟
             器上验证过这个问题，此处从未有返回过kSuper的情况。感兴趣的读者不妨做一番调研 
        */
         InvokeType type = it->GetMethodInvokeType(*class_def);
         
         //调用ClassLinker的 ResolveMethod 进行解析，下文将介绍此函数。
         //【8.7.8】
         ArtMethod* method = linker->ResolveMethod<ClassLinker::kNoICCECheckForCache>(*dex_file, 
                                                                                        method_idx,
                                                                                        dex_cache,
                                                                                        class_loader, 
                                                                                        nullptr, 
                                                                                        type);
        ......
        //调用另外一个VerifyMethod函数，其代码见下文
        MethodVerifier::FailureData result = VerifyMethod(self,method_idx,...);
        ......
    }
    return failure_data;
}



//接着来看另外一个 VerifyMethod 函数，代码如下所示。
//[method_verifier.cc->MethodVerifier::VerifyMethod]
MethodVerifier::FailureData MethodVerifier::VerifyMethod(Thread* self,
                                                          ClassLinker* linker,
                                                          const DexFile* dex_file,
                                                          const DexFile::ClassDef* class_def,
                                                          ClassDataItemIterator* it,
                                                          Handle<mirror::DexCache> dex_cache,
                                                          Handle<mirror::ClassLoader> class_loader,
                                                          CompilerCallbacks* callbacks,
                                                          bool allow_soft_failures,
                                                          LogSeverity log_level,
                                                          bool need_precise_constants,
                                                          std::string* error_string) {
    MethodVerifier::FailureData result;
    
    /*创建一个 MethodVerifier 对象，然后调用它的 Verify 方法。其内部将校验method（类型为Art Method*）所代表的Java方法。
    该方法对应的字节码在code_item（对应dex文件格式里的code_item）中。  */
    MethodVerifier verifier(self,dex_file, dex_cache, class_loader, ......);
    
    //Verify返回成功，表示校验通过。即使出现 kSoftFailure 的情况，该函数也会返回true
    if (verifier.Verify()) {
        ......
		if (code_item != nullptr && callbacks != nullptr) {
			// Let the interested party know that the method was verified.
			callbacks->MethodVerified(&verifier);
		}
		
        //failures_ 的类型为vector<VerifyError>。VerifyError为枚举变量，定义了校验中可能出现的错误情况
        if (verifier.failures_.size() != 0) { 
            result.kind = kSoftFailure; 
        }
        if (method != nullptr) {
            .....
        }
    } else {
        /*Verify返回失败，但若错误原因是一些试验性指令导致的，则也属于软错误，Dex指令码中有
          一些属于试验性质的指令，比如invokelambda。搜索dex_instruction.h文件里带kExperimental标记的指令码，
          即是ART虚拟机所支持的试验性指令  
         */
        if (UNLIKELY(verifier.have_pending_experimental_failure_)) {
            result.kind = kSoftFailure;
        } else { 
            result.kind = kHardFailure;
        }
    }
    
    ......
    
    return result;
}




//校验的关键在MethodVeifier类的Verify函数中，马上来看它。
//[method_verifier.cc->MethodVerifier::Verify]
bool MethodVerifier::Verify() {
    //从dex文件里取出该方法对应的 method_id_item 信息
    const DexFile::MethodId& method_id = dex_file_->GetMethodId(dex_method_idx_);
    //取出该函数的函数名
    const char* method_name = dex_file_->StringDataByIdx(method_id.name_idx_);
    
    /*根据函数名判断其是类实例的构造函数还是类的静态构造函数。代码中，"<init>"叫类实例构造函数
      （instance constructor），而"<clinit>"叫类的静态构造函数（static constructor）。 */
    bool instance_constructor_by_name = strcmp("<init>", method_name) == 0;
    bool static_constructor_by_name = strcmp("<clinit>", method_name) == 0;
    //上述条件有一个为true，则该函数被认为是构造函数
    bool constructor_by_name = instance_constructor_by_name || static_constructor_by_name;
    
    /*如果该函数的访问标记（access flags，可参考第3章表3-1）自己为构造函数，而函数名又不符合要
      求，则设置校验的结果为VERIFY_ERROR_BAD_CLASS_HARD（VerifyError枚举值中的一种）。Fail
      函数内部会处理VerifyError里定义的不同错误类型。其中以HARD结尾的枚举变量表示为硬错误  */
    if ((method_access_flags_ & kAccConstructor) != 0) {
        if (!constructor_by_name) {
            Fail(VERIFY_ERROR_BAD_CLASS_HARD)
                << "method is marked as constructor, but not named accordingly";
            return false;
        }
        is_constructor_ = true;
    } else if (constructor_by_name) { 
        is_constructor_ = true; 
    }
    
    
    //code_item_ 代表该函数的内容，如果为nullptr，则表示这个函数为抽象函数或native函数
    if (code_item_ == nullptr) {
        //既不是抽象函数，也不是native函数，但又没有函数内容，校验肯定会失败
        if ((method_access_flags_ & (kAccNative | kAccAbstract)) == 0) {
            Fail(VERIFY_ERROR_BAD_CLASS_HARD) << ......; //错误原因;
            return false;
        }
        return true;
    }
    
    /*参考3.2.4节可知，ins_size_ 表示输入参数所占虚拟寄存器的个数，而 registers_size_ 表示该
      函数所需虚拟寄存器的总个数。显然，下面这个if条件为true的话，这个函数肯定会校验失败  */
    if (code_item_->ins_size_ > code_item_->registers_size_) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD) << ......;
        return false;
    }
    //insn_flags_ 将保存该方法里的指令码内容
    insn_flags_.reset(arena_.AllocArray<InstructionFlags>(code_item_->insns_size_in_code_units_));
    std::uninitialized_fill_n(insn_flags_.get(), code_item_->insns_size_in_code_units_, InstructionFlags());
    
    //下面四个函数将对指令码的内容进行校验。读者不拟介绍它们，感兴趣的读者不妨自行研究。
    bool result = ComputeWidthsAndCountOps();
    result = result && ScanTryCatchBlocks();
    result = result && VerifyInstructions();
    result = result && VerifyCodeFlow();
    
    return result;
}