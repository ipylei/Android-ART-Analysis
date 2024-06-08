//[scoped_thread_state_change.h->ScopedObjectAccessAlreadyRunnable]
class ScopedObjectAccessAlreadyRunnable : public ValueObject {
    protected://为方便读者理解，此处代码位置有所移动并去掉线程安全检查方面的代码
        Thread* const self_;
        JNIEnvExt* const env_;
        JavaVMExt* const vm_;
        
        //构造函数1，入参为JNIEnv
        explicit ScopedObjectAccessAlreadyRunnable(JNIEnv* env): self_(ThreadForEnv(env)), 
                                                                 env_(down_cast<JNIEnvExt*>(env)), 
                                                                 vm_(env_->vm) {      
                /*ThreadForEnv函数先将env向下转换（down_cast）为一个JNIEnvExt对象，
                然后取其中的self成员，它代表该自己所绑定的线程对象*/  
        }
                
        explicit ScopedObjectAccessAlreadyRunnable(Thread* self): self_(self), 
                                                                  env_(down_cast<JNIEnvExt*>(self->GetJniEnv())),
                                                                  vm_(env_ != nullptr ? env_->vm : nullptr) {  
        }
                
        explicit ScopedObjectAccessAlreadyRunnable(JavaVM* vm): self_(nullptr), 
                                                                env_(nullptr), 
                                                                vm_(down_cast<JavaVMExt*>(vm)) {
        }




    public:
        //下面这三个函数用于返回对应的成员变量
        Thread* Self() const {  return self_;  }
        JNIEnvExt* Env() const { return env_;}
        JavaVMExt* Vm() const { return vm_; }
        ......
        
        /*下面四个函数明确回答了jfieldID和jmethodID在ART虚拟机中到底是什么的问题。
        其中，DecodeField 和 DecodeMethod 用于将输入的 jfieldID 和 jmethodID 还原成它们本来的面目。*/
        ArtField* DecodeField(jfieldID fid) const {
            // mutator_lock_ 是一个全局的线程同步锁对象。下面第一行代码是检查调用线程（由Self()
            //函数返回）是否持有同步锁。该函数只在调试同步锁时才生效，读者可忽略它
              Locks::mutator_lock_->AssertSharedHeld(Self());
            //原来，jfieldID就是ArtField*
            return reinterpret_cast<ArtField*>(fid);
        }
        ArtMethod* DecodeMethod(jmethodID mid) const {
            //原来，jmethodID就是ArtMethod*
            return reinterpret_cast<ArtMethod*>(mid);
        }
        
        //EncodeField和EncodeMethod用于将ArtField和ArtMethod对象的地址值转换为对应的jfieldID和jmethodID值。
        jfieldID EncodeField(ArtField* field) const {
            return reinterpret_cast<jfieldID>(field);
        }
        jmethodID EncodeMethod(ArtMethod* method) const {
            return reinterpret_cast<jmethodID>(method);
        }
        
        
        
        /*Decode：用于将jobject的值转换成对应的类型。Decode是一个模板函数，
            返回值的类型板为模参数T。使用时传的是mirror命名空间中的各种数据类型（读者可参考第7章的图7-20）*/
        template<typename T>  
        T Decode(jobject obj) const {
            /*注意下面这行代码，DecodeJObject 是Thread中的函数，
            其原型为mirror::Object*Thread::DecodeJObject(jobject obj)。
            从这可以很明显看出，jobject将被转换成mirror::Object。
            至于mirror Object*到底是什么，则会向下转换成类型T。
            总之，jobject在JNI层代表一个Java Object，
            与mirror Object在虚拟机中代表一个Java Object的作用是一致的。
            
            Thread DecodeJObject 函数比较复杂，我们留待第11章介绍JNI的时候再来回顾它  */
            
            return down_cast<T>(Self()->DecodeJObject(obj));
        }

        //此函数是上述Decode函数的逆向函数，即将输入的mirror Object转换成对应的JNI引用类型。详情见下文
        template<typename T> 
        T AddLocalReference(mirror::Object* obj) const {
            //调用JNIEnvExt的AddLocalReference函数，输入是一个mirror Object对象，返回值的类型由模板参数T决定。
            //使用时，T取值为jobject、jclass等JNI定义的类型。
            //下文将介绍AddLocalReference相关函数。
            return obj == nullptr ? nullptr : Env()->AddLocalReference<T>(obj);
        }
    
}



