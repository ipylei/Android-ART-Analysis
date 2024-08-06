//[runtime.cc->Runtime::Init]
bool Runtime::Init(RuntimeArgumentMap&& runtime_options_in) {
    ......
    //java_vm_是Runtime的成员变量，它指向一个JavaVMExt对象。
    //java_vm_对象就是ART虚拟机中全局唯一的虚拟机代表。通过Runtime GetJavaVM函数可返回这
    //个成员
    java_vm_ = new JavaVMExt(this, runtime_options);
    ......
}


//[thread.cc->Thread::Init]
bool Thread::Init(ThreadList* thread_list, JavaVMExt* java_vm,
                JNIEnvExt* jni_env_ext) {
    ......

    //tlsPtr_是ART虚拟机中每个Thread对象都有的核心成员
    tlsPtr_.pthread_self = pthread_self();
    
    ......
    
    if (jni_env_ext != nullptr) {
        ......
    }
    else {
        //jni_env 是 tlsPtr_ 的成员变量，类型为 JNIEnvExt*，它就是每个Java线程所携带的 JNIEnv 对象。
        tlsPtr_.jni_env = JNIEnvExt::Create(this, java_vm);
        ......
    }
    
    ......
    
    return true;
}




//【11.1.1】　JavaVMExt相关介绍
//先来看JavaVMExt类的声明，代码如下所示。
//[java_vm_ext.h::JavaVMExt声明]
class JavaVMExt : public JavaVM {      //JavaVMExt派生自JavaVM
    public:
        //构造函数
        JavaVMExt(Runtime* runtime, const RuntimeArgumentMap& runtime_options);
        ......
        //Java native的方法往往实现在一个动态库中，下面这个函数用于加载指定的动态库文件
        bool LoadNativeLibrary(JNIEnv* env, const std::string& path, jobject class_loader, jstring library_path, std::string* error_msg);
        //ArtMethod m是一个java native方法，下面这个函数将搜索该方法在native层的实现
        void* FindCodeForNativeMethod(ArtMethod* m);
        
        ......
        
        //下面这几个函数和JNI对 Global 与 WeakGlobal 引用对象的管理有关，详情见下文代码分析
        jobject AddGlobalRef(Thread* self, mirror::Object* obj);
        jweak AddWeakGlobalRef(Thread* self, mirror::Object* obj);
        void DeleteGlobalRef(Thread* self, jobject obj);
        void DeleteWeakGlobalRef(Thread* self, jweak obj));
        ......
    private:
        Runtime* const runtime_;
        ......
        //IndirectReferenceTable 是ART JNI实现中用来管理引用的类。下文将详细介绍它。
        //下面的globals_和weak_globals_分别用于保存Global和Weak Global的引用对象
        IndirectReferenceTable globals_;
        IndirectReferenceTable weak_globals_;
        
        //libraries_：指向一个Libraries对象，该对象用于管理承载jni方法实现的动态库文件
        std::unique_ptr<Libraries> libraries_;
        
        //JavaVM所定义的函数包含在一个JNIInvokeInterface结构体中
        const JNIInvokeInterface* const unchecked_functions_;
        
        ......;
};




//JavaVMExt构造函数的代码如下所示。
//[java_vm_ext.h->JavaVMExt::JavaVMExt]
JavaVMExt::JavaVMExt(Runtime* runtime,
                    const RuntimeArgumentMap& runtime_options)
    : runtime_(runtime),
        ......
        
    /*初始化globals_对应的IndirectReferenceTable（以后简称IRTable）对象。
      从IRTable的命名可以看出，它是一个Table。其构造函数需要如下三个参数：
          gGlobalsInitial：整数，值为512。表示这个IRTable初始能容纳512个元素。
          gGlobalsMax：整数，值为51200，表示这个IRTable最多能容纳51200个元素
          kGlobal是枚举变量，定义为enum IndirectRefKind {
              kHandleScopeOrInvalid = 0,
              kLocal = 1,
              kGlobal = 2, 
              kWeakGlobal = 3 
            }。
      IndirectRefKind 表示间接引用类型，其中kLocal、kGlobal、kWeakGlobal 都是JNI规范中定义的引用类型。
      而 kHandleScopeOrInvalid 和ART在虚拟机中调用java native方法时，引用型参数会通过一个HandleScope来保存有关
      （读者可回顾9.5.3节的内容。下文还会进行详细分析）
    */
    globals_(gGlobalsInitial, gGlobalsMax, kGlobal),
    
    libraries_(new Libraries),
    unchecked_functions_(&gJniInvokeInterface),
    
    //创建用于管理WeakGlobal引用的weak_globals_ IRT对象，其中，kWeakGlobalsInitial值为16，kWeakGlobalMax值为51200
    weak_globals_(kWeakGlobalsInitial, kWeakGlobalsMax, kWeakGlobal),
    ...... 
    {
        ......
}




