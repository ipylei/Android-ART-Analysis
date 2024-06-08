5.4.1 操作符重载(p155)
5.4.3 智能指针(p159)
5.4.4 new和delete重载(p1616)
5.4.5 函数对象(p165)

5.5.1 函数模板(p169)
5.5.2 类模板(p172)


//【art虚拟机创建、启动流程】
//注：其实就是zygote进程启动所执行的流程，然后该进程里就有了art虚拟机环境，后面的app程序都是由zygote fork而来，所以也同样包含art虚拟机环境
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
								//JavaVmExt就是JNI中代表Java虚拟机的对象，其基类为JavaVM
                                //【章节7.7】
                                java_vm_ = new JavaVMExt(this, runtime_options);
                                
                                //关键模块之Thread
                                //下面两个函数调用Thread类的Startup和Attach以初始化虚拟机主线程
                                //【章节7.5.1】
                                Thread::Startup();
                                //[*] [thread.cc->Thread::Attach]
								//【章节 7.5.2】
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
								//它处理和Class有关的工作，比如解析某个类、寻找某个类等
                                //【章节7.8】
                                class_linker_ = new ClassLinker(intern_table_);
                                if (GetHeap()->HasBootImageSpace()) {
                                    //从oat镜像文件中初始化class linker，也就是从oat文件中获取类等信息。
                                    //【7.8.2和7.8.3 重点!】
                                    bool result = class_linker_->InitFromBootImage(&error_msg);
                                }
                                
                                //关键模块之 MethodVerifier：用于校验Java方法的模块。
                                //下一章介绍类校验方面知识时将接触 MethodVerifier 类。本书不拟对该类做过多介绍。
                                //【章节8.7.6】
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
						/*Runtime InitNativeMethods主要功能是：
							·缓存一些常用或知名类的类对象，方法是创建该类对应的全局引用对象。
							·缓存一些类的常用成员函数的ID，方法是找到并保存它的jmethodID。
							·缓存一些类的常用成员变量的ID，方法找到并保存它的jfieldID。
							·以及为一些类中native成员方法注册它们在JNI层的实现函数。*/
						//【章节8.4】
                        InitNativeMethods();
                        
                        
                        //③完成和Thread类初始化相关的工作
                        //【章节8.5】
                        InitThreadGroups(self);   //缓存知名类（well known class）java.lang.ThreadGroup中的 
                                                  //mainThreadGroup 和 systemThreadGroup 这两个成员变量。
												  
                        Thread::FinishStartup(){  //完成Thread类的启动工作
                            //简单点说，ART虚拟机执行到这个地方的时候，代表主线程的操作系统线程已经创建好了，
                            //但Java层里的主线程Thread示例还未准备好。而这个准备工作就由CreatePeer来完成。
                            Thread::Current()->CreatePeer("main", false, runtime->GetMainThreadGroup());
                            
                            //调用ClassLinker的RunRootClinits，即执行 class_root 里相关类的初始化函数，也就是执行它们的"<clinit>"函数（如果有的话）。
                            //注：class_root 在 【7.8.2和7.8.3】 class_linker_->InitFromBootImage中初始化了
                            Runtime::Current()->GetClassLinker()->RunRootClinits();
                        }
                        
                        
                        //④创建系统类加载器（system class loader），返回值存储到成员变量 system_class_loader_ 里
                        //【章节8.6】
                        system_class_loader_ = CreateSystemClassLoader(this);
                        
                        // 启动虚拟机里的daemon线程，本章将它与第③个关键点一起介绍
                        // 将启动（start）HeapTaskDaemon等四个派生类的实例。当然，每个实例都单独运行在一个线程中
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