//我们来看一个使用 AddLocalReference 的代码段，代码如下所示。
//[java_lang_Class.cc->Class_getNameNative]
//下面这个函数用于返回java class的类名，读者只要关注AddLocalReference的用法即可
static jstring Class_getNameNative(JNIEnv* env, jobject javaThis) {
    ScopedFastNativeObjectAccess soa(env);
    StackHandleScope<1> hs(soa.Self());
    mirror::Class* const c = DecodeClass(soa, javaThis);
    //Class::ComputeName 函数返回值的类型是mirror::String。
    //然后经过 AddLocalReference 转转换成jstring返回
    return soa.AddLocalReference<jstring>(mirror::Class::ComputeName(hs.NewHandle(c)));
}












//接下来继续认识JNI中的几个常用函数。
//[jni_internal.cc->JNI::FindClass]
static jclass FindClass(JNIEnv* env, const char* name) {
    /*本函数为JNI类的静态成员函数，用于查找指定类名对应的类信息：
      第一个参数为JNIEnv对象，
      第二个参数为目标类类名。注意，name中的"."号需要换成"/"号。
      比如，想查找"java.lang.System"类的话，传入的name参数可以是"Ljava/lang/System;"或"java.lang.System"。
      "Ljava/lang/System;"为JNI规范的要求，见下面的介绍。 
    */
    Runtime* runtime = Runtime::Current();    //获取runtime对象
    
    //获取ClassLinker对象
    ClassLinker* class_linker = runtime->GetClassLinker();
    
    /*JNI规范要求的类名和name传的不太一样。比如类名为"java.lang.System"的JNI类名是"Ljava/lang/System;"。
    多了一个"L"前缀和";"后缀。下面这个函数将完成这种转换
    */
    std::string descriptor(NormalizeJniClassDescriptor(name));
    ScopedObjectAccess soa(env);
    
    //注意，这里定义了一个mirror Class对象
    mirror::Class* c = nullptr; 
        
    /*IsStarted返回runtime的started_成员变量。我们在本章最开始介绍Runtime Start
      函数时就见过它了。不管虚拟机有没有启动，目标类的搜索工作都将由ClassLinker来完成，
      而返回的就是一个mirror Class对象。
      ClassLinker的 FindClass 和 FindSystemClass 我们在7.8.3节简单介绍过，更细节的内容我们留待后文再述 */
    if (runtime->IsStarted()) {
        StackHandleScope<1> hs(soa.Self());
        Handle<mirror::ClassLoader> class_loader(hs.NewHandle(GetClassLoader(soa)));
        c = class_linker->FindClass(soa.Self(), descriptor.c_str(), class_loader);
    } else {
        c = class_linker->FindSystemClass(soa.Self(), descriptor.c_str());
    }
    //通过 ScopedObjectAccess 的AddLocalReference函数，将输入的mirror Class对象转换成一个jclass的值然后返回
    return soa.AddLocalReference<jclass>(c);
}
    
    
//[jni_internal.cc->JNI::FindMethod]
static jint RegisterNatives(JNIEnv* env, 
                            jclass java_class,
                            const JNINativeMethod* methods, 
                            jint method_count) {
    
    //内部调用另外一个同名函数，直接来看它
    return RegisterNativeMethods(env, java_class, methods, method_count, true);
}


/*输入参数的解释：
  （1） java_class：代表目标java class
  （2） methods：是一个数组。每一个元素对应的类型为结构体JNINativeMethod，它包含name（java
       native函数的函数名）、signature（java native函数的签名信息，符合JNI规范，由返回值
       类型和参数类型共同构成）、fnPtr（JNI库中对应函数的函数地址）这三个成员
  （3） method_count：methods数组中元素的个数    */
