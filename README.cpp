5.4.1 操作符重载(p155)
5.4.3 智能指针(p159)
5.4.4 new和delete重载(p1616)
5.4.5 函数对象(p165)

5.5.1 函数模板(p169)
5.5.2 类模板(p172)


app_main.cpp->main() {

    //调用基类AndroidRuntime的start函数 
    //[*]runtime.start("com.android.internal.os.ZygoteInit", args, zygote)
    //【章节7.1】
    AndroidRuntime.cpp->AndroidRuntime::start() {

        //加载ART虚拟机的核心动态库，从libart.so里将取出并保存三个函数的函数指针
        //第二个函数 JNI_CreateJavaVM 用于创建Java虚拟机，所以它是最关键的
        //[*]jni_invocation.Init(NULL)
        //【章节7.1】
        JniInvocation.cpp->JniInvocation::Init() {}

        //启动ART虚拟机
        //[*]startVm(&mJavaVM, &env, zygote) != 0)
        //【章节7.1】
        AndroidRuntime.cpp->AndroidRuntime::startVm() {

            //JNI_CreateJavaVM函数并非是JniInovcation Init从 libart.so 获取的那个JNI_CreateJavaVM函数
            //[*]JNI_CreateJavaVM(pJavaVM, pEnv, &initArgs) < 0)
            //【章节7.1】
            AndroidRuntime.cpp->JNI_CreateJavaVM() {

                //而JniInvocation又会调用libart.so中定义的那个 JNI_CreateJavaVM 函数
                //[*]JniInvocation::GetJniInvocation().JNI_CreateJavaVM(p_vm, p_env, vm_args)
                //【章节7.1】
                java_vm_ext.cc->JNI_CreateJavaVM() { //libart.so中的函数

                    //【① 第7章】创建Runtime对象，它就是ART虚拟机的化身
                    //[*]if (!Runtime::Create(options, ignore_unrecognized))
                    //【章节7.2】
                    runtime.cc->Runtime::Create(RuntimeOptions，bool) {

                        //[*] && Create(std::move(runtime_options))
                        runtime.cc->Runtime::Create(RuntimeArgumentMap && ) {
                            //创建Runtime对象
                            instance_ = new Runtime;

                            //使用runtime_options来初始化这个runtime对象
                            //[*]instance_->Init(std::move(runtime_options))
                            //【章节7.2】
                            bool Runtime::Init(RuntimeArgumentMap && runtime_options_in) {
                                
                                //关键模块之MemMap，用于管理内存映射
                                //【章节7.3.1】
                                MemMap::Init();

                                //关键模块之OatFileManager：art虚拟机会打开多个oat文件，通过该模块可统一管理它们
                                //【章节7.3.2】
                                oat_file_manager_ = new OatFileManager;
                                
                                //关键模块之Monitor，和Java中的monitor有关，用于实现线程同步的模块。其详情见本书第12章的内容
                                Monitor::Init(runtime_options.GetOrDefault(Opt::LockProfThreshold));

                                //关键模块之Heap，heap是art虚拟机中非常重要的模块。详情见下文分析
                                //【章节7.6】
                                heap_ = new gc::Heap(......);

                                //关键模块ArenaPool及LinearAlloc，runtime内部也需要创建很多对象或者需要存储一些信息。
                                //该模块的代码非常简单，请读者自行阅读
                                const bool use_malloc = IsAotCompiler();
                                arena_pool_.reset(new ArenaPool(use_malloc, false));
                                jit_arena_pool_.reset(new ArenaPool(false, false, "CompilerMetadata"));
                                linear_alloc_.reset(CreateLinearAlloc());

                                //接下来的一段代码和信号处理有关。ART虚拟机进程需要截获来自操作系统的某些信号
                                BlockSignals(); //阻塞SIGPIPE、SIGQUIT和SIGUSER1信号
                                InitPlatformSignalHandlers(); //为某些信号设置自定义的信号处理函数

                                if (!no_sig_chain_) {                              
                                    //关键模块之FaultManager：该模块用于处理SIGSEV信号
                                    //【章节7.4】
                                    fault_manager.Init();
                                }
                                
                                //关键模块之JavaVmExt
                                //【章节7.7】
                                java_vm_ = new JavaVMExt(this, runtime_options);
                                
                                //关键模块之Thread
                                //下面两个函数调用Thread类的Startup和Attach以初始化虚拟机主线程
                                //【章节7.5】
                                Thread::Startup();
                                //[*] [thread.cc->Thread::Attach]
                                Thread* self = Thread::Attach("main", false, nullptr, false){
                                    //①关键函数之一构造函数
                                    self = new Thread(as_daemon);

                                    //②关键函数之二
                                    bool init_success = self->Init(runtime->GetThreadList(), runtime->GetJavaVM()){
                                        //每一个线程将关联一个JNIEnvExt对象，它存储在 tlsPtr_jni_env 变量中。对主线程而言，
                                        //JNIEnvExt对象由Runtime创建并传给代表主线程的Thread对象，也就是此处分析的Thread对象
                                        if (jni_env_ext != nullptr) {
                                            tlsPtr_.jni_env = jni_env_ext;
                                        } else { 
                                            //如果外界不传入JNIEnvExt对象，则自己创建一个
                                            tlsPtr_.jni_env = JNIEnvExt::Create(this, java_vm);
                                            ......
                                        }
                                    }
                                       
                                    //③关键函数之三
                                    self->InitStringEntryPoints();
                                }
                                
                                //关键模块之ClassLinker：ClassLinker也是非常重要的模块。
                                //类的连接器，即将类关联和管理起来
                                //【章节7.8】
                                class_linker_ = new ClassLinker(intern_table_);
                                if (GetHeap()->HasBootImageSpace()) {
                                    //从oat镜像文件中初始化class linker，也就是从oat文件中获取类等信息。
                                    //【重点!】
                                    bool result = class_linker_->InitFromBootImage(&error_msg);
                                }
                                
                                //关键模块之MethodVerifier：用于校验Java方法的模块。
                                //下一章介绍类校验方面知识时将接触 MethodVerifier 类。本书不拟对该类做过多介绍。
                                verifier::MethodVerifier::Init();
                                
                                
                            }
                        }

                    }

                    //【② 第8章】启动Runtime。注意，这部分内容留待下一章介绍
                    //【章节8.1】
                    bool started = runtime->Start(){
                        //①加载JIT编译模块所对应的so库 
                        //【章节8.3】
                        if (!jit::Jit::LoadCompilerLibrary(&error_msg)) {......}
                        
                        
                        ScopedTrace trace2("InitNativeMethods");
                        //②初始化JNI层相关内容 
                        //【章节8.4】
                        InitNativeMethods();
                        
                        
                        //③完成和Thread类初始化相关的工作
                        //【章节8.5】
                        InitThreadGroups(self);   //缓存知名类（well known class）java.lang.ThreadGroup中的 
                                                  //mainThreadGroup 和 systemThreadGroup 这两个成员变量。
                        Thread::FinishStartup();  //完成Thread类的启动工作
                        
                        //④创建系统类加载器（system class loader），返回值存储到成员变量 system_class_loader_ 里
                        //【章节8.6】
                        system_class_loader_ = CreateSystemClassLoader(this);
                        
                        // 启动虚拟机里的daemon线程，本章将它与第③个关键点一起介绍
                        //【章节8.5】
                        StartDaemonThreads();  //启动虚拟机里的daemon线程。
                        
                    }

                    //获取JNI Env和Java VM对象
                     * p_env = Thread::Current()->GetJniEnv();
                     * p_vm = runtime->GetJavaVM();
                }
            }
        }
    }
}