//【8.7.5 类加载入口】
ClassLoader //含有loadClass(String name)函数
	BootClassLoader     //含有findClass(String name)函数
	BaseDexClassLoader  //含有findClass(String name)函数
	
	   PathClassLoader  //只能加载已经安装应用的 dex 或 apk 文件
	   DexClassLoader   //无限制，还可以从 SD 卡上加载包含 class.dex 的 .jar 和 .apk 文件，这也是插件化和热修复的基础，在不需要安装应用的情况下，完成需要使用的 dex 的加载

//示例：PathClassLoader or DexClassLoader

//classLoader.loadClass(name)
public Class<?>  ClassLoader::loadClass(String name){
	//return loadClass(name, false);
	protected Class<?> ClassLoader::loadClass(String name, boolean resolve){
        
		//c = findClass(name);
        //【java/dalvik/system/BaseDexClassLoader.java】
		protected Class<?> BaseDexClassLoader::findClass(String name) throws ClassNotFoundException { 
        
			// Class c = pathList.findClass(name, suppressedExceptions);
			//【java/dalvik/system/DexPathList.java】
            public Class DexPathList::findClass(String name, List<Throwable> suppressed) {
                
				//Class clazz = dex.loadClassBinaryName(name, definingContext, suppressed);
				public Class DexFile::loadClassBinaryName(String name, ClassLoader loader, List<Throwable> suppressed) {
                    
					//defineClass(name, loader, mCookie, this, suppressed);    其中:mCookie = openDexFile(fileName, null, 0, loader, elements);
                        //【*】openDexFile > DexFile_openDexFileNative >  runtime->GetOatFileManager().OpenDexFilesFromOat() 即从oat文件中找到指定的dex文件
                    //【java/dalvik/system/DexFile.java】
					private static Class DexFile::defineClass(String name, ClassLoader loader, Object cookie,  DexFile dexFile, List<Throwable> suppressed) {
						
                        //result = defineClassNative(name, loader, cookie, dexFile);
						//private static native Class DexFile::defineClassNative(String name, ClassLoader loader, Object cookie, DexFile dexFile)
						//【dalvik_system_DexFile.cc】
						static jclass DexFile_defineClassNative(JNIEnv* env,
															  jclass,
															  jstring javaName,
															  jobject javaLoader,
															  jobject cookie,
															  jobject dexFile) {
                                                                  
                            ClassLinker* class_linker = Runtime::Current()->GetClassLinker();                                      
                            class_linker->RegisterDexFile(*dex_file, class_loader.Get()){
                                ClassTable* table;
                                //给classLoader对象创建一个classTable对象
                                table = InsertClassTableForClassLoader(class_loader);
                                //将DexCache对象存入ClassTable的strong_roots_ 中 (std::vector<GcRoot<mirror::Object>>)
                                table->InsertStrongRoot(h_dex_cache.Get());
                            }                                
                                                                  
                            //【8.7.5 类加载入口】                                                
							mirror::Class* result = class_linker->DefineClass(soa.Self(),
																			  descriptor.c_str(),
																			  hash,
																			  class_loader,
																			  *dex_file,
																			  *dex_class_def);	


                            class_linker->InsertDexFileInToClassLoader(soa.Decode<mirror::Object*>(dexFile), class_loader.Get());                                                                            
                            
                        }
					}
				}
			}
		}
	}
}


