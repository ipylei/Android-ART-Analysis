//app_main.cpp->main   执行该脚本启动zygote程序， Android中所有Java进程都由Zygote进程fork而来
int main(int argc, char* const argv[]){
    ......
    //AppRuntime是一个类，定义在app_main.cpp中。其基类是 AndroidRuntime
    AppRuntime runtime(argv[0], computeArgBlockSize(argc, argv));
    ...
    bool zygote = false;
    bool startSystemServer = false;
    bool application = false;
    .....
    //zygote脚本里的启动参数为
    //"-Xzygote /system/bin --zygote --start-system-server"
    while (i < argc) {
        const char* arg = argv[i++];
        if (strcmp(arg, "--zygote") == 0) {
            zygote = true;



        //niceName将被设置为app_process的进程名，32位机对应的进程名为"zygote"，而64位机上该进程名为"zygote64"
            niceName = ZYGOTE_NICE_NAME;
        } .....
    }
    ......
    if (zygote) {//调用基类AndroidRuntime的start函数
        runtime.start("com.android.internal.os.ZygoteInit", args, zygote);
    }
    ....
}




//AndroidRuntime.cpp->AndroidRuntime::start
void AndroidRuntime::start(const char* className, const Vector<String8>& options, bool zygote)  {
    ......
    //重点关注JniInvocation.Init函数和startVm函数，它们和启动ART虚拟机有关
    JniInvocation jni_invocation;
    jni_invocation.Init(NULL);   //它将加载ART虚拟机的核心动态库。
    JNIEnv* env;
    //在ART虚拟机对应的核心动态库加载到zyogte进程后，该函数将启动ART虚拟机。
    startVm(&mJavaVM, &env, zygote) != 0) { ...;}
    
    //下面的代码和Zygote进程的处理逻辑相关，本书不拟介绍它们。对这部分内容感兴趣的读者
    //可阅读由笔者撰写的《深入理解Android 卷1》一书
    .....
}



/*它将加载ART虚拟机的核心动态库。
从libart.so里将取出并保存三个函数的函数指针：
·这三个函数的代码位于java_vm_ext.cc中。
·第二个函数JNI_CreateJavaVM用于创建Java虚拟机，所以它是最关键的。
*/
//JniInvocation.cpp->JniInvocation::Init
bool JniInvocation::Init(const char* library) {
#ifdef __ANDROID__
    char buffer[PROP_VALUE_MAX];
#else
    char* buffer = NULL;
#endif
    //art核心库是通过动态加载so的方式加载到zygote进程的。GetLibrary根据情况返回目标so的文件名。
    //正常情况下加载的art核心动态库文件名为libart.so
    library = GetLibrary(library, buffer);
    const int kDlopenFlags = RTLD_NOW | RTLD_NODELETE;
    
    //加载libart.so
    handle_ = dlopen(library, kDlopenFlags);
    
    ....
    
    //从libart.so中找到 JNI_GetDefaultJavaVMInitArgs 函数的地址（也就是函数指针），
    //将该地址存储到 JNI_GetDefaultJavaVMInitArgs_ 变量中。
    if (!FindSymbol(reinterpret_cast<void**>(&JNI_GetDefaultJavaVMInitArgs_),"JNI_GetDefaultJavaVMInitArgs")) { 
        return false; 
    }
    //从libart.so中找到 JNI_CreateJavaVM 函数，它就是创建虚拟机的入口函数。
    //该函数的地址保存在 JNI_CreateJavaVM_ 变量中
    if (!FindSymbol(reinterpret_cast<void**>(&JNI_CreateJavaVM_),"JNI_CreateJavaVM")) {
        return false;
    }
    //从libart.so中找到 JNI_GetCreatedJavaVMs 函数
    if (!FindSymbol(reinterpret_cast<void**>(&JNI_GetCreatedJavaVMs_), "JNI_GetCreatedJavaVMs")) { 
        return false; 
    }
    return true;
}





//接着来看AndroidRuntime的startVm函数，代码如下所示。
//在ART虚拟机对应的核心动态库加载到zyogte进程后，该函数将启动ART虚拟机。
//AndroidRuntime.cpp->AndroidRuntime::startVm
int AndroidRuntime::startVm(JavaVM** pJavaVM, JNIEnv** pEnv, bool zygote)
{
    JavaVMInitArgs initArgs;
    
    ....//这段省略的代码非常长，其主要功能是为ART虚拟机准备启动参数，本章先不讨论它们
    
    initArgs.version = JNI_VERSION_1_4;
    initArgs.options = mOptions.editArray();
    initArgs.nOptions = mOptions.size();
    initArgs.ignoreUnrecognized = JNI_FALSE;
    
    /*调用 JNI_CreateJavaVM 。注意，该函数和JniInvocation Init从libart.so取出的 JNI_CreateJavaVM 函数同名，
    但它们是不同的函数JniInovcation Init取出的那个函数的地址保存在JNI_CreateJavaVM_（其名称后多一个下划线）变量中。
    这一点特别容易混淆，请读者注意*/
    if (JNI_CreateJavaVM(pJavaVM, pEnv, &initArgs) < 0) {...}
    return 0;
}



//如上述代码中的注释所言，JNI_CreateJavaVM函数并非是JniInovcation Init从libart.so获取的那个JNI_CreateJavaVM函数。
//相反，它是直接在AndroidRuntime.cpp中定义的，其代码如下所示。
//AndroidRuntime.cpp->JNI_CreateJavaVM
extern "C" jint JNI_CreateJavaVM(JavaVM** p_vm, JNIEnv** p_env,void* vm_args) {
    //调用JniInvocation中的JNI_CreateJavaVM函数，
    //而JniInvocation又会调用libart.so中定义的那个 JNI_CreateJavaVM 函数。
    return JniInvocation::GetJniInvocation().JNI_CreateJavaVM(p_vm, p_env, vm_args);
}




//java_vm_ext.cc-> JNI_CreateJavaVM
extern "C" jint JNI_CreateJavaVM(JavaVM** p_vm, JNIEnv** p_env,void* vm_args) {
    ScopedTrace trace(__FUNCTION__);
    const JavaVMInitArgs* args = static_cast<JavaVMInitArgs*>(vm_args);
    
    .....//为虚拟机准备参数
    
    bool ignore_unrecognized = args->ignoreUnrecognized;
    
    //①创建Runtime对象，它就是ART虚拟机的化身
    if (!Runtime::Create(options, ignore_unrecognized)) {...}
    
    //加载其他关键动态库，它们的文件路径由/etc/public.libraries.txt文件描述
    android::InitializeNativeLoader();

    Runtime* runtime = Runtime::Current();//获取刚创建的Runtime对象
    bool started = runtime->Start();//②启动runtime。注意，这部分内容留待下一章介绍
    ....
    //获取JNI Env和Java VM对象
    *p_env = Thread::Current()->GetJniEnv();
    *p_vm = runtime->GetJavaVM();
    return JNI_OK;
}
