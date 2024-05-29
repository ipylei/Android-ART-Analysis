//[dex2oat.cc->Dex2Oat::Setup第二段]
{
	.... //第二段代码
    
	RuntimeArgumentMap runtime_options;
    
	/*为创建编译时使用的Runtime对象准备参数。由上节内容可知，dex2oat用得不是完整虚拟机。另外，
	  dex2oat中的这个runtime只有Init会被调用，而它的Start函数不会被调用。所以，dex2oat里
	  用到的这个虚拟机也叫unstarted runtime  
	*/
	if (!PrepareRuntimeOptions(&runtime_options)) { 
		return false; 
	}
	
	//CreateOatWriters将创建 ElfWriter 和 OatWriter 对象
	CreateOatWriters();       //①关键函数，见下文介绍
	
	
	AddDexFileSources();      //②关键函数，见下文介绍
    
	if (IsBootImage() && image_filenames_.size() > 1) {
		...... //multi_image_ 情况的处理
	}
}


//【9.4.2.1】 关键类介绍
/*【9.4.2.1.1】　ElfWriter 和 ElfBuilder
ElfWriter 是ART里用于往ELF文件中写入相关信息的工具类，其类家族如图9-7所示。
·ElfWriter本身是一个虚基类，定义了一些用于操作ELF文件（主要是往ELF文件里写入数据）的函数。
·ART里ElfWriter的实现类是 ElfWriterQuick。注意，它是一个模板类。
    对于32位ELF文件，它将使用ElfType32作为模板参数，
    而对于4位ELF文件，则使用ElfType64作为模板参数。
*/


//在dex2oat中，ElfWriterQuick 对象是由 CreateElfWriterQuick 函数创建的，代码如下所示。
//[elf_writer_quick.cc->CreateElfWriterQuick]
std::unique_ptr<ElfWriter> CreateElfWriterQuick(InstructionSet instruction_set,
                                                const InstructionSetFeatures* features,
                                                const CompilerOptions* compiler_options, File* elf_file) {
    if (Is64BitInstructionSet(instruction_set)) {
        //64位的情况
        return MakeUnique<ElfWriterQuick<ElfTypes64>>(instruction_set, features, compiler_options, elf_file);
    } else { 
        //本例对应为x86 32位平台，所以模板参数取值为ElfTypes32
        //MakeUnique将构造一个ElfWriterQuick<ElfTypes32>类型的对象
        return MakeUnique<ElfWriterQuick<ElfTypes32>>(instruction_set, features, compiler_options, elf_file);
    }
}


//接着来看ElfWriterQuick的构造函数，代码如下所示。
//[elf_writer_quick.cc->ElfWriterQuick<ElfTypes>::ElfWriterQuick]
template <typename ElfTypes>ElfWriterQuick<ElfTypes>::ElfWriterQuick(
                InstructionSet instruction_set,
                const InstructionSetFeatures* features,
                const CompilerOptions* compiler_options,File* elf_file)
                : 
                    ElfWriter(),      //调用基类构造函数
                    instruction_set_features_(features),
                    compiler_options_(compiler_options),
                    【*】elf_file_(elf_file),
					rodata_size_(0u), 
					text_size_(0u), 
					bss_size_(0u),
                    //先构造一个FileOutputStream对象，然后在其基础上再构造一个BufferedOutputStream对象
                    【*】output_stream_(MakeUnique<BufferedOutputStream>(MakeUnique<FileOutputStream>(elf_file))),
                    //创建一个ElfBuilder对象
                    【*】builder_(new ElfBuilder<ElfTypes>(instruction_set, features, output_stream_.get())) {
    
    .......
}




