//【8.5.1】
//[runtime.cc->Runtime::InitThreadgroups]
void Runtime::InitThreadGroups(Thread* self) {
    JNIEnvExt* env = self->GetJniEnv();
    ScopedJniEnvLocalRefState env_state(env);
    
    //main_thread_group_为jobject，保存了java/lang/ThreadGroup类中的mainThreadGroup成员
    main_thread_group_ = env->NewGlobalRef(env->GetStaticObjectField(
            WellKnownClasses::java_lang_ThreadGroup,    //目标类
            //成员的ID
            WellKnownClasses::java_lang_ThreadGroup_mainThreadGroup));
            
    system_thread_group_ = env->NewGlobalRef(env->GetStaticObjectField(
            WellKnownClasses::java_lang_ThreadGroup,
            WellKnownClasses::java_lang_ThreadGroup_systemThreadGroup));
}



//【8.5.2】
//[thread.cc->Thread::FinishStartup]
void Thread::FinishStartup() {
    Runtime* runtime = Runtime::Current();
    ScopedObjectAccess soa(Thread::Current());
    
    //GetMainThreadGroup 函数返回上节 InitThreadGroups 中所获取的 main_thread_group_ ，注意，
    //第一个参数代表线程名，取值为"main"
    Thread::Current()->CreatePeer("main", false, runtime->GetMainThreadGroup());
    Thread::Current()->AssertNoPendingException();
    
    //调用ClassLinker的RunRootClinits，即执行class root里相关类的初始化函数，
    //也就是执行它们的"<clinit>"函数（如果有的话）。
    Runtime::Current()->GetClassLinker()->RunRootClinits();
}
/*
在Java Thread类中有一个名为 nativePeer 的成员变量，这个变量就是该Thread实例所关联的操作系统的线程。
当然，出于管理需要，nativePeer 并不会直接对应到操作系统里线程ID这样的信息，而是根据不同虚拟机的实现被设置成不同的信息。

CreatePeer的功能包括两个部分：
·创建一个Java Thread实例。
·把调用线程（操作系统意义的线程，即此处的art Thread对象）关联到上述Java Thread实例的nativePeer成员。

简单点说，ART虚拟机执行到这个地方的时候，代表主线程的操作系统线程已经创建好了，
但Java层里的主线程Thread示例还未准备好。而这个准备工作就由CreatePeer来完成。
*/
//[thread.cc->Thread::CreatePeer]
void Thread::CreatePeer(const char* name, bool as_daemon, jobject thread_group) {
    //注意，本例中，name的取值为"main"，代表主线程
    Runtime* runtime = Runtime::Current();
    
    //tlsPtr_ 是Thread类的关键，读者可回顾为7.5.2.1节的内容
    JNIEnv* env = tlsPtr_.jni_env;
    if (thread_group == nullptr) {
        thread_group = runtime->GetMainThreadGroup();
    }
    
    //创建一个Java String对象，其内容为线程的名称，比如此例的"main"
    ScopedLocalRef<jobject> thread_name(env, env->NewStringUTF(name));
    jint thread_priority = GetNativePriority();

    jboolean thread_is_daemon = as_daemon;
     
    //创建一个Java Thread对象
    ScopedLocalRef<jobject> peer(env, env->AllocObject(WellKnownClasses::java_lang_Thread));
    
    ......
    
    {
        ScopedObjectAccess soa(this);
        //tlsPtr_的opeer成员保存了Java层Thread实例
        tlsPtr_.opeer = soa.Decode<mirror::Object*>(peer.get());
    }
    
    //调用该Java Thread的init函数，进行一些初始化工作，比如设置函数名
    env->CallNonvirtualVoidMethod(peer.get(),
                                WellKnownClasses::java_lang_Thread,
                                WellKnownClasses::java_lang_Thread_init,
                                thread_group, thread_name.get(), thread_priority,
                                thread_is_daemon);
                                
    ......
    
    Thread* self = this;
    
    //把调用线程（指此处的art Thread对象）对象的地址设置到peer（Java Thread对象）的nativePeer成员。这样，art Thread对象就和一个Java Thread对象关联了起来
    env->SetLongField(peer.get(), WellKnownClasses::java_lang_Thread_nativePeer, reinterpret_cast<jlong>(self));
    
    ......
    
}


//接着来看RunRootClinits函数，从其名称可以看出，ClassLinker中在之前获取的root class将被初始化。
//[class_linker.cc->ClassLinker::RunRootClinits]
void ClassLinker::RunRootClinits() {
    Thread* self = Thread::Current();

    //关于Root class，读者可回顾7.8.3节的内容
    for (size_t i = 0; i < ClassLinker::kClassRootsMax; ++i) {
        mirror::Class* c = GetClassRoot(ClassRoot(i));
        
        //初始化root classes中的那些非数组类及引用类型的class
        if (!c->IsArrayClass() && !c->IsPrimitive()) {
            StackHandleScope<1> hs(self);
            Handle<mirror::Class> h_class(hs.NewHandle(GetClassRoot(ClassRoot(i))));
            //确保该class初始化。该函数我们将在下一节详细介绍
            EnsureInitialized(self, h_class, true, true);
            self->AssertNoPendingException();
        }
    }
}



//8.5.3　Runtime::StartDaemonThreads
//StartDaemonThreads函数本身非常简单，代码如下所示。
//[runtime.cc->Runtime::StartDaemonThreads]
void Runtime::StartDaemonThreads() {
    ScopedTrace trace(__FUNCTION__);
    
    ......
    
    Thread* self = Thread::Current();
    JNIEnv* env = self->GetJniEnv();
    //其实就是调用Java Daemons类的start函数
    env->CallStaticVoidMethod(WellKnownClasses::java_lang_Daemons, WellKnownClasses::java_lang_Daemons_start);
    
    ......
    
}



/*

·Daemon有4个派生类，分别是 HeapTaskDaemon、ReferenceQueueDaemon、FinalizerDaemon 和 FinalizerWatchdogDaemon。
这四个派生类实现了run函数，而且都和GC有关。
*/
//[Daemons.java->Daemons::start]
public static void start() {
        ReferenceQueueDaemon.INSTANCE.start();
        FinalizerDaemon.INSTANCE.start();
        FinalizerWatchdogDaemon.INSTANCE.start();
        HeapTaskDaemon.INSTANCE.start();
}