//【8.7.5 类加载入口】
mirror::Class* ClassLinker::DefineClass(Thread* self,
                                        const char* descriptor,
                                        size_t hash,
                                        Handle<mirror::ClassLoader> class_loader,
                                        const DexFile& dex_file, 
                                        const DexFile::ClassDef& dex_class_def) {
    
    //注册 DexFile 对象，包含如下操作：
    //给classLoader对象创建一个classTable对象
    //将DexCache对象存入ClassTable的 strong_roots_ 中 (std::vector<GcRoot<mirror::Object>>)
    mirror::DexCache* dex_cache = RegisterDexFile(dex_file, class_loader.Get());
    
    //这个函数将把类的状态从kStatusNotReady切换为kStatusIdx
	//【8.7.2】  【Status】：kStatusIdx
	ClassLinker::SetupClass(dex_file, dex_class_def, klass, class_loader.Get());

    
    //插入ClassLoader对应的ClassTable的classes_中 (std::vector<ClassSet> classes_ GUARDED_BY(lock_);)
    //注意，不同的线程可以同时调用DefineClass来加载同一个类。这种线程同步直接的关系要处理好。
    mirror::Class* existing = InsertClass(descriptor, klass.Get(), hash);
	
	//[class_linker.cc->ClassLinker::LoadClass]
	//【8.7.3 类加载_相关函数1】 【Status】：kStatusLoaded
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
			klass->SetMethodsPtr(AllocArtMethodArray(self, allocator, it.NumDirectMethods() + it.NumVirtualMethods()), 
								it.NumDirectMethods(), 
								it.NumVirtualMethods()
				);
			
            //处理 direct 方法
			for (size_t i = 0; it.HasNextDirectMethod(); i++, it.Next()) {
				LoadMethod(self, dex_file, it, klass, method);
				//注意，oat_class 信息只在 LinkCode 中用到。LinkCode 留待10.1节介绍
                //【10.1.1】
				LinkCode(method, oat_class, class_def_method_index);
			}
			
			//处理 virtual方法。注意，对表示virtual方法的ArtMethod对象而言，
			//它们的 method_index_ 和 klass  methods_ 数组没有关系，也不在下面的循环中设置。
			for (size_t i = 0; it.HasNextVirtualMethod(); i++, it.Next()) {
				//和direct方法处理一样，唯一不同的是，此处不调用ArtMethod的 SetMethodIndex 函数，即不设置它的 method_index_ 成员
				LoadMethod(self, dex_file, it, klass, method);
                //【10.1.1】
				LinkCode(method, oat_class, class_def_method_index);
			}
											
		}
	}

	
	
	
	//接下来，如果目标类有基类或实现了接口类的话，我们相应地需要把它们“找到”
	//【8.7.3 类加载_相关函数2】 【Status】：kStatusLoaded
	bool ClassLinker::LoadSuperAndInterfaces(Handle<mirror::Class> klass,
										const DexFile& dex_file) {
		
		
		//寻找类，并保存到DexCache对象中
		//mirror::Class* super_class = ResolveType(dex_file, super_class_idx, klass.Get());
		//【8.7.8_ClassLinker中其他常用函数】
		mirror::Class* ClassLinker::ResolveType(const DexFile& dex_file,
                                                uint16_t type_idx,
                                                Handle<mirror::DexCache> dex_cache,
                                                Handle<mirror::ClassLoader> class_loader){	
										
			//resolved = FindClass(self, descriptor, class_loader);
			//【7.8.2和7.8.3  |  8.7.8_ClassLinker中其他常用函数】
			mirror::Class* ClassLinker::FindClass(Thread* self,
                                                  const char* descriptor, 
                                                  Handle<mirror::ClassLoader> class_loader) {
				//从ClassLoader对应的ClassTable中根据hash值搜索目标类						  
				//mirror::Class* klass = LookupClass(self, descriptor, hash, class_loader.Get());
				//【7.8.2和7.8.3】
				mirror::Class* ClassLinker::LookupClass(Thread* self,
                                                        const char* descriptor,
                                                        size_t hash,
                                                        mirror::ClassLoader* class_loader) {
                    //【A_7.8.1.4_ClassTable和ClassSet】
                    //mirror::Class* result = class_table->Lookup(descriptor, hash)
                    mirror::Class* ClassTable::Lookup(const char* descriptor, size_t hash){
                        //遍历所有的ClassSet
                        for (ClassSet& class_set : classes_) {
                            auto it = class_set.FindWithHash(descriptor, hash);
                            if (it != class_set.end()) {
                                return it->Read();
                            }
                        }
                        return nullptr;
                    }
					if (result != nullptr) {
						return result;
					}
					
					// Lookup failed but need to search dex_caches_.
					mirror::Class* result = LookupClassFromBootImage(descriptor);					
					if (result != nullptr) {
                        //result = InsertClass(descriptor, result, hash){
                        mirror::Class* ClassLinker::InsertClass(const char* descriptor, mirror::Class* klass, size_t hash){
                            class_table->InsertWithHash(klass, hash);
                        }
					 }
										
				}
				 
			}	
		}
										
	}
											
											
											
											
    //【8.7.4 链接类 LinkClass】 【Status】：kStatusResolving\kStatusResolved\kStatusRetired
	//[class_linker.cc->ClassLinker::LinkClass]
	//if (!LinkClass(self, descriptor, klass, interfaces, &h_new_class))
	bool ClassLinker::LinkClass(Thread* self,
								const char* descriptor,
								Handle<mirror::Class> klass, //待link的目标class
								Handle<mirror::ObjectArray<mirror::Class>> interfaces,
								MutableHandle<mirror::Class>* h_new_class_out) {
									
		//对该类所包含的方法（包括它实现的接口方法、继承自父类的方法等）进行处理
		//（更新目标类的iftable_、vtable_、相关隐含成员emedded_imf等信息）
		//【8.7.4.1 类链接_LinkMethods探讨】
		if (!LinkMethods(self, klass, interfaces, imt)) { 
			return false;
		}
		
		
		
		//下面两个函数分别对类的成员进行处理。
		//【8.7.4.2_类链接_LinkFields探讨】
		if (!LinkInstanceFields(self, klass)) { 
			return false;
		}
		size_t class_size;
		//尤其注意 LinkStaticFields 函数，它的返回值包括 class_size，代表该类所需内存大小。
		//【8.7.4.2_类链接_LinkFields探讨】
		if (!LinkStaticFields(self, klass, &class_size)) { 
			return false; 
		}								
	}											

}




