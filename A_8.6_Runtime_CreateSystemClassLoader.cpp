//本节来看Runtime Start中的最后一个关键函数——CreateSystemClassLoader，代码非常简单。
//创建系统类加载器（system class loader），返回值存储到成员变量 system_class_loader_ 里
//[runtime.cc->Runtime::CreateSystemClassLoader]
static jobject CreateSystemClassLoader(Runtime* runtime) {
    
    ......
    
    ScopedObjectAccess soa(Thread::Current());
    ClassLinker* cl = Runtime::Current()->GetClassLinker();
    auto pointer_size = cl->GetImagePointerSize();

    StackHandleScope<2> hs(soa.Self());
    
    //获取java/lang/ClassLoader对应的Class对象
    Handle<mirror::Class> class_loader_class(hs.NewHandle(soa.Decode<mirror::Class*>( 
            WellKnownClasses::java_lang_ClassLoader)));
            
    //找到ClassLoader类的getSystemClassLoader函数
    ArtMethod* getSystemClassLoader = class_loader_class->FindDirectMethod(
            "getSystemClassLoader", "()Ljava/lang/ClassLoader;",pointer_size);
            
    //调用上面的getSystemClassLoader方法。具体调用过程我们后文会介绍
    JValue result = InvokeWithJValues(soa, nullptr, soa.EncodeMethod(getSystemClassLoader), nullptr);
    
    JNIEnv* env = soa.Self()->GetJniEnv();
    
    //调用的结果是一个jobject对象，待会我们看完 getSystemClassLoader 这个Java函数的代码后，就知道这个jobject到底是什么了
    //【****】
	ScopedLocalRef<jobject> system_class_loader(env, soa.AddLocalReference<jobject>(result.GetL()));
	
	
    //下面这个函数将把获取得到的 jobject 对象存储到Thread tlsPtr_ 的 class_loader_override 成员变量中，
    //其作用以后我们遇见再说。
    soa.Self()->SetClassLoaderOverride(system_class_loader.get());
    
    
    
    
    
    //获取java/lang/Thread对应的Class对象
    Handle<mirror::Class> thread_class(hs.NewHandle(soa.Decode<mirror::Class*>(WellKnownClasses::java_lang_Thread)));
    //获取该类的 contextClassLoader 成员变量
    ArtField* contextClassLoader = thread_class->FindDeclaredInstanceField("contextClassLoader","Ljava/lang/ClassLoader;");
    //设置这个成员变量的值为刚才获取得到的那个 system_class_loader 对象
    contextClassLoader->SetObject<false>(soa.Self()->GetPeer(), soa.Decode<mirror::ClassLoader*>(system_class_loader.get()));
    
    //创建该对象的一个全局引用
    return env->NewGlobalRef(system_class_loader.get());
}





//要看懂上述代码，最好结合Java层对应的部分。先来看ClassLoader Java类里的那个getSystemClassLoader函数。
//[ClassLoader.java->ClassLoader::getSystemClassLoader]
public static ClassLoader getSystemClassLoader() {
        //loader是SystemClassLoader类的静态成员。此函数返回的数据类型是ClassLoader
        return SystemClassLoader.loader;
}

//SystemClassLoader是ClassLoader的静态内部类
public class ClassLoader{
static private class SystemClassLoader {
        //loader成员变量的值来自于createSytemClassLoader。这部分先不介绍
        public static ClassLoader loader = ClassLoader.createSystemClassLoader();
    }
}

//接着来看createSystemClassLoader，它和8.7.9节的关系非常密切。
//[ClassLoader.java->ClassLoader::createSystemClassLoader]
private static ClassLoader createSystemClassLoader() {
    //读取系统属性"java.class.path"的值，默认取值为"."。
    String classPath = System.getProperty("java.class.path", ".");
    String librarySearchPath = System.getProperty("java.library.path", "");
    
    //SystemClassLoader的数据类型是PathClassLoader，并且使用BootClassLoader作为它的委托对象。
    return new PathClassLoader(classPath, librarySearchPath, BootClassLoader.getInstance());
}