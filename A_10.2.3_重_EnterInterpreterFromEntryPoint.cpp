/*
上述 artQuickToInterpreterBridge 代码中，暂时不考虑HDeoptimize及Thread对栈管理的处理逻辑，它的主要功能就是：
·构造 ShadowFrame 对象，并借助 BuildQuickShadowFrameVisitor 将该方法所需的参数存储到这个 ShadowFrame 对象中。
·进入 EnterInterpreterFromEntryPoint，这就是解释执行模式的核心处理函数。马上来看它。
*/
//【10.2.3】　EnterInterpreterFromEntryPoint
//我们先观察EnterInterpreterFromEntryPoint函数的【声明】。如下所示。
//提示　参考图10-7，我们依然称所调用的Java方法为B，其ArtMethod对象为ArtMethod*B。
//[interpreter.h->EnterInterpreterFromEntryPoint]
extern JValue EnterInterpreterFromEntryPoint(Thread* self,  //代表调用线程的Thread对象
                                            const DexFile::CodeItem* code_item,  //方法B的dex指令码内容
                                            ShadowFrame* shadow_frame            //方法B所需的参数
);


//现在来看其【定义】。
//[interpreter.cc->EnterInterpreterFromEntryPoint]
JValue EnterInterpreterFromEntryPoint(Thread* self,
                                      const DexFile::CodeItem* code_item, 
                                      ShadowFrame* shadow_frame) {
    ......
    
    //下面这段代码和JIT有关，相关知识见本章后续对JIT的介绍
    jit::Jit* jit = Runtime::Current()->GetJit();
    if (jit != nullptr) {
        jit->NotifyCompiledCodeToInterpreterTransition(self, shadow_frame->GetMethod());
    }
    
    //【*】关键函数
    return Execute(self, code_item, *shadow_frame, JValue());
}



//来看Execute函数，代码如下所示。
//[interpreter.cc->Execute]
static inline JValue Execute(Thread* self,
                            const DexFile::CodeItem* code_item,
                            ShadowFrame& shadow_frame,
                            JValue result_register,
                            bool stay_in_interpreter = false) {
    /*注意 stay_in_interpreter 参数，它表示是否强制使用解释执行模式。
      默认为false，它表示如果方法B存在jit编译得到的机器码，则转到jit去执行。    
    */
    
    /*下面这个if条件的判断很有深意。我们在本章解释图10-5里ShadowFrame成员变量时曾说过，如果
      是HDeoptimize的情况，ShadowFrame 的dex_pc_不是0（这表示有一部分指令以机器码方式执行）。
      如果dex_pc_为0的话，则表示该方法从一开始就将以解释方式执行。我们称这种情况为纯解释执行的方法，
      此时，我们就需要检查它是否存在JIT的情况。  
    */
    if (LIKELY(shadow_frame.GetDexPC() == 0)) {
        instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
        ArtMethod *method = shadow_frame.GetMethod();
        
        if (UNLIKELY(instrumentation->HasMethodEntryListeners())) {
            instrumentation->MethodEnterEvent(self, shadow_frame.GetThisObject(code_item->ins_size_), method, 0);
        }
        
        //判断这个需要纯解释执行的方法是否经过JIT编译了
        if (!stay_in_interpreter) {
            jit::Jit* jit = Runtime::Current()->GetJit();
            if (jit != nullptr) {
                jit->MethodEntered(self, shadow_frame.GetMethod());
                if (jit->CanInvokeCompiledCode(method)) {
                    ...... //转入jit编译的【机器码去执行】并返回结果
                    JValue result;
                    // Pop the shadow frame before calling into compiled code.
                    self->PopShadowFrame();
                    ArtInterpreterToCompiledCodeBridge(self, nullptr, code_item, &shadow_frame, &result);
                    // Push the shadow frame back as the caller will expect it.
                    self->PushShadowFrame(&shadow_frame);
                    return result;
                }
        } }
    } //dex_pc_是否为0判断结束

    ...... //下面是【解释执行】的处理逻辑
    
    ArtMethod* method = shadow_frame.GetMethod();
    // transaction_active 和dex2oat编译逻辑有关，完整虚拟机运行时候返回false
    bool transaction_active = Runtime::Current()->IsActiveTransaction();
    
    //是否略过Access检查，即判断是否有权限执行本方法。大部分情况下该if条件是满足的
    if (LIKELY(method->SkipAccessChecks())) {
        /*在ART虚拟机中，【解释执行的实现方式】有三种，由 kInterpreterImplKind 取值来控制：
          （1）kMterpImplKind：根据不同CPU平台，采用对应汇编语言编写的，基于goto逻辑的实现。这也是 kInterpreterImplKind 的默认取值。
          （2）kSwitchImplKind：由C++编写，基于switch/case逻辑实现。
          （3）kComputedGotoImplKind：由C++编写，基于goto逻辑实现。根据代码中的注释所述，这种实现的代码不支持使用clang编译器。
           这三种实现的思路大同小异，首选自然是速度更快的汇编处理 kMterpImplKind 模式。
           为了展示一些dex指令的处理逻辑，笔者拟讨论 kSwitchImplKind 模式的相关代码。 
         */
         
        //(1)采用对应汇编语言编写的
        if (kInterpreterImplKind == kMterpImplKind) {
            if (transaction_active) {
                .....
            }
            //针对dex2oat的情况
            else if (UNLIKELY(!Runtime::Current()->IsStarted())) {
                ...... 
            } 
            else {
                ......
                //ExecuteMterpImpl 函数的定义由汇编代码实现
                bool returned = ExecuteMterpImpl(self, code_item, &shadow_frame, &result_register);
                if (returned) {
                    return result_register;
                }
            }
        } 
        //(2)由C++编写，基于switch/case逻辑实现
        else if (kInterpreterImplKind == kSwitchImplKind) {
            if (transaction_active) {......
            } else {
                //kSwitchImplKind 的入口函数。注意，最后一个参数的值为false。
                return ExecuteSwitchImpl<false, false>(self, code_item, shadow_frame, result_register, false);
            }
        } 
        //kInterpreterImplKind取值为 kComputedGotoImplKind 的情况
        //(3)由C++编写，基于goto逻辑实现
        else {　
            if (transaction_active) {
                ......
            }
            else {
                return ExecuteGotoImpl<false, false>(self, code_item, shadow_frame,result_register);
            }
        }
    }
    else {
        ......
    }
}