//【8.7.7 类初始化】
//[class_linker.cc->ClassLinker::InitializeClass]
bool ClassLinker::InitializeClass(Thread* self, 
                                  Handle<mirror::Class> klass,
                                  bool can_init_statics, 
                                  bool can_init_parents) {
									  
	//VerifyClass(self, klass);
	//[class_linker.cc->ClassLinker::VerifyClass]
	//【8.7.6.3 类校验入口】
	void ClassLinker::VerifyClass(Thread* self, Handle<mirror::Class> klass,
									LogSeverity log_level) {
	    
		//【8.7.6.2 校验类】
		MethodVerifier::FailureKind MethodVerifier::VerifyClass(Thread* self,
                                                        mirror::Class* klass,
                                                        ......) {      //待校验的类由kclass表示
			

			//【8.7.6.1 校验方法】
			template <bool kDirect>
			MethodVerifier::FailureData MethodVerifier::VerifyMethods(Thread* self,
																	ClassLinker* linker,
																	const DexFile* dex_file,
																	const DexFile::ClassDef* class_def,
																	ClassDataItemIterator* it,
																	...) {
			}
		}
        
        ResolveClassExceptionHandlerTypes(klass){
            //【8.7.8_ClassLinker中其他常用函数】
            ResolveType()
        }
		
	}
    
    
    //【8.7.8_ClassLinker中其他常用函数】
    ArtField* field = ResolveField()
}




