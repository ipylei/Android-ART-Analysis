//[jni.h->JNIEnv声明]
typedef _JNIEnv JNIEnv; //JNIEnv是_JNIEnv类的别名

//_JNIENv类的声明
struct _JNIEnv {
    const struct JNINativeInterface* functions;
#if defined(__cplusplus) //JNIEnv提供了非常多的功能函数
    jint GetVersion()
    { return functions->GetVersion(this); }//调用functions类的对应函数
    jclass DefineClass(const char *name, jobject loader, const jbyte* buf, jsize bufLen)
    { return functions->DefineClass(this, name, loader, buf, bufLen); }
    jclass FindClass(const char* name)
    { return functions->FindClass(this, name); }
    ......//非常多的函数，实现方法和上面的类似，都是调用functions中同名的函数
}
//JNINativeInterface和上节中提到的JNIInvokeInterface有些类似，都是包含了很多函数指针的结构体。.



//[jni.h->JNINativeInterface]
struct JNINativeInterface {
    void*       reserved0;  void*       reserved1;
    void*       reserved2;  void*       reserved3;
    jint        (*GetVersion)(JNIEnv *);
    jclass      (*DefineClass)(JNIEnv*, const char*, jobject, const jbyte*,  jsize);
    jclass      (*FindClass)(JNIEnv*, const char*);
    jmethodID   (*FromReflectedMethod)(JNIEnv*, jobject);
    jfieldID    (*FromReflectedField)(JNIEnv*, jobject);
    jobject     (*ToReflectedMethod)(JNIEnv*, jclass, jmethodID, jboolean);
    ......
}



//现在来看JNIEnvExt，它是JNIEnv的派生类。其创建是通过Create函数来完成的。
//jni_env_ext.cc->JNIEnvExt::Create
JNIEnvExt* JNIEnvExt::Create(Thread* self_in, JavaVMExt* vm_in) {
    std::unique_ptr<JNIEnvExt> ret(new JNIEnvExt(self_in, vm_in));
    if (CheckLocalsValid(ret.get())) {//检查该JNIEnvExt对象，细节以后再讨论
        return ret.release();
    }
    return nullptr;
}



//构造函数
//jni_env_ext.cc->JNIEnvExt::JNIEnvExt
JNIEnvExt::JNIEnvExt(Thread* self_in, JavaVMExt* vm_in)
        : self(self_in),vm(vm_in),local_ref_cookie(IRT_FIRST_SEGMENT), 
        locals(kLocals Initial, kLocalsMax, kLocal, false), 
        check_jni(false), 
        runtime_deleted(false),
        critical(0), 
        monitors("monitors", kMonitorsInitial, kMonitorsMax) {
            
    /*functions 成员变量是JNIEnv类的成员变量，
    unchecked_functions 是JNIEnvExt的成员这部分代码和JavaVMExt非常类似。
    GetJniNativeInterface 返回全局静态对象 gJniNativeInterface*/

    functions = unchecked_functions = GetJniNativeInterface();
    //如果启用checkJni的话，将设置functions的内容为全局静态对象 gCheckNativeInterface
    if (vm->IsCheckJniEnabled()) {SetCheckJniEnabled(true); }
}


//jni_internal.cc->gJniNativeInterface
const JNINativeInterface gJniNativeInterface = {
    nullptr,  nullptr,  nullptr, nullptr,
    //下面这些函数为JNI类的静态成员变量，也定义在jni_internal.cc中
    JNI::GetVersion,
    JNI::DefineClass,
    JNI::FindClass,
    .....
}