//【10.2.3.1】　ExecuteSwitchImpl
//结合上文，当前所执行的Java方法为B，其ArtMethod对象为ArtMethod*B。
//[interpreter_switch_impl.cc->ExecuteSwitchImpl]
template<bool do_access_check, bool transaction_active>
JValue ExecuteSwitchImpl(Thread* self, 
                         const DexFile::CodeItem* code_item,
                        ShadowFrame& shadow_frame, 
                        JValue result_register,
                        bool interpret_one_instruction) {
                            
    //注意上文Execute代码中调用ExeucteSwitchImpl时设置的最后一个参数为false，所以此处 interpret_one_instruction 为false。
    
    constexpr bool do_assignability_check = do_access_check;
    ......
    //dex_pc指向要执行的dex指令
    uint32_t dex_pc = shadow_frame.GetDexPC();
    
    const auto* const instrumentation = Runtime::Current()->GetInstrumentation();
    
    //insns代表方法B的dex指令码数组
    const uint16_t* const insns = code_item->insns_;
    const Instruction* inst = Instruction::At(insns + dex_pc);
    uint16_t inst_data;
    
    //方法B对应的ArtMethod对象
    ArtMethod* method = shadow_frame.GetMethod();
    jit::Jit* jit = Runtime::Current()->GetJit();
    
    ......
    
    do { //【遍历】方法B的dex指令码数组，
        dex_pc = inst->GetDexPc(insns);
        shadow_frame.SetDexPC(dex_pc);
        ......
        inst_data = inst->Fetch16(0);
        /*借助switch/case，针对每一种dex指令进行处理。注意，处理每种dex指令前，都有一个 PREAMBLE 宏，
            该宏就是调用 instrumentation 的 DexPcMovedEvent 函数。10.5节将单独介绍和 instrumentation 相关的内容。  
        */
        switch (inst->Opcode(inst_data)) {
            
            case Instruction::NOP: //处理NOP指令
                PREAMBLE();
                //Next_1xx是Instruction类的成员函数，用于跳过本指令的参数，使之指向下一条指令的开头。
                //1xx是dex指令码存储格式的一种。读者可不用管它。
                inst = inst->Next_1xx();
                break;
                
            ...... //其他dex指令码的处理
            
            //invoke-direct指令码的处理
            case Instruction::INVOKE_DIRECT: { 
                PREAMBLE();
                //【*】DoInvoke的分析见下文。
                bool success = DoInvoke<kDirect, false, do_access_check>(self, shadow_frame, inst, inst_data, &result_register);
                /*Next_3xx 也是 Instruction 类的成员函数。下面的 POSSIBLY_HANDLE_PENDING_EXCEPTION是一个宏，
                   如果有异常发生，则进入异常处理，否则将调用 Next_3xx 函数使得inst指向
                  下一条指令。整个解释执行的流程就这样循环直到所有指令码处理完毕。   
                */
                POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
                break;
            }
            
            ......
        }
    } while (!interpret_one_instruction); //循环
    
    //记录dex指令执行的位置并更新到shadow_frame中
    shadow_frame.SetDexPC(inst->GetDexPc(insns));
    return result_register;
}




