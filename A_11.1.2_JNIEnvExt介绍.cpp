//先来看JNIEnvExt类的声明，代码如下所示。
//[jni_env_ext.h->JNIEnvExt类什么]
struct JNIEnvExt : public JNIEnv {
    static JNIEnvExt* Create(Thread* self, JavaVMExt* vm);
    
    //下面两个函数和JNI层对引用对象的管理有关。详情见下文介绍
    void PushFrame(int capacity);
    void PopFrame();
    
    /*下面三个函数和Local引用对象的管理有关。在JNI层中，Java层传入的jobject参数，以及JNI层
      内部通过NewObject等函数（类似Java层的new操作符）创建的对象都属于Local引用对象。
      从JNI函数返回到Java层后，这些Local引用对象理论上就属于被回收的对象。
      如果JNI层想长久保存（比如在下一次JNI调用或其他JNI函数内使用的话），则需要通过NewGlobalRef或NewGlobalWeakRef
      将其转换成一个Global或WeakGlobal引用对象。
      读者回顾上节的代码可知，JavaVMExt保存了 Global 和 WeakGlobal 的引用对象，而JNIEnvExt管理的是Local引用对象。 
    */
    template<typename T>
    T AddLocalReference(mirror::Object* obj);
    jobject NewLocalRef(mirror::Object* obj);
    void DeleteLocalRef(jobject obj);

    Thread* const self;       //一个JNIEnv对象关联一个线程
    JavaVMExt* const vm;      //全局的JavaVM对象

    //local_ref_cookie和locals用于管理Local引用对象，详情见下文代码分析
    uint32_t local_ref_cookie;
    IndirectReferenceTable locals GUARDED_BY(Locks::mutator_lock_);
    std::vector<uint32_t> stacked_local_ref_cookies;
    ReferenceTable monitors;
    ......

    private:
        ......
        std::vector<std::pair<uintptr_t, jobject>> locked_objects_;
};



//我们先认识JNIEnvExt的构造函数。
//[jni_env_ext.cc->JNIEnvExt::JNIEnvExt]
JNIEnvExt::JNIEnvExt(Thread* self_in, JavaVMExt* vm_in)
        : 
        self(self_in), vm(vm_in),
        /*IRT_FIRST_SEGMENT为常量，值为0。kLocalsInitial取值为64，kLocalsMax取值为512 */
        local_ref_cookie(IRT_FIRST_SEGMENT),
        locals(kLocalsInitial, kLocalsMax, kLocal, false),
        check_jni(false),  runtime_deleted(false),  critical(0),
        monitors("monitors", kMonitorsInitial, kMonitorsMax) {
            
    functions = unchecked_functions = GetJniNativeInterface();
    ......
    
}



//JNI规范中针对JNIEnv定义了两百多个函数，在ART虚拟机中，这些函数对应的实现由 GetJniNativeInterface函数的返回值来指示。
//[jni_internal.cc->GetJniNativeInterface]
const JNINativeInterface* GetJniNativeInterface() {
    return &gJniNativeInterface;
}