//8.7.3 加载类(LoadClass)
//[class_linker.cc->ClassLinker::LoadClass]
void ClassLinker::LoadClass(Thread * self,
							const DexFile & dex_file,
							const DexFile::ClassDef & dex_class_def,
							Handle < mirror::Class > klass) {
												

	//LoadClassMembers(self, dex_file, class_data, klass, nullptr);
	void ClassLinker::LoadClassMembers(Thread * self,
									const DexFile & dex_file,
									const uint8_t * class_data,
									Handle < mirror::Class > klass,
									const OatFile::OatClass * oat_class) {
										
										
		//遍历 class_data_item 中的静态成员变量数组，然后填充信息到 sfields 数组里
		 for (; it.HasNextStaticField(); it.Next()) {
			 LoadField(it, klass, &sfields->At(num_sfields));
		}	
		 
		//遍历 class_data_item 中的非静态成员变量数组，然后填充信息到 ifields 数组里
        for (; it.HasNextInstanceField(); it.Next()) {
			LoadField(it, klass,  & ifields->At(num_ifields)); //类似的处理
		}
		
		//设置Class类的 sfields_ 和 ifields_成员变量
        klass->SetSFieldsPtr(sfields);
        klass->SetIFieldsPtr(ifields);
		klass->SetMethodsPtr(
				AllocArtMethodArray(self, allocator, it.NumDirectMethods() + it.NumVirtualMethods()), 
				it.NumDirectMethods(), 
				it.NumVirtualMethods()
			);
		
		for (size_t i = 0; it.HasNextDirectMethod(); i++, it.Next()) {
			LoadMethod(self, dex_file, it, klass, method);
			//注意，oat_class 信息只在LinkCode中用到。LinkCode留待10.1节介绍
            LinkCode(method, oat_class, class_def_method_index);
		}
		
		//处理virtual方法。注意，对表示virtual方法的ArtMethod对象而言，
		//它们的 method_index_ 和 klass  methods_ 数组没有关系，也不在下面的循环中设置。
        for (size_t i = 0; it.HasNextVirtualMethod(); i++, it.Next()) {
            //和direct方法处理一样，唯一不同的是，此处不调用ArtMethod的 SetMethodIndex 函数，即不设置它的 method_index_ 成员
			
			LoadMethod(self, dex_file, it, klass, method);
			LinkCode(method, oat_class, class_def_method_index);
		}
										
	}
}								