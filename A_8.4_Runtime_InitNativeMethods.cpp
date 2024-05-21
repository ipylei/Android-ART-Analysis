//本节介绍Runtime的 InitNativeMethods。代码如下所示。
//[runtime.cc->Runtime::InitNativeMethods]
void Runtime::InitNativeMethods() {
    Thread* self = Thread::Current();
    JNIEnv* env = self->GetJniEnv();
    
    //很多个初始化函数，其实就是缓存一些信息以及注册一些native函数
    JniConstants::init(env);   //8.4.1
    RegisterRuntimeNativeMethods(env);  //8.4.2
    WellKnownClasses::Init(env);  //8.4.3
    
    {
        std::string error_msg;
        //加载libjavacore.so，LoadNativeLibrary函数我们后续章节再来介绍
        if (!java_vm_->LoadNativeLibrary(env, "libjavacore.so", nullptr, nullptr, &error_msg)) {......}
    }
    
    {
        constexpr const char* kOpenJdkLibrary = kIsDebugBuild? "libopenjdkd.so" : "libopenjdk.so";
        std::string error_msg;
        if (!java_vm_->LoadNativeLibrary(env, kOpenJdkLibrary, nullptr, nullptr, &error_msg)) {......}
    }
    WellKnownClasses::LateInit(env);  //8.4.3
}




//【8.4.1】
//JniConstants Init将缓存一些基本的Java类信息，来看代码。
//[JniConstants.cpp->JniConstants::Init]
void JniConstants::init(JNIEnv* env) {
    if (g_constants_initialized) { return; }
    std::lock_guard<std::mutex> guard(g_constants_mutex);
    
    ......
        
    //findClass见下文介绍，不过读者也可以猜测其实现，应该是先通过JNIEnv的FindClass找到对应的jclass对象，
    //然后再将其转换成一个全局引用对象。下面的代码一共保存了55个类的信息
      bigDecimalClass = findClass(env, "java/math/BigDecimal");
      booleanClass = findClass(env, "java/lang/Boolean");
      byteClass = findClass(env, "java/lang/Byte");
      byteArrayClass = findClass(env, "[B");
    ......
    g_constants_initialized = true;
}

//简单看一下findClass的实现。
//[JniConstants.cpp->JniConstants::findClass]
static jclass findClass(JNIEnv* env, const char* name) {
    ScopedLocalRef<jclass> localClass(env, env->FindClass(name));
    jclass result = reinterpret_cast<jclass>(env->NewGlobalRef(localClass.get()));
    ......
    return result;
}






//【8.4.2】
//RegisterRuntimeNativeMethods将把一些系统Java类里的native函数关联到JNI层中对应的函数指针。
//[runtime.cc->Runtime::RegisterRuntimeNativeMethods]
void Runtime::RegisterRuntimeNativeMethods(JNIEnv* env) {
    //注册一些Java类的native函数，内部就是调用JNIEnv的 RegisterNativeMethods 函数进行处理。
    //下面的注册函数一共涉及27个类。
    register_dalvik_system_DexFile(env);
    ....
}


//我们来其中一个类对应的JNI注册函数，代码如下所示。
//[dalvik_system_VMStack.cc->register_dalvik_system_VMStack]
static JNINativeMethod gMethods[] = {
    //native函数签名信息前使用了"!"，表示调用的时候将使用fast jni模式
    NATIVE_METHOD(VMStack, fillStackTraceElements, "!(Ljava/lang/Thread;[Ljava/lang/StackTraceElement;)I"),
    NATIVE_METHOD(VMStack, getCallingClassLoader, "!()Ljava/lang/ClassLoader;"),
    NATIVE_METHOD(VMStack, getClosestUserClassLoader, "!()Ljava/lang/ClassLoader;"),
    .....
};

//注册函数是register_dalvik_system_VMStack
void register_dalvik_system_VMStack(JNIEnv* env) {
    //下面的REGISTER_NATIVE_METHODS是一个宏
    REGISTER_NATIVE_METHODS("dalvik/system/VMStack");
}

/*这个宏定义在jni_internal.h中，注意它的gMethods。
    在每个需要注册的文件里，比如上面的dalvik_system_VMStack.cc中将定义这个gMethods变量  
  */
#define REGISTER_NATIVE_METHODS(jni_class_name) RegisterNativeMethods(env, jni_class_name, gMethods, arraysize(gMethods))



//【8.4.3】　WellKnownClasses Init和LastInit
//[well_known_classes.cc->WellKnownClasses::Init]
void WellKnownClasses::Init(JNIEnv* env) {
    //CacheClass函数内容与 JniConstants 的findClass几乎一样。下面代码一共缓存了41个知名类（Well Known Class之意）
    com_android_dex_Dex = CacheClass(env, "com/android/dex/Dex");
    
    ......
    
    //缓存一些Java方法，返回值的类型是jmethodId，其实就是指向一个ArtMethod对象，一共50个左右
    dalvik_system_VMRuntime_runFinalization = CacheMethod(env, 
                                        dalvik_system_VMRuntime, true, "runFinalization", "(J)V");
    
    ......    
    
    //缓存一些成员变量的信息
    dalvik_system_DexFile_cookie = CacheField(env, dalvik_system_DexFile, false, "mCookie", "Ljava/lang/Object;");
    
    ......
    
    //缓存对应类的ValueOf函数
    java_lang_Boolean_valueOf = CachePrimitiveBoxingMethod(env, 'Z', "java/lang/Boolean");
    
    ......      
    
    //缓存Java String类中的某些函数，请读者自行阅读。
    Thread::Current()->InitStringEntryPoints();
}


//[well_known_classes.cc->WellKnownClasses::LateInit]
void WellKnownClasses::LateInit(JNIEnv* env) {
    ScopedLocalRef<jclass> java_lang_Runtime(env, env->FindClass("java/lang/Runtime"));
    
    //缓存java.lang.Runtime类的nativeLoad成员函数
    java_lang_Runtime_nativeLoad = CacheMethod(env, 
                    java_lang_Runtime.get(), true, "nativeLoad", "(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;)Ljava/lang/String;");
}