//dex2oat
//【9.1】
int main(int argc, char** argv) {
    //int result = art::dex2oat(argc, argv); //调用dex2oat函数
	static int dex2oat(int argc, char** argv) {
		TimingLogger timings("compiler", false, false);
		//MakeUnique：art中的辅助函数，用来创建一个由unique_ptr智能指针包裹的目标对象
		std::unique_ptr<Dex2Oat> dex2oat = MakeUnique<Dex2Oat>(&timings);
		
		//①解析参数
		//【9.2】
		dex2oat->ParseArgs(argc, argv){
			std::unique_ptr<ParserOptions> parser_options(new ParserOptions());
			
			//【9.2.1】
			compiler_options_.reset(new CompilerOptions());
			//【9.2.2】
			ProcessOptions(parser_options.get()); 
			//【9.2.3】
			InsertCompileOptions(argc, argv); 
			
		}
		
		.... //是否基于profile文件对热点函数进行编译。本书不讨论与之相关的内容
		
		//②打开输入文件，创建输出的.oat文件
		//OpenFile的目的比较简单，就是创建输出的.oat文件
		//【9.3】
		dex2oat->OpenFile(); 
		
		//③准备环境
		//【9.4】
		dex2oat->Setup(){
            //【9.4.1 part1】 
            /*
            在做类校验时，外界可以传递一个回调接口对象
                ·当类校验失败时，该【接口对象(callback)】的 ClassRejected 函数将被调用。
                ·当类的Java方法校验通过时，该【接口对象(callback)】的 MethodVerified 函数将被调用。
            */
            
            //【9.4.2 part2】
            /*
            【9.4.2.2】 
            OAT和ELF的关系
                ·一个命名为 oatdata 的symbol。参考4.2.2.3节的介绍可知，该符号的value字段标记一个虚拟内存的地址，
                                            它恰好是.rodata的起始位置。而该符号的size取值也正好与.rodata的size相等。
                
                ·一个命名为 oatexec 的symbol，它的value恰好是.text section在虚拟内存的起始位置。
                                            同样，oatexec symbol的size也与.text sectio的size相等。
                
                ·一个命名为 oatlastword 的symbol，它是ELF文件中oat内容结束的位置。
                                            注意，由于oatlastword symbol本身还占据了四个字节，
                                            所以它的起始位置为oatexec起始位置 +(oatexec size)-4个字节。


            CreateOatWriters();   //CreateOatWriters 将创建 ElfWriter 和 OatWriter 对象
                                  //ElfWriter(构造段:Section) 
                                  //OatWriter(读取dex文件，写入封装的oat信息)对象
                                  
                                  
                                  
            AddDexFileSources();  //打开并读取输入文件(.jar包中的dex文件)，然后填充到对应的字段：
                                      zipped_dex_files_ 成员变量存储jar包中对应的dex项
                                      zipped_dex_file_locations_ 用于存储dex项的路径信息。
                                      oat_dex_files_ 是vector数组，元素类型为OatDexFile(保存了一个dex项的路径和该dex项的源)
                                      zip_archives_ 类型为vector<unique_ptr<ZipArchive>>，其元素的类型为 ZipArchive，代表一个Zip归档对象，即jar文件
            */
            
            //【9.4.3 part3】
            /*
                WriteAndOpenDexFiles;  //为boot.oat文件对应的OatWriter准备数据， 如将OatDexFile信息写入oat文件的OatDexFile区域
                
            */
            
            //【9.4.4 part4】
            /*
                创建art runtime对象，没有执行它的Start函数。所以，该runtime对象也叫unstarted runtime
            */
        }

		bool result;
		//镜像有boot image和app image两大类，镜像文件是指.art文件
		if (dex2oat->IsImage()) {
			
			//④编译boot镜像或app镜像
			//【9.5】
			result = CompileImage(*dex2oat){
				
				//①编译    
                //【9.5.1】 Compile函数的目标就是编译上文所打开的那些Dex文件中的Java方法
				dex2oat.Compile(){
					//创建一个CompilerDriver实例
					driver_.reset(new CompilerDriver(compiler_options_.get(),  ......));
					//调用CompileAll进行编译
					driver_->CompileAll(class_loader_, dex_files_, timings_){
						//
						driver_->PreCompile(class_loader, dex_files, timings){
                            //LoadImageClasses 函数的主要工作是遍历 image_classes_ 中的类，然后通过ClassLinker的FindSystemClass进行加载
                            LoadImageClasses(timings);
                            
                            //遍历dex文件，校验其中的类
                            Verify(class_loader, dex_files, timings);  
                            //遍历dex文件，确保类的初始化
                            InitializeClasses(class_loader, dex_files, timings);
                            //遍历 image_classes_ 中的类，检查类的引用型成员变量，将这些变量对应的Class对象也加到image_classes_容器中。 
                            UpdateImageClasses(timings);
                        }
						
						//
						driver_->Compile(class_loader, dex_files, timings){
							driver_->CompileDexFile(class_loader,*dex_file,dex_files,......){
								//编译入口：构造函数
								CompileClassVisitor visitor(&context){
									//dex_to_dex_compilation_level 作为下面的参数
									optimizer::DexToDexCompilationLevel dex_to_dex_compilation_level = GetDexToDexCompilationLevel(soa.Self(), *driver, jclass_loader, dex_file, class_def);
									
									CompileMethod(soa.Self(), driver, it.GetMethodCodeItem(), it.GetMethodAccessFlags(),it.GetMethodInvokeType(class_def),
												  class_def_index, method_idx, jclass_loader, dex_file, dex_to_dex_compilation_level,compilation_enabled, dex_cache){
										//【9.5.2】 dex到dex优化的入口函数为ArtCompileDEX。下文将介绍它
										//if
										compiled_method = optimizer::ArtCompileDEX(driver,code_item,access_flags,invoke_type,class_def_idx,method_idx,
																				class_loader,dex_file,(verified_method != nullptr)
																					? dex_to_dex_compilation_level
																					: optimizer::DexToDexCompilationLevel::kRequired);
										
										//【9.5.3】 native标记的函数将调用JniCompile进行编译
										//else if
										compiled_method = driver->GetCompiler()->JniCompile(access_flags, method_idx, dex_file);
										
                                        
                                        //【9.5.4】dex到机器码的编译优化将由Optimizing的Compile来完成
                                        //else
                                        compiled_method = driver->GetCompiler()->Compile(code_item, access_flags, invoke_type,class_def_idx, method_idx,
                                                                                         class_loader,dex_file, dex_cache);
										
										/*注意，不管最终进行的是dex到dex编译、jni编译还是dex到机器码的编译，其返回结果都由一个 CompiledMethod 对象表示。
											下面将对这个编译结果进行处理。如果该结果不为空，则将其存储到driver中去以做后续的处理。
										*/
										driver->AddCompiledMethod(method_ref, compiled_method, non_relative_linker_patch_count);
									}
								}
							}
						}
					}
				}
				
				
				if(!dex2oat.WriteOatFiles()){
					//②输出.oat文件
					......
				};  
				
				
				//③处理.art文件
				if (!dex2oat.HandleImage()) {
					......
				}
			}
			
		} else {
			//编译app，但不生成art文件（即镜像文件）。其内容和CompileImage差不多，只是少了生成.art文件的步骤。
			result = CompileApp(*dex2oat);
		}
		dex2oat->Shutdown(); //清理工作
		return result;
	}
    return result;
}



