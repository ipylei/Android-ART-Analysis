//【9.2】　ParseArgs介绍
//现在来看ParseArgs的代码，如下所示。
//[dex2oat.cc->ParseArgs]
void Dex2Oat::ParseArgs(int argc, char** argv) {
    ....
    std::unique_ptr<ParserOptions> parser_options(new ParserOptions());
	
    //compiler_options_ 是 dex2oat 的成员变量，指向一个ComilerOptions 对象，它用于存储和编译相关的选项。下文将介绍CompilerOptions。
    //【9.2.1】
	compiler_options_.reset(new CompilerOptions());
	
    for (int i = 0; i < argc; i++) {
        const StringPiece option(argv[i]);
		if (option.starts_with("--dex-file=")) { //处理--dex-file选项
			dex_filenames_.push_back(option.substr(strlen("--dex-file=")).data());
		} 
		else if {....}  //其他选项
		else if (option == "--runtime-arg") {
			runtime_args_.push_back(argv[i]);
		} 
		else if (option.starts_with("--image=")) {
			image_filenames_.push_back(option.substr(strlen("--image=")).data());
		} 
		else if {......}  //其他选项
		else if (option.starts_with("--compiled-classes=")) {
			compiled_classes_filename_ = option.substr(strlen("--compiled-classes=")).data();
		} 
		else if {.......}  //其他选项
		else if (!compiler_options_->ParseCompilerOption(option, Usage)) {
			......
		}
    }
	//【9.2.2】
    ProcessOptions(parser_options.get()); //见下文介绍
    //【9.2.3】
	InsertCompileOptions(argc, argv); //见下文介绍
}



//【9.2.1】[compiler_options.cc->CompilerOptions默认构造函数]
CompilerOptions::CompilerOptions()
		: /*compiler_filter_ 类型为Filter（枚举变量），和图9-1所示的编译过滤器对应。
		  kDefaultCompilerFilter是默认设置，值为CompilerFilter::kSpeed。 */
		  compiler_filter_(kDefaultCompilerFilter),
		  
			/*根据一个Java方法对应dex字节码数量的多少（由第3章图3-8 code_item 中 insns_size 描述），
			可将其分为huge、large、small和tiny四类。其中，
				huge方法包含10000（kDefaultHuge-MethodThreshold）个dex字节码、
				large方法对应为6000（kDefaultLargeMethodThreshold）、
				small方法为60（kDefaultSmallMethodThreshold）、
				tiny方法为20（kDefaultTinyMethodThreshold）。
			编译时，huge方法和large方法可能会被略过。
			*/
			huge_method_threshold_(kDefaultHugeMethodThreshold),
			large_method_threshold_(kDefaultLargeMethodThreshold),
			small_method_threshold_(kDefaultSmallMethodThreshold),
			tiny_method_threshold_(kDefaultTinyMethodThreshold),
			
			......
			
			//下面两个成员变量和编译内联方法有关，默认值都是-1
			inline_depth_limit_(kUnsetInlineDepthLimit),
			inline_max_code_units_(kUnsetInlineMaxCodeUnits),
			
			//下面这个成员变量默认值为false
			include_patch_information_(kDefaultIncludePatchInformation),
			
			......
			//下面这三个成员变量和7.2.2节中提到的隐式空指针检查、堆栈溢出检查、隐式线程挂起检查有
			//关。注意，除隐式线程挂起检查的参数默认为false外，其他两种检查的参数默认都是true。
			implicit_null_checks_(true), 
			implicit_so_checks_(true),
			implicit_suspend_checks_(false),
			
			/*编译为PIC（Position Indepent Code）。读者可回顾4.2.4.5节的内容。  */
			compile_pic_(false),
			
			......
			
			force_determinism_(false) {
			
			
			
}





//【9.2.2】　ProcessOptions函数介绍
//ProcessOptions代码如下所示。[dex2oat.cc->ProcessOptions]
void ProcessOptions(ParserOptions* parser_options) {
	
    //image_filenames_ 不为空，所以 boot_image_ 为true，表示此次是boot image的编译
    boot_image_ = !image_filenames_.empty();
    app_image_ = app_image_fd_ != -1 || !app_image_file_name_.empty();
	
	//编译boot image时，编译选项需要设置debuggable_为true
    if (IsBootImage()) { 
        compiler_options_->debuggable_ = true;
    }
	
    ..... //对dex2oat命令行选项进行检查
	
    /* dex_locations_ 是dex2oat的成员变量，类型为vector<const char*>。可通过命令行选项
       --dex-location 指定。本例中没有使用该选项，
	   所以它的值来源于 dex_file_names_（选项由--dex-file选项指定） 
	 */
    if (dex_locations_.empty()) {      
		//本例没有传入--dex-location命令行选项，所以，dex_locations_ 的取值和 dex_file_names_ 一样。
        for (const char* dex_file_name : dex_filenames_) {
            dex_locations_.push_back(dex_file_name);
        }
    }
    ......

    switch (instruction_set_) {
        ...
        case kX86:
        .....
        //设置compiler_options_的成员变量，主要和隐式检查有关
        compiler_options_->implicit_null_checks_ = true;
        compiler_options_->implicit_so_checks_ = true;
        break;
        default:         break;
    }
    ....
    /*表9-1的最后介绍过 key_value_store_，它也是Dex2Oat的成员变量，类型为SafeMap。SafeMap
      是和STL map类似的容器类。ART源码中有大量自定义的容器类。笔者建议读者暂时不要研究ART里
	  这些自定义容器类，先将其当作STL里对应容器来看待。下面这行代码将初始化key_value_store_
      容器，其内容将在后续代码中逐步填入。
	 */
    key_value_store_.reset(new SafeMap<std::string, std::string>());
	
    ......
	
}


//【9.2.3】　InsertCompileOptions函数介绍
//[dex2oat.cc->Dex2Oat::InsertCompileOptions]
void InsertCompileOptions(int argc, char** argv) {
    std::ostringstream oss;
    for (int i = 0; i < argc; ++i) {
        if (i > 0) { 
			oss << ' '; 
		}
        oss << argv[i];
    }
    /*填充 key_value_store_ 的内容，OatHeader 代表Oat文件头结构。下文将详细介绍它，
	kDex2OatCmdLineKey 为键名，取值为"dex2oat-cmdline"。  
	 */
    key_value_store_->Put(OatHeader::kDex2OatCmdLineKey, oss.str());
    oss.str(""); // Reset.
    oss << kRuntimeISA;
    //设置host编译时键值对的内容，对本例而言，该值不存在
    key_value_store_->Put(OatHeader::kDex2OatHostKey, oss.str());
    //设置pic键值对的内容
    key_value_store_->Put(OatHeader::kPicKey,
						 compiler_options_->compile_pic_ ? 
								OatHeader::kTrueValue 
								: 
								OatHeader::kFalseValue);
    .... //设置其他键值对内容;
}