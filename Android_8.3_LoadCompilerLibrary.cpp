//首先来看Jit动态库的加载，非常简单。
//[jit.cc->Jit::LoadCompilerLibrary]
bool Jit::LoadCompilerLibrary(std::string* error_msg) {
    jit_library_handle_ = dlopen(kIsDebugBuild ? "libartd-compiler.so" :"libart-compiler.so", RTLD_NOW);
    
    .....
    
    jit_load_ = reinterpret_cast<void* (*)(bool*)>(dlsym(jit_library_handle_,"jit_load"));
    jit_unload_ = reinterpret_cast<void (*)(void*)>(dlsym(jit_library_handle_,"jit_unload"));
    jit_compile_method_ = reinterpret_cast<bool (*)(void*, ArtMethod*, Thread*, bool)>(
                                dlsym(jit_library_handle_, "jit_compile_method"));
    jit_types_loaded_ = reinterpret_cast<void (*)(void*, mirror::Class**,size_t)>(
                                dlsym(jit_library_handle_, "jit_types_loaded"));

    return true;
}
//Jit LoadCompilerLibrary将加载libart-compiler.so，
//然后保存其中几个关键函数的函数指针。这些函数的作用以后我们再介绍。