/*
jni_entrypoints_x86.S
quick_entrypoints_x86.S

http://aospxref.com/android-7.0.0_r7/xref/art/runtime/arch/arm64/jni_entrypoints_arm64.S
http://aospxref.com/android-7.0.0_r7/xref/art/runtime/arch/arm64/quick_entrypoints_arm64.S

非JNI方法
    有机器码：机器码执行模式
    无机器码：解释执行模式

JNI方法
    有机器码：机器码入口为(一串汇编代码，本身会跳转到JNI机器码入口地址)
                => jni机器码入口
                    > 已注册：为 Native层函数
                    > 未注册：为 art_jni_dlsym_lookup_stub
                        1.bl artFindNativeMethod(注册Native层函数；返回Native层函数)
                        2.执行【Native层函数】
                    
              
    无机器码：机器码入口为(art_quick_generic_jni_trampoline)
                    1.bl artQuickGenericJniTrampoline(寻找并返回Native层函数地址，同时未注册的情况下还进行注册)
                    2.执行【Native层函数】
    
    【所以】：
        有机器码：机器码入口 -> JNI机器码入口 -> Native层函数地址
        无机器码：机器码入口 -> Native层函数地址
        
*/


//【10.2 解释执行】
//一个方法如果采取解释执行的话，其ArtMethod对象的机器码入口将指向一段跳转代码——art_quick_to_interpreter_bridge
//【10.2.2】
//art_quick_to_interpreter_bridge