//由于Java native方法的真实实现是在 Native 层，而Native层的代码逻辑又往往封装在一个动态库文件中，
//所以，JNI的一个重要工作就是加载一个包含了native方法实现的动态库文件。
//在ART虚拟机中，加载的核心函数就是JavaVMExt的 LoadNativeLibrary 函数。
//[java_vm_ext.cc->JavaVMExt::LoadNativeLibrary]
bool JavaVMExt::LoadNativeLibrary(JNIEnv* env,
                                const std::string& path, 
                                jobject class_loader,
                                jstring library_path, 
                                std::string* error_msg) {
    /*注意参数。
      path：代表目标动态库的文件名，不包含路径信息。Java层通过System.loadLibrary加载动态库时，
        只需指定动态库的名称（比如libxxx），不包含路径和后缀名。
      class_loader：根据JNI规范，目标动态库必须和一个ClassLoader对象相关联，同一个目标动态库不能由不同的ClassLoader对象加载。
      library_path：动态库文件搜索路径。我们将在这个路径下搜索path对应的动态库文件。 
    */
    ......

    SharedLibrary* library;
    Thread* self = Thread::Current();
    
    {
        MutexLock mu(self, *Locks::jni_libraries_lock_);
        /*可能会有多个线程触发目标动态库加载，所以这里先同步判断一下path对应的动态库是否已经
          加载。libraries_ 内部包含一个map容器，保存了动态库名和一个动态库（由SharedLibrary类描述）的关系   
        */
        library = libraries_->Get(path);
    }
    
    ...... //如果library不为空，则需要检查加载它的 ClassLoader 对象和传入的class_loader是否为同一个

    Locks::mutator_lock_->AssertNotHeld(self);
    const char* path_str = path.empty() ? nullptr : path.c_str();
    
    /*加载动态库，Linux平台上就是使用dlopen方式加载。但Android系统做了相关定制，主要是出于
      安全方面的考虑。比如，一个应用不能加载另外一个应用携带的动态库。下面这个函数请读者自行阅读。
      总之，OpenNativeLibrary 成功返回后，handle 代表目标动态库的句柄。
    */
    //【*】 Android 4.4 中这里是 dlopen
    void* handle = android::OpenNativeLibrary(env, runtime_->GetTargetSdkVersion(), path_str, class_loader, library_path);

    ......
    bool created_library = false;
    {
        //构造一个SharedLibrary 对象，并将其保存到 libraries_ 内部的map容器中
        std::unique_ptr<SharedLibrary> new_library(new SharedLibrary(env, self, path, handle, class_loader, class_loader_allocator));
        MutexLock mu(self, *Locks::jni_libraries_lock_);
        library = libraries_->Get(path);
        if (library == nullptr) {
            library = new_library.release();
            libraries_->Put(path, library);
            created_library = true;
        }
    }
    ......

    bool was_successful = false;
    void* sym;
    ......
    /*找到动态库中的 JNI_OnLoad 函数。如果有该函数，则需要执行它。
      一般而言，动态库会在JNI_OnLoad 函数中将【Java native方法】与【Native层】对应的实现函数绑定。
      绑定是利用JNIEnv RegisterNativeMethods 来完成的，下文将介绍它。 
      【JNIEnv RegisterNativeMethods 亦可参考 8.2】 
    */
    sym = library->FindSymbol("JNI_OnLoad", nullptr);
    if (sym == nullptr) {
        was_successful = true;
    } else {
        ......
        typedef int (*JNI_OnLoadFn)(JavaVM*, void*);
        JNI_OnLoadFn jni_on_load = reinterpret_cast<JNI_OnLoadFn>(sym);
        //执行JNI_OnLoad函数。该函数的返回值需要处理
        int version = (*jni_on_load)(this, nullptr);
        
        ......
        
        //version取值为JNI_ERR，表示JNI_Onload处理失败
        if (version == JNI_ERR) {
            ...... 
        }
        else if (IsBadJniVersion(version)) {
            //version取值必须为JNI_VERSION_1_2、JNI_VERSION_1_4和JNI_VERSION_1_6中的一个
            ......
        } else { 
            was_successful = true; 
        }
    }
    
    library->SetResult(was_successful);
    return was_successful; //返回该动态库是否加载成功
}
/*
·首先是动态库本身加载到虚拟机进程，这是借助操作系统提供的功能来实现的。如果这一步操作成功，该动态库对应的信息将保存到libraries_中。
·如果动态库定义了JNI_OnLoad函数，则需要执行这个函数。
    该函数执行成功，则整个动态库加载成功。
    如果执行失败，则认为动态库加载失败。注意，无论该步骤执行成功与否，动态库都已经在第一步中加载到虚拟机内存了，
        并不会因为JNI_OnLoad执行失败而卸载这个动态库。
*/




