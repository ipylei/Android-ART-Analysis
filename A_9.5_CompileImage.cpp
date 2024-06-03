//[dex2oat.cc->Dex2Oat::CompileImage]
static int CompileImage(Dex2Oat& dex2oat) {
    //加载profile文件，对基于profile文件的编译有效，本例不涉及它
    dex2oat.LoadClassProfileDescriptors();
	
	//①编译
    dex2oat.Compile(); 
	
    if(!dex2oat.WriteOatFiles()){
		//②输出.oat文件
		......
	};   
	
    .....
	
    //③处理.art文件
    if (!dex2oat.HandleImage()) {
			......
	}
    .....　//其他处理，内容非常简单。感兴趣的读者可自行阅读
	
    return EXIT_SUCCESS;
}



//[dex2oat.cc->Dex2Oat::Compile]
void Compile() {
    .... //略去次要内容
    //创建一个CompilerDriver对象
    driver_.reset(new CompilerDriver(compiler_options_.get(),  ......));
    driver_->SetDexFilesForOatFile(dex_files_);
	
    //调用CompileAll进行编译
    driver_->CompileAll(class_loader_, dex_files_, timings_);
}



//[compiler_driver.h->CompilerDriver::CompilerDriver]
CompilerDriver::CompilerDriver(                  //输入参数比较多
								const CompilerOptions* compiler_options,
								VerificationResults* verification_results,
								DexFileToMethodInlinerMap* method_inliner_map,
								Compiler::Kind compiler_kind,InstructionSet instruction_set,
								const InstructionSetFeatures* instruction_set_features,
								bool boot_image,bool app_image,
								std::unordered_set<std::string>* image_classes,
								std::unordered_set<std::string>* compiled_classes,
								std::unordered_set<std::string>* compiled_methods,
								size_t thread_count,......)
								
								: 
								compiler_options_(compiler_options),      //编译选项
								//存储校验结果，读者可回顾图9-6
								verification_results_(verification_results),
								method_inliner_map_(method_inliner_map),
								//创建一个OptimizingCompiler对象
								compiler_(Compiler::Create(this, compiler_kind)),
								//枚举变量，本例取默认值kOptimizing
								compiler_kind_(compiler_kind),
								instruction_set_(instruction_set),
								instruction_set_features_(instruction_set_features),
								.......
								/*compiled_methods_类型为MethodTable，它是数据类型的别名，其真实类型为SafeMap
								  <const MethodReference, CompiledMethod*, MethodReferenceComparator>，
								 代表一个map容器，key为MethodReference（代表一个Java方法），value指向Java方
								  法编译的结果对象。MethodReferenceComparator为map容器用到的比较器，读者可不
								  用理会它 */
								compiled_methods_(MethodTable::key_compare()),
								non_relative_linker_patch_count_(0u),
								//在本例中，boot_image_为true
								boot_image_(boot_image),app_image_(app_image),
								//字符串集合，在本例中，其内容来自/system/etc/preloaded-classes
								image_classes_(image_classes),
								//字符串集合，在本例中，其内容来自/system/etc/compiled-classes-phone
								classes_to_compile_(compiled_classes),
								//字符串集合，保存需要编译的Java方法名，在本例中，该变量取值为nullptr
								methods_to_compile_(compiled_methods),
								......
								support_boot_image_fixup_(instruction_set != kMips && instruction_set != kMips64),
																  //在本例中，该变量取值为true
								//指向一个vector<const DexFile*>数组，存储dex文件对象
								dex_files_for_oat_file_(nullptr),
								//类型为CompiledMethodStrorage。在本例中，swap_fd取值为-1。
								compiled_method_storage_(swap_fd),
								......
								/*类型为vector<DexFileMethodSet>，DexFileMethodSet类型见下文介绍*/
								dex_to_dex_references_(),
								//指向一个BitVector对象，位图。其作用见下文分析
								current_dex_to_dex_methods_(nullptr) {
			
		compiler_->Init(); //Init函数很简单，请读者自行阅读
    ......
}




//[compiler_driver.cc->CompilerDriver::DexFileMethodSet]
class CompilerDriver::DexFileMethodSet {
    public:
        ...... //略过成员函数
    private:
        const DexFile& dex_file_;      //指向一个Dex文件对象

		//位图对象，其第n位对应 dex_file_ 里method_ids的第n个元素
        BitVector method_indexes_;
};







//[compiler_driver.cc->CompilerDriver::CompileAll]
void CompilerDriver::CompileAll(jobject class_loader, const std::vector<const DexFile*>& dex_files, TimingLogger* timings) {
    //创建线程池。这部分内容非常简单，笔者不拟介绍它们
    InitializeThreadPools();
	
    //重点来看PreCompile和Compile两个关键函数
    PreCompile(class_loader, dex_files, timings);
	
    if (!GetCompilerOptions().VerifyAtRuntime()) {
        Compile(class_loader, dex_files, timings);
    }
    .....
    FreeThreadPools();
}