//[quick_trampoline_entrypoints.cc->artQuickToInterpreterBridge]
extern "C" uint64_t artQuickToInterpreterBridge (ArtMethod* method,
                                                Thread* self, 
                                                ArtMethod** sp) {

    //【10.2.3】解释执行的入口函数
    //[interpreter.h->EnterInterpreterFromEntryPoint]
    //[interpreter.cc->EnterInterpreterFromEntryPoint]
    result = interpreter::EnterInterpreterFromEntryPoint(self, code_item, shadow_frame){
        
        //下面这段代码和JIT有关，相关知识见本章后续对JIT的介绍
        jit::Jit* jit = Runtime::Current()->GetJit();
        if (jit != nullptr) {
            jit->NotifyCompiledCodeToInterpreterTransition(self, shadow_frame->GetMethod());
        }
        
        //【*】关键函数
        ////[interpreter.cc->Execute]
        return Execute(self, code_item, *shadow_frame, JValue()){
            //(1)采用对应汇编语言编写的
            bool returned = ExecuteMterpImpl(self, code_item, &shadow_frame, &result_register);
            if (returned) {
                return result_register;
            }
            
            //(2)由C++编写，基于switch/case逻辑实现
            return ExecuteSwitchImpl<false, false>(self, code_item, shadow_frame, result_register, false){
                //【*】invoke-direct指令码的处理
                case Instruction::INVOKE_DIRECT: { 
                    bool success = DoInvoke<kDirect, false, do_access_check>(self, shadow_frame, inst, inst_data, &result_register){
                        //下面这段代码和JIT有关，我们留待后续章节再来介绍。
                        jit::Jit* jit = Runtime::Current()->GetJit();
                        if (jit != nullptr) {
                            ......
                        }
                        
                        //【*】instrumentation的处理
                        return DoCall<is_range, do_access_check>(called_method, self, shadow_frame, inst, inst_data,result){
                            //调用DoCallCommon，我们接着看这个函数
                            return DoCallCommon<is_range, do_assignability_check>( called_method, self, shadow_frame,result, number_of_inputs, arg, vregC){
                                ArtMethod* target = new_shadow_frame->GetMethod();
                                
                                //【*】如果处于调试模式，或者方法C不存在机器码，则调用 ArtInterpreterToInterpreterBridge 函数，显然，它是解释执行的继续。
                                if (ClassLinker::ShouldUseInterpreterEntrypoint(target, target->GetEntryPointFromQuickCompiledCode())) {
                                    ArtInterpreterToInterpreterBridge(self, code_item, new_shadow_frame,result){
                                        //内部继续调用 Execute()
                                    }
                                } 
                                else {
                                    //【*】如果可以用机器码方式执行方法C，则调用 ArtInterpreterToCompiledCodeBridge，它将从解释执行模式进入机器码执行模式。
                                    ArtInterpreterToCompiledCodeBridge(self, shadow_frame.GetMethod(), code_item,new_shadow_frame, result){
                                        //void ArtMethod::Invoke()
                                        method->Invoke(){
                                            //if
                                            art_quick_invoke_stub()
                                            //else
                                            art_quick_invoke_static_stub()
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
            }
            
            //(3)由C++编写，基于goto逻辑实现
            return ExecuteGotoImpl<false, false>(self, code_item, shadow_frame,result_register);
        }
    }
}