//[elf_builder.h->ElfBuilder的构造函数]
ElfBuilder(InstructionSet isa, const InstructionSetFeatures* features, OutputStream* output)
        : 
		isa_(isa), 
		features_(features), 
		stream_(output),
    /*rodata_的类型为Section，对应ELF的.rodata section。rodata_除了包含输出信息外，还包含了ELF Section的相关标志。*/
    rodata_(this, ".rodata",SHT_PROGBITS,SHF_ALLOC,nullptr,0,kPageSize, 0),
    text_(this, ".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR,.....),
    ......
    dynsym_(this, ".dynsym", SHT_DYNSYM, SHF_ALLOC, &dynstr_),
    ..... 
	{
    /*从Execution View来观察ELF文件时，我们将看到segment以及描述它的Program Header（代码
      中简写为phdr）。下面的phdr_flags_为Section类的成员变量，用于标示该segment的标志。而
      phdr_type_ 则描述segment的类型。这些标记的详情请读者回顾第4章的内容。  */
    text_.phdr_flags_ = PF_R | PF_X;
    .....
    dynamic_.phdr_type_ = PT_DYNAMIC;
    ......
}

//[填写.rodata section示例代码]
//首先，调用ElfWriter的StartRoData函数。返回值的类型是一个OutputStream对象
OutputStream* rodata = elf_writer->StartRoData();
//调用OutputStream的WriteFully，写入数据
rodata->WriteFully(.rodata section的数据)
//然后调用ElfWriter的EndRoData结束对.rodata section的数据写入。EndRoData的参数
//是要结束输入的OutputStream对象
elf_writer->EndRoData(rodata)


/*
CreateOatWriters 函数中创建了用于输出Elf文件的 ElfWriter 以及 用于输出Oat信息的 OatWriter。
注意，一个ElfWriter对象并未和一个OatWriter对象有直接关系，
二者是通过在elf_writers_以及oat_writers_数组中的索引来关联的。以本章中的boot.oat为例。
*/




//[dex2oat.cc->Dex2Oat::AddDexFileSources]
bool AddDexFileSources() {
    TimingLogger::ScopedTiming t2("AddDexFileSources", timings_);
    if (zip_fd_ != -1) {......}
    else if (oat_writers_.size() > 1u) {......}
    else {
        /*对本例而言，dex_filenames_ 是数组，其元素来源于dex2oat的--dex-file选项指定那些jar
          文件路径名，dex_locations_ 内容和dex_filenames_ 一样。oat_writers_ 数组只包含一个
          元素，就是用于往boot.oat里输出oat信息的OatWriter对象。下面这个循环的含义就是将所
          有的输入dex文件和代表boot.oat的OatWriter对象关联起来，
		  具体的关联方式见下文OatWrtier的 AddDexFileSource 函数的代码分析。 */
        for (size_t i = 0; i != dex_filenames_.size(); ++i) {
            if (!oat_writers_[0]->AddDexFileSource(dex_filenames_[i],
                        dex_locations_[i])) { return false; }
        }
    }
    return true;
}



//[oat_writer.cc->OatWriter::AddDexFileSource]
bool OatWriter::AddDexFileSource(const char* filename,
								 const char* location,
								 CreateTypeLookupTable create_type_lookup_table) {
    /*为了从一个Dex文件中根据类名（字符串表示）快速找到类在dex文件中class_defs数组中的索引
      （即class_def_idx），ART设计了一个名为 TypeLookupTable 的类来实现该功能。
	  其实现类似 HashMap。笔者不拟介绍它。
	  AddDexFileSource第三个参数为CreateTypeLookupTable枚举变量，代
      代表是否创建类型查找表。默认为kCreate，即创建类型查找表   */
    uint32_t magic;
    std::string error_msg;
	
    /* OpenAndReadMagic 函数将打开filename指定的文件。如果成功的话，输入的jar包文件将被打开，
       返回的是该文件对应的文件描述符。ScopedFd 是辅助类，它的实例在析构时会关闭构造时传入的文
       件描述符。简单点说，ScopedFd会在实例对象生命结束时自动关闭文件。 
	 */
    ScopedFd fd(OpenAndReadMagic(filename, &magic, &error_msg));
    if (fd.get() == -1) {.....
    } else if (IsDexMagic(magic)) {.....}
        else if (IsZipMagic(magic)) { //jar包实际为zip压缩文件
        if (!AddZippedDexFilesSource(std::move(fd), location, create_type_lookup_table)) { 
			return false; 
		}
    } ....
    return true;
}



//[oat_writer.cc->OatWriter::AddZippedDexFilesSource]
bool OatWriter::AddZippedDexFilesSource(ScopedFd&& zip_fd,
										const char* location, 
										CreateTypeLookupTable create_type_lookup_table) {
    /*输入参数 zif_fd 代表被打开的jar文件。下面的ZipArchive::OpenFromFd函数用于处理这个文件。
      zip_archives_ 类型为vector<unique_ptr<ZipArchive>>，其元素的类型为ZipArchive，代
      表一个Zip归档对象。通过ZipArchive相关函数可以读取zip文件中的内容。  
	  */
    zip_archives_.emplace_back(ZipArchive::OpenFromFd(zip_fd.release(), location, &error_msg));
    //下面的zip_archive代表上面打开的jar包文件
    ZipArchive* zip_archive = zip_archives_.back().get();
    ......
    //读取其中的dex项
    for (size_t i = 0; ; ++i) {
        //如果jar包中含多个dex项，则第一个dex项名为classes.dex，其后的dex项名为classes2.dex，以此类推
        std::string entry_name = DexFile::GetMultiDexClassesDexName(i);
		//从ZipArchive中搜索指定名称的压缩项，返回值的类型为ZipEntry
        std::unique_ptr<ZipEntry> entry(zip_archive->Find(entry_name.c_str(), &error_msg));
        if (entry == nullptr) {  
			break;
		}
        //zipped_dex_files_ 成员变量存储jar包中对应的dex项
        zipped_dex_files_.push_back(std::move(entry));
        /* zipped_dex_file_locations_ 用于存储dex项的路径信息。注意，dex项位于jar包中，它
           的路径信息由jar包路径信息处理而来，以framework.jar为例，它的两个dex项路径信息如
           下所示：
           （1）classes.dex项路径信息为/system/framework/framework.jar
           （2）classes2.dex项路径信息为/system/framework/framework.jar:classes2.dex 
		 */
		 
	    zipped_dex_file_locations_.push_back(DexFile::GetMultiDexLocation(i, location));
	    //full_location就是dex路径名
        const char* full_location = zipped_dex_file_locations_.back().c_str();
	    /* oat_dex_files_ 是vector数组，元素类型为OatDexFile，它是OatWriter的内部类，
		  读者可回顾图9-9。注意下面的emplace_back函数的参数：
		  （1）首先，DexFileSource构造一个临时对象，假设是temp，
		  （2）full_location、temp、create_type_lookup_table三个参数一起构造一个Oat-
			  DexFile临时对象，假设为temp1
		  （3）temp1通过emplace_back加入oat_dex_files_数组的末尾 。 
		*/
	    oat_dex_files_.emplace_back(full_location,
									DexFileSource(zipped_dex_files_.back().get()),
									create_type_lookup_table);
    }
    return true;
}