//[compiler_driver.cc->CompilerDriver::PreCompile]
void CompilerDriver::PreCompile(jobject class_loader, const std::vector<const DexFile*>& dex_files,....) {
    /*注意参数。本例中，class_loader为nullptr，dex_files为13个jar包所包含的dex项。下面的
      LoadImageClasses 函数的主要工作是遍历 image_classes_ 中的类，然后通过ClassLinker的
      FindSystemClass进行加载。另外，还要检查Java方法所抛出的异常（如果有抛出异常的话）对应的类型是否存在  
	 */
    LoadImageClasses(timings);

    const bool verification_enabled = compiler_options_->IsVerificationEnabled();
    const bool never_verify = ...;

	const bool verify_only_profile = ...;
    //本例中，if条件满足
    if ((never_verify || verification_enabled) && !verify_only_profile) {
        /*下面的Resolve函数主要工作为遍历dex文件，然后：
          （1）解析其中的类型，即遍历dex文件里的type_ids数组。内部将调用ClassLinker的ResolveType函数。
          （2）解析dex里的类、成员变量、成员函数。内部将调用ClassLinker的ResolveType、ResolveField和ResolveMethod等函数。
          读者可回顾8.7.8.1节的内容。 */
        Resolve(class_loader, dex_files, timings);
    }
    .....
    /*下面三个函数的作用：
      （1）Verify：遍历dex文件，校验其中的类。校验结果通过QuickCompilationCallback存储在CompilerDriver的verification_results_中。
      （2）InitializeClasses：遍历dex文件，确保类的初始化。
      （3）UpdateImageClasses：遍历image_classes_中的类，检查类的引用型成员变量，将这些
       变量对应的Class对象也加到image_classes_容器中。 
	*/
    Verify(class_loader, dex_files, timings);
    InitializeClasses(class_loader, dex_files, timings);
    UpdateImageClasses(timings);
}




//[compiler_driver.cc->CompilerDriver::Compile]
void CompilerDriver::Compile(jobject class_loader, const std::vector<const DexFile*>& dex_files, TimingLogger* timings) {
    //遍历dex文件，调用CompileDexFile进行编译
    for (const DexFile* dex_file : dex_files) {
		//首先遍历需要编译的Dex文件对象。针对每一个Dex文件对象，调用CompileDexFile。
		//在这一轮的CompileDexFile中，那些只能做dex到dex优化的Java方法以及对应的Dex文件对象将保存到CompilerDriver的 dex_to_dex_references_ 容器中
        CompileDexFile(class_loader,*dex_file,dex_files,......);
        ......
    }
    /*有一些Java方法不能编译成机器码，只能做dex到dex的优化。下面将针对这些Java方法进行优化。
      dex_to_dex_references变量的类型是ArrayRef<DexFileMethodSet>。ArrayRef是一个数组，而 DexFileMethodSet 的内容我们在上文已经见过了。
	  它的method_indexes_位图对象保存了一个Dex文件里需要进行dex到dex优化的Java方法的method_id。  
	*/
    ArrayRef<DexFileMethodSet> dex_to_dex_references;
    {
        MutexLock lock(Thread::Current(), dex_to_dex_references_lock_);
        //将CompilerDriver的 dex_to_dex_references_ 成员变量赋值给 dex_to_dex_references
        dex_to_dex_references = ArrayRef<DexFileMethodSet>(dex_to_dex_references_);
    }
    //遍历dex_to_dex_references里的对象
    for (const auto& method_set : dex_to_dex_references) {
        current_dex_to_dex_methods_ = &method_set.GetMethodIndexes();
        //调用CompileDexFile函数
		//接下来遍历dex_to_dex_references_容器，再次调用CompileDexFile方法，对其中所包含的Java方法进行dex到dex优化
        CompileDexFile(class_loader,  method_set.GetDexFile(), dex_files,......);
    }
    current_dex_to_dex_methods_ = nullptr;
}



//[compiler_driver.cc->CompilerDriver::CompileDexFile]
void CompilerDriver::CompileDexFile(jobject class_loader, 
									const DexFile& dex_file, 
									const std::vector<const DexFile*>& dex_files, 
									ThreadPool* thread_pool, 
									size_t thread_count, 
									TimingLogger* timings) {
    TimingLogger::ScopedTiming t("Compile Dex File", timings);
    /* ParallelCompilationManager 为并行编译管理器，它通过往线程池（由thread_pool表示）添加
      多个编译任务来实现。具体的编译任务将由线程池中的线程来执行。 */
    ParallelCompilationManager context(Runtime::Current()->GetClassLinker(), class_loader, this, &dex_file, dex_files, thread_pool);
    
	//我们需要重点关注 CompileClassVisitor
    CompileClassVisitor visitor(&context);
	
	/*context.ForAll将触发线程池进行编译工作。注意，编译是以类为单位进行处理的，每一个待编译
      的类都会交由CompileClassVisitor的Visit函数进行处理。
	 */
    context.ForAll(0, dex_file.NumClassDefs(), &visitor, thread_count);
}



