//【9.5.3.2 JniCompile第一部分】
//[optimizing_compiler.cc->JniCompile]
CompiledMethod* JniCompile(uint32_t access_flags,
                           uint32_t method_idx, 
                           const DexFile& dex_file) const OVERRIDE {
    //内部调用 ArtJniCompileMethodInternal，我们直接来看它
    return ArtQuickJniCompileMethod(GetCompilerDriver(), access_flags, method_idx, dex_file);
}

CompiledMethod* ArtQuickJniCompileMethod(CompilerDriver* compiler, 
                                         uint32_t access_flags, 
                                         uint32_t method_idx, 
                                         const DexFile& dex_file) {               
    //核心功能由 ArtJniCompileMethodInternal 完成，下文将详细分析其代码
    return ArtJniCompileMethodInternal(compiler, access_flags, method_idx, dex_file);
}



//【9.5.3.3 JniCompile第二部分】

//【9.5.3.4 JniCompile第三部分】

//【9.5.3.5 JniCompile第四部分】

//【9.5.3.6 JniCompile第五部分】
//[jni_compiler.cc->ArtJniCompileMethodInternal]
{
    ......
    
    for (uint32_t i = 0; i < args_count; ++i) {
            ......//
        //从mr_conv对应的栈空间中拷贝参数到main_jni_conv对应栈空间中去
        CopyParameter(jni_asm.get(), mr_conv.get(), main_jni_conv.get(), frame_size, main_out_arg_size);
    }
    
    if (is_static) { //拷贝jclass对应的参数到栈空间
        mr_conv->ResetIterator(FrameOffset(frame_size + main_out_arg_size));
        main_jni_conv->ResetIterator(FrameOffset(main_out_arg_size));
        main_jni_conv->Next();  // Skip JNIEnv*
        FrameOffset handle_scope_offset = main_jni_conv->CurrentParamHandleScopeEntryOffset();
        if (main_jni_conv->IsCurrentParamOnStack()) {
            FrameOffset out_off = main_jni_conv->CurrentParamStackOffset();
            __ CreateHandleScopeEntry(out_off, handle_scope_offset, mr_conv->InterproceduralScratchRegister(), false);
        } 
        else {
            ......
        }
    }

    // 拷贝JNIEnv对象的地址到栈空间指定位置
    main_jni_conv->ResetIterator(FrameOffset(main_out_arg_size));
    if (main_jni_conv->IsCurrentParamInRegister()) {
        ......
    }
    else {
        FrameOffset jni_env = main_jni_conv->CurrentParamStackOffset();
        if (is_64_bit_target) {
            .....
        }
        else {
            __ CopyRawPtrFromThread32(jni_env, Thread::JniEnvOffset<4>(), main_jni_conv->InterproceduralScratchRegister());
        }
    }
    
    //jni对应的native函数的地址保存在ArtMethod对象 ptr_sized_fields_ . entry_point_from_jni_ 成员变量中。
    MemberOffset jni_entrypoint_offset = ArtMethod::EntryPointFromJniOffset(InstructionSetPointerSize(instruction_set));
    __ Call(main_jni_conv->MethodStackOffset(), jni_entrypoint_offset, mr_conv->InterproceduralScratchRegister());
    
    //保存native函数的返回值到栈上
    FrameOffset return_save_location = main_jni_conv->ReturnValueSaveLocation();
    if (main_jni_conv->SizeOfReturnValue() != 0 && !reference_return) {
        ......
        __ Store(return_save_location, main_jni_conv->ReturnRegister(), main_jni_conv->SizeOfReturnValue());
    }
}


//【9.5.3.7 JniCompile第六部分】