static jint RegisterNativeMethods(JNIEnv* env, jclass java_class,
                                    const JNINativeMethod* methods, 
                                    jint method_count, 
                                    bool return_errors) {

    ScopedObjectAccess soa(env);
    mirror::Class* c = soa.Decode<mirror::Class*>(java_class);
    
    for (jint i = 0; i < method_count; ++i) {   //遍历methods数组
        const char* name = methods[i].name;     //java native函数的函数名
        const char* sig = methods[i].signature; //函数签名
        const void* fnPtr = methods[i].fnPtr;   //JNI层函数地址(即Native函数地址)
        ......
        
        bool is_fast = false;                   //fast jni模式，见下文解释
        if (*sig == '!') {
            is_fast = true;
            ++sig;
        }
        
        ArtMethod* m = nullptr;
        bool warn_on_going_to_parent = down_cast<JNIEnvExt*>(env)->vm->IsCheckJniEnabled();
        
        //搜索目标类及它的父类
        for (mirror::Class* current_class = c; 
            current_class != nullptr; 
            current_class = current_class->GetSuperClass()) {
            //从 current_class 中找到函数名为 name，且签名信息与 sig 相同的函数，返回值指向一个 ArtMethod 对象。
            //FindMethod是模板函数，当模板参数取值为true的时候，表示只搜索类中标记为native的函数。
            m = FindMethod<true>(current_class, name, sig);
            if (m != nullptr) { 
                break; 
            }
            
            //如果上面的FindMethod<true>没有找到匹配的ArtMethod对象，则尝试搜索非native标记的函数。
            //不过我们就是要处理标记为native的函数，这里找出非native的函数有什么用？请接着看下文
            m = FindMethod<false>(current_class, name, sig);
            if (m != nullptr) {   
                break;  
            }
            ......
        }
        
        if (m == nullptr) { 
            return JNI_ERR;
        }
        else if (!m->IsNative()) {
            //原来，如果找到的ArtMethod并不是一个native标记的函数，则此处打印一条错误信息并返回错误
            ......
            return JNI_ERR;
        }
        
        //调用 ArtMethod 的RegisterNative函数。我们先不讨论其内部代码
        m->RegisterNative(fnPtr, is_fast);
    }
    return JNI_OK;
}








//8.2.3.3 LocalRef、GlobalRef和WeakGlobalRef相关函数
//[jni_internal.cc]
//将jobject对象转换成全局引用对象和删除
static jobject NewGlobalRef(JNIEnv* env, jobject obj);
static void DeleteGlobalRef(JNIEnv* env, jobject obj);

//创建和删除弱全局引用
static jweak NewWeakGlobalRef(JNIEnv* env, jobject obj);
static void DeleteWeakGlobalRef(JNIEnv* env, jweak obj);

//创建和删除弱引用对象
static jobject NewLocalRef(JNIEnv* env, jobject obj);
static void DeleteLocalRef(JNIEnv* env, jobject obj);



//[jni_internal.cc->JNI::NewGlobalRef]
static jobject NewGlobalRef(JNIEnv* env, jobject obj) {
    ScopedObjectAccess soa(env);
    //先解析出这个jobject对应的mirror Object对象
    mirror::Object* decoded_obj = soa.Decode<mirror::Object*>(obj);

    //调用JavaVMExt的 AddGlobalRef 函数
    return soa.Vm()->AddGlobalRef(soa.Self(), decoded_obj);
}

//[java_vm_ext.cc->JavaVMExt::AddGlobalRef]
jobject JavaVMExt::AddGlobalRef(Thread* self, mirror::Object* obj) {
    if (obj == nullptr) { return nullptr;}
    WriterMutexLock mu(self, globals_lock_);
    /*globals_ 是JavaVMExt的成员变量，其类型是 IndirectReferenceTable。
    如其名所示，它是一个容器，可以通过Add往里边添加元素。返回值的类型是 IndirectRef。
        （1）IndirectRef 定义为typedef void* IndirectRef。所以，它其实就是void*。
        （2）IRT_FIRST_SEGMENT 是一个int变量，取值为0，其具体含义和 IndirectReferenceTable 的实现有关。
    */
    IndirectRef ref = globals_.Add(IRT_FIRST_SEGMENT, obj);
    return reinterpret_cast<jobject>(ref);
}