//[compiler_driver.cc->CompileClassVisitor类]
class CompileClassVisitor : public CompilationVisitor {
    public:
        explicit CompileClassVisitor(const ParallelCompilationManager* manager) : manager_(manager) {}
        //编译时，编译线程将调用下面的这个Visit函数，参数为待处理类在dex文件里class_ids数组中的索引
        virtual void Visit(size_t class_def_index) ..... {
			//找到dex文件对象
			const DexFile& dex_file = *manager_->GetDexFile();
			//根据class_def_index索引号找到目标类对应的ClassDef信息
			const DexFile::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
			......
			/*DexToDexCompilationLevel 是一个枚举变量，详情见下文对 GetDexToDexCompilationLevel 函数的介绍   */
			optimizer::DexToDexCompilationLevel dex_to_dex_compilation_level = GetDexToDexCompilationLevel(soa.Self(), *driver, jclass_loader, dex_file, class_def);
			//迭代器，用于遍历类中的信息
			ClassDataItemIterator it(dex_file, class_data);
			.... //略过成员变量
			/*检查类名是否包含在CompileDriver的classes_to_compile_容器中，如果不在，则设置
			  compilation_enabled为false，否则为true */
			bool compilation_enabled = driver->IsClassToCompile(dex_file.StringByTypeIdx(class_def.class_idx_));

			//遍历direct的Java方法
			int64_t previous_direct_method_idx = -1;
			while (it.HasNextDirectMethod()) {
				uint32_t method_idx = it.GetMemberIndex();
			.....
				previous_direct_method_idx = method_idx;
				CompileMethod(soa.Self(), driver, it.GetMethodCodeItem(),
							  it.GetMethodAccessFlags(),it.GetMethodInvokeType(class_def),
							class_def_index, method_idx, jclass_loader, dex_file,
							dex_to_dex_compilation_level,compilation_enabled,
							dex_cache);
				it.Next();
			}
			//编译虚函数，也是调用 CompileMethod 函数
			.......
    }...
};




//[dex_to_dex_compiler.h->DexToDexCompilationLevel]
enum class DexToDexCompilationLevel {
    kDontDexToDexCompile,           //不做编译优化
    kRequired,                      //进行dex到dex的编译优化
    kOptimize                       //做dex到本地机器码的编译优化
};


//[compiler_driver.cc->GetDexToDexCompilationLevel]
static optimizer::DexToDexCompilationLevel GetDexToDexCompilationLevel(Thread* self, const CompilerDriver& driver,
																		Handle<mirror::ClassLoader> class_loader,
																		const DexFile& dex_file, const DexFile::ClassDef& class_def) {
    //注意参数：dex_file代表一个Dex文件对象，class_def代表待处理的类
    auto* const runtime = Runtime::Current();
    //如果runtime使用jit编译或者编译选项为kVerifyAtRuntime，则返回kDontDexToDexCompile。即无需编译
    if (runtime->UseJitCompilation() || driver.GetCompilerOptions().VerifyAtRuntime()) {
		return optimizer::DexToDexCompilationLevel::kDontDexToDexCompile;
    }
	
    const char* descriptor = dex_file.GetClassDescriptor(class_def);
    ClassLinker* class_linker = runtime->GetClassLinker();
    mirror::Class* klass = class_linker->FindClass(self, descriptor, class_loader);
    if (klass == nullptr) { //如果ClassLinker中无法找到目标类，也无需编译
        ......
        return optimizer::DexToDexCompilationLevel::kDontDexToDexCompile;
    }
	
	//如果这个类已经校验通过，则可以做dex到机器码的优化
    if (klass->IsVerified()) { 
        return optimizer::DexToDexCompilationLevel::kOptimize;
    } 
	//如果编译时校验失败，但可以在运行时再次校验，则可以做dex到dex的优化
	else if (klass->IsCompileTimeVerified()) {
        return optimizer::DexToDexCompilationLevel::kRequired;
    } 
	//其他情况下，将不允许做编译优化
	else { 
        return optimizer::DexToDexCompilationLevel::kDontDexToDexCompile;
    }
}




