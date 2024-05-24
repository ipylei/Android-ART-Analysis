//[dex2oat.cc->Dex2Oat::Setup第二段]
{
	.... //第二段代码
	RuntimeArgumentMap runtime_options;
	/*为创建编译时使用的Runtime对象准备参数。由上节内容可知，dex2oat用得不是完整虚拟机。另外，
	  dex2oat中的这个runtime只有Init会被调用，而它的Start函数不会被调用。所以，dex2oat里
	  用到的这个虚拟机也叫unstarted runtime  */
	if (!PrepareRuntimeOptions(&runtime_options)) { 
		return false; 
	}
	
	//CreateOatWriters将创建ElfWriter和OatWriter对象
	CreateOatWriters();       //①关键函数，见下文介绍
	
	
	AddDexFileSources();      //②关键函数，见下文介绍
	if (IsBootImage() && image_filenames_.size() > 1) {
		...... //multi_image_情况的处理
	}
}



//【9.4.2.1.1】　ElfWriter和ElfBuilder
//ElfWriter是ART里用于往ELF文件中写入相关信息的工具类，其类家族如图9-7所示。


/*
CreateOatWriters函数中创建了用于输出Elf文件的 ElfWriter 以及 用于输出Oat信息的 OatWriter。
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