/*
方法B中有一条invoke指令，用于调用方法C。
现在我们就来考察方法B是如何处理这条invoke指令以调用方法C的。
我们称方法C对应的ArtMethod对象为ArtMethod*C。
*/


//【10.2.3.2】　DoInvoke
//[interpreter_common.h->DoInvoke]
template<InvokeType type, bool is_range, bool do_access_check>
static inline bool DoInvoke(Thread* self, 
                            ShadowFrame& shadow_frame,
                            const Instruction* inst, 
                            uint16_t inst_data, 
                            JValue* result) {
    /*先观察DoInvoke的参数：
     （1）模板参数 type：指明调用类型，比如kStatic、kDirect等。
     （2）模板参数 is_range：如果该方法有多于五个参数的话，则需要使用invoke-xxx-range这样的指令。
     （3）模板参数 do_access_check：是否需要访问检查。即检查是否有权限调用invoke指令的目标方法C。
     
     （4）shadow_frame：方法B对应的ShadowFrame对象。
     （5）inst：invoke指令对应的Instruction对象。
     （6）inst_data：invoke指令对应的参数。
     （7）result：用于存储方法C执行的结果。 
    */
    
    //method_idx为方法C在dex文件里method_ids数组中的索引
    const uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
    
    //找到[方法C]对应的对象。它作为参数存储在方法B的ShawdowFrame对象中。
    const uint32_t vregC = (is_range) ? inst->VRegC_3rc() : inst->VRegC_35c();
    Object* receiver = (type == kStatic) ? nullptr : shadow_frame.GetVRegReference(vregC);
    
    //sf_method 代表 ArtMethod* B。
    ArtMethod* sf_method = shadow_frame.GetMethod();
    
    /*FindMethodFromCode 用于查找代表目标[方法C]对应的ArtMethod对象，即ArtMethod* C。其内
    部会根据do_access_check的情况检查方法B是否有权限调用方法C。
      注意，FindMethodFromCode 函数是根据不同调用类型（kStatic、kDirect、kVirtual、kSuper、kInterface）
      以找到对应的ArtMethod对象的关键代码。这部分内容请读者自行阅读。
    */
    ArtMethod* const called_method = FindMethodFromCode<type, do_access_check>( method_idx, &receiver, sf_method, self);
    
    //假设方法C对应的ArtMethod对象找到了，所以，called_method 不为空。
    if (UNLIKELY(called_method == nullptr)) {
        .......
    }
    else if (UNLIKELY(!called_method->IsInvokable())) {
        ......
    }
    else {
        //下面这段代码和JIT有关，我们留待后续章节再来介绍。
        jit::Jit* jit = Runtime::Current()->GetJit();
        if (jit != nullptr) {
            ......
        }
        
        ......            
        //instrumentation的处理
        return DoCall<is_range, do_access_check>(called_method, self, shadow_frame, inst, inst_data,result);
    }
}




//[interpreter_common.cc->DoCall]
template<bool is_range, bool do_assignability_check>
bool DoCall(ArtMethod* called_method, 
            Thread* self,
            ShadowFrame& shadow_frame,
            const Instruction* inst,
            uint16_t inst_data, 
            JValue* result) {
    const uint16_t number_of_inputs = (is_range) ? inst->VRegA_3rc(inst_data) : inst->VRegA_35c(inst_data);
    
    //kMaxVarArgsRegs为编译常量，值为5
    uint32_t arg[Instruction::kMaxVarArgRegs] = {};
    uint32_t vregC = 0;
    if (is_range) {
        ......
    }
    else {
        vregC = inst->VRegC_35c();
        inst->GetVarArgs(arg, inst_data); //将调用方法C的参数存储到arg数组中
    }
    //调用DoCallCommon，我们接着看这个函数
    return DoCallCommon<is_range, do_assignability_check>( called_method, self, shadow_frame,result, number_of_inputs, arg, vregC);
}