//[compiler_driver.cc->CompileMethod]
static void CompileMethod(Thread* self, CompilerDriver* driver, const DexFile::CodeItem* code_item,.......) {
    //存储编译的结果，类型为CompiledMethod
    CompiledMethod* compiled_method = nullptr;
    MethodReference method_ref(&dex_file, method_idx);
	
	
    //针对dex到dex的编译优化
    if (driver->GetCurrentDexToDexMethods() != nullptr) {
        //如果该方法被标记为只能做dex到dex的编译优化，则进行下面的处理。
        if (driver->GetCurrentDexToDexMethods()->IsBitSet(method_idx)) {
            const VerifiedMethod* verified_method = driver->GetVerificationResults()->GetVerifiedMethod(method_ref);
            //dex到dex优化的入口函数为ArtCompileDEX。下文将介绍它
			compiled_method = optimizer::ArtCompileDEX(driver,code_item,access_flags,invoke_type,class_def_idx,method_idx,
													class_loader,dex_file,(verified_method != nullptr)
														? dex_to_dex_compilation_level
														: optimizer::DexToDexCompilationLevel::kRequired);
        }
    } 
	//针对jni函数的编译
	else if ((access_flags & kAccNative) != 0) {
 
        if (!driver->GetCompilerOptions().IsJniCompilationEnabled() && InstructionSetHasGenericJniStub(driver->GetInstructionSet())) {
        
		} 
		else {//对本例而言，native标记的函数将调用JniCompile进行编译
            /*jni函数的编译入口函数为JniCompile。GetCompiler将返回OptimizingCompiler。
              所以，对dex2oat来说，OptimizingCompiler的JniCompile函数将被调用。下文将
              单独用一节来介绍它  */
            compiled_method = driver->GetCompiler()->JniCompile(access_flags, method_idx, dex_file);
        }
    } 
	//abstract函数无需编译
	else if ((access_flags & kAccAbstract) != 0) {
        
    } 
	else {
        const VerifiedMethod* verified_method = driver->GetVerificationResults()->GetVerifiedMethod(method_ref);
        ....
        if (compile) {
            /*dex到机器码的编译优化将由Optimizing的Compile来完成。该函数返回一个CompiledMethod对象。注意，如果一个Java方法不能做dex到机器码优化的话，该函数将返回
              nullptr。   
			*/
            compiled_method = driver->GetCompiler()->Compile(code_item,
                        access_flags, invoke_type,class_def_idx, method_idx,
                        class_loader,dex_file, dex_cache);
        }
        /*如果Compile返回nullptr，并且前面调用GetDexToDexCompilationLevel返回结果不为
          kDontDexToDexCompile，则需要对该方法进行标记以在后续尝试Dex到Dex优化。 */
        if (compiled_method == nullptr && dex_to_dex_compilation_level != optimizer::DexToDexCompilationLevel::kDontDexToDexCompile) {
            driver->MarkForDexToDexCompilation(self, method_ref);
		}
    }
	
    /*注意，不管最终进行的是dex到dex编译、jni编译还是dex到机器码的编译，其返回结果都由一个 CompiledMethod 对象表示。
	下面将对这个编译结果进行处理。如果该结果不为空，则将其存储到driver中去以做后续的处理。*/
    if (compiled_method != nullptr) {
        size_t non_relative_linker_patch_count = 0u;
        for (const LinkerPatch& patch : compiled_method->GetPatches()) {
            if (!patch.IsPcRelative()) {
                ++non_relative_linker_patch_count;
            }  
		}
        bool compile_pic = driver->GetCompilerOptions().GetCompilePic();
        driver->AddCompiledMethod(method_ref, compiled_method, non_relative_linker_patch_count);
    } ......
}




//[compiled_method.h->LinkerPatch类]
class LinkerPatch {
    public:
        enum class Type : uint8_t { //枚举变量，定义了目标地址计算方式的种类
            kRecordPosition, //用于记录，不和目标地址绑定，用于patchoat程序
            kMethod, //绝对地址，但只在arm cpu上使用
            kCall, //绝对地址
            kCallRelative, //相对地址
            /*下面两种方式和调用Java String类提供的函数有关。其中，kString为函数的绝对地址，而kStringRelative为函数的相对地址。 */
            kString, 
			kStringRelative,
            //相对地址，需结合DexCache里resolved_methods_数组里的信息计算最终地址
            kDexCacheArray,
		};
    .....
    private:
		const DexFile* target_dex_file_; //目标Dex文件
		/*目标地址的偏移量，结合下面patch_type_的情况可计算出最终的地址值。注意，literal_offset_和
		  patch_type_两个成员变量一共占据32个比特位，literal_offset_使用前24位，patch_type_
			使用后8位。     
		*/
		uint32_t literal_offset_:24;
		Type patch_type_:8; //跳转类型
};