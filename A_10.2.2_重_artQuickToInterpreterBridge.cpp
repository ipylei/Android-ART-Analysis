//【10.2.2】　artQuickToInterpreterBridge
//注意　请读者注意，本节先关注解释执行的整体执行流程，其中涉及栈管理、HDeoptimize的相关知识将留待后续部分再做详细介绍。
//[quick_trampoline_entrypoints.cc->artQuickToInterpreterBridge]
extern "C" uint64_t artQuickToInterpreterBridge(ArtMethod* method,
                                                Thread* self, 
                                                ArtMethod** sp) {
    //参数 method 代表当前被调用的Java方法，我们用图10-7中的ArtMethod* B 表示它
    ScopedQuickEntrypointChecks sqec(self);
    JValue tmp_value;
    
    /*PopStackedShadowFrame 和 Thread 对栈的管理有关。此处假设是从【机器码】跳转到【解释执行】模式，
      并且不是HDeoptimize的情况，那么，该函数返回值deopt_frame为nullptr。 
    */
    ShadowFrame* deopt_frame = self->PopStackedShadowFrame(StackedShadowFrameType::kSingleFrameDeoptimizationShadowFrame, false);

    ManagedStack fragment; //重要：构造一个 ManagedStack 对象。
    uint32_t shorty_len = 0;
    
    //如果不是代理方法的话，non_proxy_method 就是ArtMethod* B本身。
    ArtMethod* non_proxy_method = method->GetInterfaceMethodIfProxy(sizeof(void*));
    const DexFile::CodeItem* code_item = non_proxy_method->GetCodeItem();
    const char* shorty = non_proxy_method->GetShorty(&shorty_len);

    JValue result; //存储方法调用的返回值
    if (deopt_frame != nullptr) {
        ..... //和HDeoptimize有关，后续章节再介绍它
    } 
    else {
        const char* old_cause = ......;
        uint16_t num_regs = code_item->registers_size_;
        
        //创建代表ArtMethod B的栈帧对象ShawFrame。注意，它的 link_ 取值为 nullptr，dex_pc_ 取值为0
        ShadowFrameAllocaUniquePtr shadow_frame_unique_ptr = CREATE_SHADOW_FRAME(num_regs, /* link */ nullptr, method, /* dex_pc_ */ 0);
        ShadowFrame* shadow_frame = shadow_frame_unique_ptr.get();
        size_t first_arg_reg = code_item->registers_size_ - code_item->ins_size_;
        
        //借助 BuildQuickShadowFrameVisitor 将调用参数放到 shadow_frame 对象中
        BuildQuickShadowFrameVisitor shadow_frame_builder(sp, method->IsStatic(), shorty, shorty_len, shadow_frame, first_arg_reg);
        shadow_frame_builder.VisitArguments();
        
        //判断ArtMethod* B【所属的类】是否已经初始化
        const bool needs_initialization = method->IsStatic() && !method->GetDeclaringClass()->IsInitialized();
        
        //重要：下面两行代码将 fragment 和 shadow_frame 放到 Thread 类对应的成员变量中去处理
        //我们后续再讨论这部分内容
        self->PushManagedStackFragment(&fragment);
        self->PushShadowFrame(shadow_frame);
        
        ......
        
        //如果ArtMethod B所属类没有初始化，则先初始化它。类初始化就是调用 ClassLinker 的 EnsureInitialized 函数
        if (needs_initialization) {
            StackHandleScope<1> hs(self);
            Handle<mirror::Class> h_class(hs.NewHandle(shadow_frame->GetMethod()->GetDeclaringClass()));
            if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(self, h_class, true, true)) {
                ......
            }
        }
        
        //【*】解释执行的入口函数
        result = interpreter::EnterInterpreterFromEntryPoint(self, code_item, shadow_frame);
    }
        
        
    //和Thread对栈的管理有关
    self->PopManagedStackFragment(fragment);
    
    //根据sp的位置找到本方法的【调用者】，以图10-7为例，即找到ArtMethod* A，是它调用了本方法（对应为ArtMethod* B）。
    ArtMethod* caller = QuickArgumentVisitor::GetCallingMethod(sp);
    if (UNLIKELY(Dbg::IsForcedInterpreterNeededForUpcall(self, caller))) {
        //和HDeoptimize有关
        self->PushDeoptimizationContext(result, shorty[0] == 'L', /* from_code */ false, self->GetException());
        self->SetException(Thread::GetDeoptimizationException());
    }
    return result.GetJ();      //artQuickToInterpreterBridge 返回

}