//FindCodeForNativeMethod
//如何将一个【Java native方法】与【动态中的方法】关联起来呢？
//有一种简单的方法就是根据Java native方法的签名信息（由所属类的全路径、返回值类型、参数类型等共同组成）来搜索动态库。来看代码。

//[java_vm_ext.cc->JavaVMExt::FindCodeForNativeMethod]
void* JavaVMExt::FindCodeForNativeMethod(ArtMethod* m) {  //注意：参数m是Java层的native函数
    mirror::Class* c = m->GetDeclaringClass();
    std::string detail;
    void* native_method;
    Thread* self = Thread::Current();
    {
        MutexLock mu(self, *Locks::jni_libraries_lock_);
        //内部将遍历 libraries_ 的map容器，找到符合条件的【Native函数】，返回值 native_method 实际上是一个函数指针对象
        native_method = libraries_->FindNativeMethod(m, detail);
    }
    if (native_method == nullptr) {
        self->ThrowNewException("Ljava/lang/UnsatisfiedLinkError;", detail.c_str());
    }
    return native_method;
}



//[java_vm_ext.cc->Libraris::FindNativeMethod]
void* FindNativeMethod(ArtMethod* m, std::string& detail) {
    
    /*
    m代表一个Java的native方法，JniShortName 和 JniLongName 将根据这个方法的信息得到 【Native函数】的函数名。下文将给出一个示例。
    */
    std::string jni_short_name(JniShortName(m));
    std::string jni_long_name(JniLongName(m));
    ......
    ScopedObjectAccessUnchecked soa(Thread::Current());
    
    //libraries_ 是 Libraries 类的map容器，保存已经加载的动态库信息。下面将遍历
    //这个容器，然后调用 FindSymbol（Linux平台上，其内部使用 dlsym）来搜索目标函数
    for (const auto& lib : libraries_) {
        SharedLibrary* const library = lib.second;
        ......
        const char* shorty = library->NeedsNativeBridge()? m->GetShorty() : nullptr;
        void* fn = library->FindSymbol(jni_short_name, shorty);
        if (fn == nullptr) {
            fn = library->FindSymbol(jni_long_name, shorty);
        }
        if (fn != nullptr) {
            return fn;
        }
    }
    ......
    return nullptr;
}


/*
假设有一个Java native方法，其Java层定义如下。
package pkg;
class Cls {
    native double f(int i, String s);
}

那么，JniShortName和JniLongName返回方法f 在 Native层对应实现函数的函数名。
    ·Java_pkg_Cls_f：短命名规则下函数名中不包含f的参数信息。
    ·Java_pkg_Cls_f__ILjava_lang_String_2：长命名规则下函数名将包含f的参数信息。
    
    
    
通过上述代码可知，通过 FindCodeForNativeMethod 来找到一个 java native 方法对应的 Native 函数会比较慢。
因为它要遍历和搜索虚拟机所加载的所有和JNI有关的动态库。
*/