//[interpreter_common.cc->DoCallCommon]
template <bool is_range, bool do_assignability_check, size_t kVarArgMax>
static inline bool DoCallCommon(ArtMethod* called_method,
                                Thread* self, 
                                ShadowFrame& shadow_frame, 
                                JValue* result,
                                uint16_t number_of_inputs,
                                uint32_t (&arg)[kVarArgMax],
                                uint32_t vregC) {
    bool string_init = false;
    
    //和String类的构造函数有关。此处不拟讨论。
    if (UNLIKELY(called_method->GetDeclaringClass()->IsStringClass() && called_method->IsConstructor())) {
        .....
    }

    const DexFile::CodeItem* code_item = called_method->GetCodeItem();
    uint16_t num_regs;
    if (LIKELY(code_item != nullptr)) {
        num_regs = code_item->registers_size_;
    } else {
        num_regs = number_of_inputs;
    }
    
    uint32_t string_init_vreg_this = is_range ? vregC : arg[0];
    if (UNLIKELY(string_init)) {
        ......
    }

    size_t first_dest_reg = num_regs - number_of_inputs;
    
    ......
    
    //创建方法C所需的 ShadowFrame 对象。
    ShadowFrameAllocaUniquePtr shadow_frame_unique_ptr =  CREATE_SHADOW_FRAME(num_regs, &shadow_frame, called_method, 0);
    ShadowFrame* new_shadow_frame = shadow_frame_unique_ptr.get();
    
    if (do_assignability_check) {
        ...... //不考虑这种情况，读者可自行阅读
    } else {
        size_t arg_index = 0;
        if (is_range) {
            ......
        }
        else {
            //从调用方法B的 ShadowFrame 对象中拷贝方法C【所需的参数】到C的ShadowFrame对象里
            for (; arg_index < number_of_inputs; ++arg_index) {
                AssignRegister(new_shadow_frame, shadow_frame, first_dest_reg + arg_index, arg[arg_index]);
            }
        }
        ......
    }
    
    //准备方法C对应的 ShadowFrame 对象后，现在将考虑如何跳转到目标方法C。
    if (LIKELY(Runtime::Current()->IsStarted())) {
        ArtMethod* target = new_shadow_frame->GetMethod();
     
        //如果处于调试模式，或者方法C不存在机器码，则调用 ArtInterpreterToInterpreterBridge 函数，显然，它是解释执行的继续。
        //【ShouldUseInterpreterEntrypoint返回为true，则一定不是JNI方法!】
        if (ClassLinker::ShouldUseInterpreterEntrypoint(target, target->GetEntryPointFromQuickCompiledCode())) {
            ArtInterpreterToInterpreterBridge(self, code_item, new_shadow_frame, result);
        } 
        else {
            //如果可以用机器码方式执行方法C，则调用ArtInterpreterToCompiledCodeBridge，
            //它将从【解释执行模式】进入【机器码执行模式】。
            ArtInterpreterToCompiledCodeBridge(self, shadow_frame.GetMethod(), code_item,new_shadow_frame, result);
        }
    } 
    else { 
        //dex2oat中的处理。因为dex2oat要执行诸如类的初始化方法"<clinit>"，这些方法【都】采用解释执行模式来处理的。
        //内部： ArtInterpreterToInterpreterBridge(self, code_item, shadow_frame, result);
        UnstartedRuntime::Invoke(self, code_item, new_shadow_frame, result, first_dest_reg)
    }
    ......
    return !self->IsExceptionPending();
}


//【解释模式->解释模式】
//ArtInterpreterToInterpreterBridge的代码如下所示。
//[interpreter.cc->ArtInterpreterToInterpreterBridge]
void ArtInterpreterToInterpreterBridge(Thread* self,
                                        const DexFile::CodeItem* code_item,
                                        ShadowFrame* shadow_frame, 
                                        JValue* result) {
    ......
    
    self->PushShadowFrame(shadow_frame); //方法C 对应的ShadowFrame对象入栈
    
    ArtMethod* method = shadow_frame->GetMethod();
    const bool is_static = method->IsStatic();
    
    //如果方法C为静态方法，则判断该方法所属的类是否初始化过了，如果没有，则先初始化这个类。
    if (is_static) {
        mirror::Class* declaring_class = method->GetDeclaringClass();
        if (UNLIKELY(!declaring_class->IsInitialized())) {
            StackHandleScope<1> hs(self);
            HandleWrapper<Class> h_declaring_class(hs.NewHandleWrapper(&declaring_class));
            if (UNLIKELY(!Runtime::Current()->GetClassLinker()->EnsureInitialized(self, h_declaring_class, true, true))) {
                ......
            }
        }
    }
    
    //如果不是JNI方法，则调用Execute执行该方法。Execute函数我们在【10.2.3】节介绍过它了。
    if (LIKELY(!shadow_frame->GetMethod()->IsNative())) {
        result->SetJ(Execute(self, code_item, *shadow_frame, JValue()).GetJ());
    } 
    else {
        ......                  
        /*dex2oat中的处理*/ 
    }
    self->PopShadowFrame();      //方法C对应的ShadowFrame出栈
}


