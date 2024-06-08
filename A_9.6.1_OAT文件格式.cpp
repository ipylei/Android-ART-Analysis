//【9.6.1　OAT文件格式】


//图9-37中OatHeader中有七个代表trampoline code偏移量位置的成员。它们在OatWriter InitOatCode函数中被设置，代码如下所示。
//[oat_writer.cc->OatWriter::InitOatCode]
//[oat_writer.cc->OatWriter::InitOatCode]
size_t OatWriter::InitOatCode(size_t offset) {
    size_t old_offset = offset;
    size_t adjusted_offset = offset;
    offset = RoundUp(offset, kPageSize);
    oat_header_->SetExecutableOffset(offset);
    size_executable_offset_alignment_ = offset - old_offset;
    
    //只有boot oat文件中的OatHeader才会真正创建trampoline函数
    if (compiler_driver_->IsBootImage()) {
        InstructionSet instruction_set = compiler_driver_->GetInstructionSet();
        /*DO_TRAMPOLINE是一个宏，它先：
        （1）调用CompilerDriver CreateXXX函数生成trampoline的机器码
        （2）将机器码的位置存储在OatHeader对应的trampoline 偏移量成员中。*/
        #define DO_TRAMPOLINE(field, fn_name) \
            offset = CompiledCode::AlignCode(offset, instruction_set); \
            adjusted_offset = offset + CompiledCode::CodeDelta(instruction_set); \
            oat_header_->Set ## fn_name ## Offset(adjusted_offset); \
            field = compiler_driver_->Create ## fn_name(); \
            offset += field->size();
            
        //注意，这里只设置了五个trampoline code偏移量。宏的第一个参数为OatHeader中对应的成员名
        DO_TRAMPOLINE(jni_dlsym_lookup_, JniDlsymLookup);
        DO_TRAMPOLINE(quick_generic_jni_trampoline_, QuickGenericJniTrampoline);
        DO_TRAMPOLINE(quick_imt_conflict_trampoline_, QuickImtConflictTrampoline);
        DO_TRAMPOLINE(quick_resolution_trampoline_, QuickResolutionTrampoline);
        DO_TRAMPOLINE(quick_to_interpreter_bridge_, QuickToInterpreterBridge);
        #undef DO_TRAMPOLINE
    } else { //对非boot镜像而言，它们的OatHeader中trampoline code偏移量均设置为0
        oat_header_->SetInterpreterToInterpreterBridgeOffset(0);
        ......
        oat_header_->SetQuickToInterpreterBridgeOffset(0);
    }
    return offset;
}



//蹦床

/*
通过上述代码可知，只有boot oat文件才会设置trampoline code区域。值得注意的是，boot oat只设置了其中的五个成员变量，
没有被设置的两个成员变量是interpreter_to_interpreter_bridge_offset_和interpreter_to_compiled_code_bridge_offset_，
这两个成员变量的值均为0。

那么，什么是trampoline code呢？它就是一段跳转到指定函数的代码。所以，对于 trampoline code而言，我们应关注跳转的目标。
下面以 CreateJniDlsymLookup 函数为例，来看对应的trampoline code都是什么。
*/
//[compiler_driver.cc->CREATE_TRAMPOLINE]
std::unique_ptr<const std::vector<uint8_t>> CompilerDriver::CreateJniDlsymLookup() const {
    //调用CREATE_TRAMPOLINE宏
    CREATE_TRAMPOLINE(JNI, kJniAbi, pDlsymLookup)
}

/*CREATE_TRAMPOLINE用于生成跳转到Thread tlsPtr_.quick_entrypoints 或 jni_entrypoints
  里对应函数的机器码。简单点说，trampoline code的跳转目标实际上就是 quick_entrypoints 和 jni_entrypoints里相关的函数。
  宏的第一个参数type取值为JNI或QUICK。JNI表示跳转到jni_entrypoints里的函数。QUICK表示跳转到quick_entrypoints里的函数。  
*/
#define CREATE_TRAMPOLINE(type, abi, offset)     
        if (Is64BitInstructionSet(instruction_set_)) { 
            ......
        } else { 
            //不同平台有对应的实现。
            return CreateTrampoline32(instruction_set_, abi, type ## _ENTRYPOINT_OFFSET(4, offset)); 
        }
    ......
}



//在x86平台上，trampoline code其实非常简单，来看它生成的机器码。[trampoline_compiler.cc->CreateTrampoline]
static std::unique_ptr<const std::vector<uint8_t>> CreateTrampoline(ArenaAllocator* arena, ThreadOffset<4> offset) {
    X86Assembler assembler(arena);
    //生成一条jump到Thread对象指定偏移位置（也就是目标函数）的指令
    __ fs()->jmp(Address::Absolute(offset));
    __ int3();//生成一条int3中断指令
    __ FinalizeCode();
    size_t cs = __ CodeSize();
    
    ......
    
    __ FinalizeInstructions(code);
    return std::move(entry_stub);
}



/* 蹦床
笔者总结了OatHeader中trampoline code五个偏移量和Thread tlsPtr_ 中 jni_entrypoints_ 或 quick_entrypoints_ 相关成员的对应关系。

interpreter_to_interpreter_bridge_offset_ 和 interpreter_to_compiled_code_bridge_offset_ ，这两个成员变量的值均为0。

·OatHeader jni_dlsym_lookup_offset_ 跳转到 jni_entrypoints_ 里 pDlsymLookup 所指向的函数。
·OatHeader quick_generic_jni_trampoline_offset_ 跳转到 quick_entrypoints_ 里 pQuickGenericJniTrampoline 所指向的函数。
·OatHeader quick_imt_conflict_trampoline_offset_ 跳转到 quick_entrypoints_ 里 pQuickImtConflictTrampoline 所指向的函数。
·OatHeader quick_resolution_trampoline_offset_ 跳转到 quick_entrypoints_ 里 pQuickResolutionTrampoline 所指向的函数。
·OatHeader quick_to_interpreter_bridge_offset_ 跳转到 quick_entrypoints_ 里 pQuickToInterpreterBridge_ 所指向的函数。
*/