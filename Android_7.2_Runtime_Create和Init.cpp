//runtime.cc->Runtime::Create(RuntimeOptions，bool)
bool Runtime::Create(const RuntimeOptions& raw_options, bool ignore_unrecognized) {
    /*虚拟机是一个复杂系统，所以它有很多控制参数。
    创建Runtime时，调用者将这些参数信息放在本函数的入参raw_options对象中，
    该对象的类型是RuntimeOptions。不过，Runtime内部却使用类型为RuntimeArgumentMap的对象来存储参数。
    下面这段代码中，ParseOptions函数将存储在raw_options里的参数信息提取并保存到runtime_options对象里，
    而runtime_options的类型就是RuntimeArgumentMap。*/

    RuntimeArgumentMap runtime_options;
    
    //·ParseOptions先将外部调用者传入的参数信息（保存在raw_options中）提取并保存到runtime_options里。
    //·然后再用runtime_options作为入参来创建（Create）一个Runtime对象。
    return ParseOptions(raw_options, ignore_unrecognized, &runtime_options) 
        && Create(std::move(runtime_options));
}



//runtime.cc->Runtime::Create(RuntimeArgumentMap&&)
bool Runtime::Create(RuntimeArgumentMap&& runtime_options) {
    //一个虚拟机进程中只有一个Runtime对象，名为instance_，采用单例方式来创建
    if (Runtime::instance_ != nullptr) { return false; }
    
    instance_ = new Runtime; //创建Runtime对象
    //用保存了虚拟机控制参数信息的runtime_options来初始化这个runtime对象。
    //重点来看Init函数
    if (!instance_->Init(std::move(runtime_options))) {....}
    return true;
}



