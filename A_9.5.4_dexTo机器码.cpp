//[optimizing_compiler.cc->OptimizingCompiler::Compile]
CompiledMethod* OptimizingCompiler::Compile(const DexFile::CodeItem* code_item,.....) const {
    CompilerDriver* compiler_driver = GetCompilerDriver();
    //代表Java方法编译结果的CompiledMethod对象
    CompiledMethod* method = nullptr;
    ....
    //method_idx为待编译java方法在dex_file中method_ids数组中的索引。
    //假设该方法通过了前面的校验（参考9.5.1.2.1节）。
    if (compiler_driver->IsMethodVerifiedWithoutFailures(method_idx, class_def_idx, dex_file) || ......) {
        ArenaAllocator arena(Runtime::Current()->GetArenaPool());
        CodeVectorAllocator code_allocator(&arena);
        /*TryCompile的内容大部分在第6章中见过，包含构造CFG、执行优化任务（RunOptimizations）、
          编译ART IR、分配寄存器等  */
        std::unique_ptr<CodeGenerator> codegen(
            TryCompile(&arena,&code_allocator,code_item,.... false));
        if (codegen.get() != nullptr) {
            //创建 CompiledMethod 对象
            method = Emit(&arena, &code_allocator, codegen.get(), compiler_driver, code_item);
        }
        .....
    } 
    .....
    return method;
}