//【解释模式->机器码模式】
//如果方法C存在机器码，则需要从解释执行模式转入机器码执行模式。我们来看其中的关键函数 ArtInterpreterToCompiledCodeBridge。
//[interpreter_common.cc->ArtInterpreterToCompiledCodeBridge]
void ArtInterpreterToCompiledCodeBridge(Thread* self,
                                        ArtMethod* caller, 
                                        const DexFile::CodeItem* code_item,
                                        ShadowFrame* shadow_frame, 
                                        JValue* result) {
    ArtMethod* method = shadow_frame->GetMethod();
    if (method->IsStatic()) {
        //检查方法C所属类是否完成了初始化，如果没有，则先初始化该类。
        ......
    }
    uint16_t arg_offset = (code_item == nullptr) ? 0 : code_item->registers_size_ - code_item->ins_size_;
    jit::Jit* jit = Runtime::Current()->GetJit();
    
    ...... //JIT相关，此处先略过
    
    //调用ArtMethod* C的Invoke函数。直接来看这个函数的代码。
    method->Invoke(self, shadow_frame->GetVRegArgs(arg_offset), 
                        (shadow_frame->NumberOfVRegs() - arg_offset) * sizeof(uint32_t),
                        result,
                        method->GetInterfaceMethodIfProxy(sizeof(void*))->GetShorty()
                    ); 
}




//[art_method.cc->ArtMethod::Invoke]
void ArtMethod::Invoke(Thread* self, 
                       uint32_t* args, 
                       uint32_t args_size,
                       JValue* result,
                       const char* shorty) {
    /* 注意参数
       （1）args：方法C所需的参数。它是一个数组，元素个数为args_size。
       （2）result：存储方法C调用结果的对象。
       （3）shorty：方法C的简短描述。    
    */
    
    //栈操作，详情见下文分析
    ManagedStack fragment;
    self->PushManagedStackFragment(&fragment);//

    Runtime* runtime = Runtime::Current();
    if (UNLIKELY(!runtime->IsStarted() || Dbg::IsForcedInterpreterNeededForCalling(self, this))) {
        ......
    }
    else {
        //再次判断方法C是否存在机器码
        bool have_quick_code = GetEntryPointFromQuickCompiledCode() != nullptr;
        
        if (LIKELY(have_quick_code)) {
            //如果是非静态函数，则调用 art_quick_invoke_stub 函数，
            //否则调用 art_quick_invoke_static_stub 函数。
            //这两个函数也是由汇编代码编写。我们看其中的 art_quick_invoke_stub 函数。
            if (!IsStatic()) {
                (*art_quick_invoke_stub)(this, args, args_size, self, result, shorty);
            } 
            else {
                (*art_quick_invoke_static_stub)(this, args, args_size, self, result, shorty);
            }
            
            //和HDeoptimize有关。详情见下文。
            if (UNLIKELY(self->GetException() == Thread::GetDeoptimizationException())) {
                self->DeoptimizeWithDeoptimizationException(result);
            }
        }
        else {
            LOG(INFO) << "Not invoking '" << PrettyMethod(this) << "' code=null";
            if (result != nullptr) {
              result->SetJ(0);
            }
        }
        ......
    }
    
    self->PopManagedStackFragment(fragment);
    
}


/*
art_quick_invoke_stub 虽然是由汇编代码编写，但其内容相对比较容易简单。从上面展示的代码可知，
在x86平台上，art_quick_invoke_stub 将：
    ·首先准备好栈空间，尤其是将机器码函数所需的参数拷贝到栈上。在相关参数中，EAX寄存器存储着目标方法对应的ArtMethod对象。
    ·【然后通过call指令跳转到该ArtMethod对象的机器码入口】。如此这般，我们就将以机器码方式执行这个方法。
    ·该方法的机器码执行完后将返回到 art_quick_invoke_stub 执行。此时，art_quick_invoke_stub 将把执行结果存储到result位置。
    ·当调用流程从 art_quick_invoke_stub 返回后，解释执行的处理逻辑就得到了方法C机器码执行的结果。
*/