//初始化这个runtime对象
//runtime.cc->Runtime::Init
bool Runtime::Init(RuntimeArgumentMap&& runtime_options_in) {
    RuntimeArgumentMap runtime_options(std::move(runtime_options_in));
    
    //关键模块之MemMap：用于管理内存映射。ART大量使用了内存映射技术。
    //比如.oat文件就会通过mmap映射到虚拟机进程的虚拟内存中来。
	//【章节7.3.1】
    MemMap::Init();
    using Opt = RuntimeArgumentMap;//C++11里using的用法
    QuasiAtomic::Startup(); //MIPS架构中需要使用它，其他CPU架构可不考虑
    
    //关键模块之OatFileManager：art虚拟机会打开多个oat文件，通过该模块可统一管理它们
	//【章节7.3.2】
    oat_file_manager_ = new OatFileManager;

    Thread::SetSensitiveThreadHook(runtime_options.GetOrDefault(Opt::HookIsSensitiveThread));
    
    //关键模块之Monitor：和Java中的monitor有关，用于实现线程同步的模块。其详情见本书第12章的内容
    Monitor::Init(runtime_options.GetOrDefault(Opt::LockProfThreshold));
    
    /*从runtime_options中提取参数。Opt是 RuntimeArgumentMap 的别名，
    而 BootClassPath 是runtime_options.def中定义的一个控制参数的名称。
    该控制参数的数据类型是vector<unique_ptr<const DexFile>>。
    从RuntimeArgumentMap中获取一个控制参数的值的函数有两个：
     （1）GetOrDefault：从指定参数中获取其值，如果外界没有设置该控制参数，
            则返回参数配置文件里的配置的默认值。这里的参数配置文件就是上文提到的runtime_options.def。
     （2）ReleaseOrDefault：功能和GetOrDefault一样，唯一的区别在于如果外界设置了该参数，
           该函数将通过std::move函数将参数的值返回给调用者。std::move的含义我们在第5章中已做过介绍。
           使用move的话，外界传入的参数将移动到返回值所在对象里，从而节省了一份内存。
           比如，假设参数值存储在一个string对象中，如果不使用move的话，
           那么RuntimeArgumentMap内部将保留一份string，而调用者拿到作为返回值的另外一份string。
           显然，不使用move的话，将会有两个string对象，内存会浪费一些。
           所以，ReleaseOrDefault用于获取【类类型】的控制参数的值，
           而对于int等基础数据类型，使用GetOrDefault即可。*/
    boot_class_path_string_ = runtime_options.ReleaseOrDefault(Opt::BootClassPath);
    
    ......//从runtime_options中获取其他控制参数的值


    /*接下来的关键模块为：
     （1）MointorList：它维护了一组Monitor对象
     （2）MonitorPool：用于创建Monitor对象
     （3）ThreadList：用于管理ART中的线程对象（线程对象的数据类型为Thread）的模块
     （4）InternTable：该模块和string intern table有关。它其实就是字符串常量池。
         根据Java语言规范（Java Language Specification，简写为JLS）的要求，
         内容完全相同的字符串常量（string literal）应该共享同一份资源。
         比如，假设String a="hello"，String b="hello"，那么a==b（直接比较对象a是否等于对象b）应该返回true。
         intern_table_ 的目的很好理解，就是减少内存占用。
         另外，String类中有一个intern方法，它可以将某个String对象添加到intern table中。*/
    monitor_list_ = new MonitorList;
    monitor_pool_ = MonitorPool::Create();
    thread_list_ = new ThreadList;
    intern_table_ = new InternTable;
    
    .....//从runtime_options中获取控制参数



    //关键模块之Heap：heap是art虚拟机中非常重要的模块。详情见下文分析
	//【章节7.6】
    heap_ = new gc::Heap(......);

    ....
	
    //和lambda有关，以后碰见它时再来介绍
    lambda_box_table_ = MakeUnique<lambda::BoxTable>();
    
    /*关键模块 ArenaPool 及 LinearAlloc ：runtime内部也需要创建很多对象或者需要存储一些信息。
    为了更好地管理虚拟机自己的内存使用，runtime设计了：
     （1）内存池类ArenaPool。ArenaPool可管理多个内存单元（由Arena表示）。
     （2）对内存使用者而言，使用内存分配器（LinearAlloc）即可在ArenaPool上分配任意大小的内存。
     该模块的代码非常简单，请读者自行阅读。*/
    const bool use_malloc = IsAotCompiler();
    arena_pool_.reset(new ArenaPool(use_malloc, false));
    jit_arena_pool_.reset(new ArenaPool(false, false, "CompilerMetadata"));
    linear_alloc_.reset(CreateLinearAlloc());

    //接下来的一段代码和信号处理有关。ART虚拟机进程需要截获来自操作系统的某些信号
    BlockSignals();//阻塞SIGPIPE、SIGQUIT和SIGUSER1信号
    
    /*为某些信号设置自定义的信号处理函数。该函数在linux和android平台上的处理不尽相同。
    在android（也就是针对设备的编译）平台上，这段代码并未启用。
    详情可参考该函数在runtime_android.cc中的实现*/
    InitPlatformSignalHandlers();

    if (!no_sig_chain_) {//对在目标设备上运行的art虚拟机来说，该变量取默认值false
    
        //获取sigaction和sigprocmask两个函数的函数指针。这和linux信号处理
        //函数的调用方法有关。此处不拟讨论它，感兴趣的读者可参考代码中的注释
        InitializeSignalChain();
        
        /*下面三个变量的介绍如下：
          （1）implicit_null_checks_：是否启用隐式空指针检查，此处取值为true。
          （2）implict_so_checkes_：是否启用隐式堆栈溢出（stackoverflow）检查，此处取值为true。
          （3）implict_suspend_checks_：是否启用隐式线程暂停（thread suspension）检查，此处取值为false。
            suspend check相关内容将在第11章做详细介绍。*/
        if (implicit_null_checks_ || implicit_so_checks_ || implicit_suspend_checks_) {
            //关键模块之FaultManager：该模块用于处理SIGSEV信号
			//【章节7.4】
            fault_manager.Init();
			
            /*下面的SuspensionHandler、StackOverflowHandler和NullPointerHandler有共同的基类FaultHandler，
			 笔者将它们归为关键模块FaultManager之中。这部分内容留待下文再介绍*/
            if (implicit_suspend_checks_) {
                new SuspensionHandler(&fault_manager);
            }
            if (implicit_so_checks_) {
                new StackOverflowHandler(&fault_manager);
            }
            if (implicit_null_checks_) {
                new NullPointerHandler(&fault_manager);
            }
            .....    
        }



    }
    
    
    /*关键模块之JavaVmExt：JavaVmExt就是JNI中代表Java虚拟机的对象，其基类为JavaVM，
    真实类型为JavaVmExt。根据JNI规范，一个进程只有唯一的一个JavaVm对象。
    对art虚拟机来说，这个JavaVm对象就是此处的java_vm_。*/
	//【章节7.7】
    java_vm_ = new JavaVMExt(this, runtime_options);
    
    //关键模块之Thread：Thread是虚拟机中代表线程的类，
    //下面两个函数调用Thread类的Startup和Attach以初始化虚拟机主线程
	//【章节7.5】
    Thread::Startup();
    Thread* self = Thread::Attach("main", false, nullptr, false);
    self->TransitionFromSuspendedToRunnable();
    
    //关键模块之ClassLinker：ClassLinker也是非常重要的模块。
    //从其命名可以看出，它处理和Class有关的工作，比如解析某个类、寻找某个类等
	//【章节7.8】
    class_linker_ = new ClassLinker(intern_table_);
    if (GetHeap()->HasBootImageSpace()) {
        std::string error_msg;
        //从oat镜像文件中初始化class linker，也就是从oat文件中获取类等信息。
        bool result = class_linker_->InitFromBootImage(&error_msg);
        {
            ScopedTrace trace2("AddImageStringsToTable");
            //处理和 intern table有关的初始化
            GetInternTable()->AddImagesStringsToTable(heap_->GetBootImageSpaces());
        }
        {
            ScopedTrace trace2("MoveImageClassesToClassTable");
            //art虚拟机中每一个class loader 都有一个class table，
            //它存储了该loader所加载的各种class。
            //下面这个函数将把来自镜像中的类信息添加到boot class loader对应的ClassTable中。
            //这部分内容将在ClassLinker一节中介绍
            GetClassLinker()->AddBootImageClassesToClassTable();
        }
    }
    
    ......
    
    //关键模块之MethodVerifier：用于校验Java方法的模块。
    //下一章介绍类校验方面知识时将接触MethodVerifier类。本书不拟对该类做过多介绍。
    verifier::MethodVerifier::Init();
    
    /*下面这段代码用于创建两个异常对象。注意，此处ThrowNewException将创建异常对象，
    而ClearException将清除异常对象。这样的话，Init函数返回后将不会导致异常投递。
    这是JNI函数中常用的做法。读者可以先不用了解这么多，后续章节介绍JNI及异常投递时还会详细介绍。
    pre_allocated_OutOfMemoryError_和pre_allocated_NoClassDefFoundError_代表Java层OutOfMemoryError
    对象和NoClassDefFoundError对象。*/
    self->ThrowNewException("Ljava/lang/OutOfMemoryError;",....);
    pre_allocated_OutOfMemoryError_ = GcRoot<mirror::Throwable>(self->GetException());
    self->ClearException();

    self->ThrowNewException("Ljava/lang/NoClassDefFoundError;",...);
    pre_allocated_NoClassDefFoundError_ = GcRoot<mirror::Throwable>(self->GetException());
    self->ClearException();
    
    ......//native bridge library加载，本文不涉及相关内容
    
    return true;
}