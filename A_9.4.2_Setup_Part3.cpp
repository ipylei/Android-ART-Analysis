//【9.4.3】
//[dex2oat.cc->Dex2Oat::Setup第三段]
{....
/* rodata_为Dex2Oat的成员变量，类型为vector<OutputStream*>。上文代码中涉及OutputStream的地
   方是在ElfWriter处。每一个输出ElF文件都有一个ElfWriter对象，此处的rodata_就和待创建的 ELF的.rodata section有关。
   由于本例中只会创建一个ELF文件—即boot.oat，所以rodata_数组的长度为1。 
   */
    rodata_.reserve(oat_writers_.size());
    //遍历oat_writers_数组
    for (size_t i = 0, size = oat_writers_.size(); i != size; ++i) {
        //ElfWriter的StartRoData返回ElfBuilder里的.rodata Section对象。回顾图9-8可知，Section类是OutputStream的子类。
        rodata_.push_back(elf_writers_[i]->StartRoData());
        
        //两个临时变量，用于存储OatWriter WriteAndOpenDexFiles 的结果。
        std::unique_ptr<MemMap> opened_dex_files_map;
        std::vector<std::unique_ptr<const DexFile>> opened_dex_files;
        
        /* WriteAndOpenDexFiles 比较关键。下文将详细介绍它。此处先了解它的几个参数：
           （1）rodata_.back()，对应一个ELF文件的rodata section。在本例中，这个ELF文件就是boot.oat。
           （2）oat_files_[i]就是代表boot.oat的File对象。注意，它是输出文件。
           （3）opened_dex_files_map 和 opened_dex_files 为输出参数。其作用见下文代码分析。
           
           注意，一个OatWriter对象对应哪些输入dex项是在上一节AddDexFileSource函数中加进去的。
           所以WriteAndOpenDexFiles一方面要打开输入的dex项（对应函数名中的OpenDexFiles），另一方面
           要将这些dex项的信息写入到oat文件中（对应函数名中的Write）。    
        */
        if (!oat_writers_[i]->WriteAndOpenDexFiles(rodata_.back(),
                    oat_files_[i].get(),instruction_set_,
                    instruction_set_features_.get(),key_value_store_.get(),
                    true,      //注意，这个参数为true
                    &opened_dex_files_map, &opened_dex_files)) {
            
            ..... 
                    
        }
        //又冒出几个和文件有关的成员变量，下文将介绍它们的含义。
        dex_files_per_oat_file_.push_back(MakeNonOwningPointerVector(opened_dex_files));
        if (opened_dex_files_map != nullptr) {
            opened_dex_files_maps_.push_back(std::move(opened_dex_files_map));
            for (std::unique_ptr<const DexFile>& dex_file : opened_dex_files) {
                /* dex_file_oat_index_map_类型为unordered_map<const DexFile*,size_t>，
                key值为一个DexFile对象，value为对应的OatWriter索引。对本例编译boot.oat而言，i取值为0。
                */
                dex_file_oat_index_map_.emplace(dex_file.get(), i);
                opened_dex_files_.push_back(std::move(dex_file));
            }    
        }    
    }
} //for循环结束
//赋值opened_dex_files_的内容给dex_files_数组。
dex_files_ = MakeNonOwningPointerVector(opened_dex_files_);



dex_files_per_oat_file_
dex_file_oat_index_map_
dex_file_oat_index_map_


//【9.4.3.1.1】
[oat.h->OatHeader]
//PACKED(4)表示OatHeader内存布局按4字节对齐。有些成员变量的含义将在后续章节中介绍
class PACKED(4) OatHeader {
	public:
		static constexpr uint8_t kOatMagic[] = { 'o', 'a', 't', '\n' };
		static constexpr uint8_t kOatVersion[] = { '0', '7', '9', '\0' };
		uint8_t magic_[4];          //oat文件对应的魔幻数，取值为kOatMagic
		uint8_t version_[4];        //oat文件的版本号，取值为kOatVersion
		uint32_t adler32_checksum_; //oat文件的校验和

		InstructionSet instruction_set_;           //该oat文件对应的CPU架构
		uint32_t instruction_set_features_bitmap_; //CPU特性描述
		uint32_t dex_file_count_;                  //oat文件包含多少个dex文件
		uint32_t executable_offset_;
		
		//蹦床
		//下面的成员变量描述的是虚拟机中几个trampoline函数的入口地址，它和Java虚拟机如何执行Java代码（不论是解释执行还是编译成机器码后的执行）有关，我们留待后续章节再来介绍。
		uint32_t interpreter_to_interpreter_bridge_offset_;
		uint32_t interpreter_to_compiled_code_bridge_offset_;
		uint32_t jni_dlsym_lookup_offset_;
		uint32_t quick_generic_jni_trampoline_offset_;
		uint32_t quick_imt_conflict_trampoline_offset_;
		uint32_t quick_resolution_trampoline_offset_;
		uint32_t quick_to_interpreter_bridge_offset_;
		
		......
		
		uint32_t key_value_store_size_;          //指明key_value_store_数组的真实长度
		uint8_t key_value_store_[0];             //上文介绍过它，请参考图9-3
}



//【9.4.3.1.2】　OatDexFile介绍
//接着来看OatDexFile项，它在代码中对应为OatDexFile类。图9-13所示Oat文件中包含的OatDexFile信息由该类的成员变量组成。
//[oat_writer.cc->OatDexFile类]
class OatWriter::OatDexFile { //此处只列出写入OatDexFile项的成员变量
    //描述dex_file_location_data_成员变量的长度
    uint32_t dex_file_location_size_;
	
    //dex项的路径信息，比如/system/framework/framework.jar::classes2.dex
    const char* dex_file_location_data_;
	
    uint32_t dex_file_location_checksum_;      //校验和信息
    uint32_t dex_file_offset_;                 //DexFile项相对于OatHeader的位置
    uint32_t class_offsets_offset_;            //ClassOffsets项相对于OatHeader的位置
    uint32_t lookup_table_offset_;             //TypeLooupTable项相对于OatHeader的位置
	
    //下面这个数组存储的是每一个OatClass项相对于OatHeader的位置。数组的长度为Dex文件中所定义的类的个数
    dchecked_vector<uint32_t> class_offsets_;
};



//【9.4.3.1.3】　OatClass介绍
//OatClass代表dex文件中的一个类，它包含的内容如图9-14所示。
//[oat_writer.cc->OatWriter::OatClass::OatClass]
OatWriter::OatClass::OatClass(size_t offset,
								const dchecked_vector<CompiledMethod*>& compiled_methods,
								uint32_t num_non_null_compiled_methods,
								mirror::Class::Status status)
								
								: compiled_methods_(compiled_methods) {      //保存compiled_method_数组
    /*注意构造函数的参数：
    offset：代表该OatClass对象相对于OatHeader的位置。
    compiled_methods：该类经过dex2oat编译处理的方法。
    num_non_null_compiled_methods：由于并不是所有方法都会经过编译，所以这个变量表示经过编译处理的方法的个数。
    status：类的状态。  
	*/
    uint32_t num_methods = compiled_methods.size();
    offset_ = offset;
    oat_method_offsets_offsets_from_oat_class_.resize(num_methods);
	
    //如果没有方法被编译处理过，则type_为kOatClassNoneCompiled。
    if (num_non_null_compiled_methods == 0) {
        type_ = kOatClassNoneCompiled;
    } else if (num_non_null_compiled_methods == num_methods) {
        type_ = kOatClassAllCompiled;            //所有方法都被编译处理过了
    } else { 
		type_ = kOatClassSomeCompiled;  
	}

	status_ = status;
	
    //method_offsets_和method_headers_数组的长度与被编译处理过的方法的格个数相等
    method_offsets_.resize(num_non_null_compiled_methods);
    method_headers_.resize(num_non_null_compiled_methods);

    uint32_t oat_method_offsets_offset_from_oat_class = sizeof(type_) + sizeof(status_);
	
    /*如果只有部分Java方法经过编译处理，则需要一种方式记录是哪些Java方法经过处理了。这里借助了
      位图的方法。比如，假设一共有三个Java方法，其中第0和第2个Java方法是编译处理过的，则可以
      设计一个三比特的位图，将其第0和第2位的值设为1即可 
	 */
    if (type_ == kOatClassSomeCompiled) {
        //位图的大小应能包含所有Java方法的个数
        method_bitmap_.reset(new BitVector(num_methods, false, Allocator::GetMallocAllocator()));
        method_bitmap_size_ = method_bitmap_->GetSizeOf();
        oat_method_offsets_offset_from_oat_class += sizeof(method_bitmap_size_);
        oat_method_offsets_offset_from_oat_class += method_bitmap_size_;
    } else { //如果所有Java方法或者没有Java方法被编译，则无需位图
        method_bitmap_ = nullptr;
        method_bitmap_size_ = 0;
    }
	
    //遍历 compiled_methods_ 数组
    for (size_t i = 0; i < num_methods; i++) {
        CompiledMethod* compiled_method = compiled_methods_[i];
		
		//为空，表示该Java方法没有编译处理过，所以将设置oat_method_offsets_offsets_from_oat_class_[i]的值为0
        if (compiled_method == nullptr) {      
            oat_method_offsets_offsets_from_oat_class_[i] = 0;
        } 
		//else分支表示这个Java方法已经编译处理过
		else { 
			 //oat_method_offsets_offsets_from_oat_class_[i]描述了OatMethodOffsets相比OatClass的位置
			oat_method_offsets_offsets_from_oat_class_[i] = oat_method_offsets_offset_from_oat_class;
			oat_method_offsets_offset_from_oat_class += sizeof(OatMethodOffsets);
			if (type_ == kOatClassSomeCompiled) {
				method_bitmap_->SetBit(i);      //设置位图对应比特位的值为1
			}
		}  
	} 
}




//[oat_writer.cc->OatWriter::OatClass::Write]
bool OatWriter::OatClass::Write(OatWriter* oat_writer,
								OutputStream* out,  
								const size_t file_offset) const {
    //输出status_
    if (!out->WriteFully(&status_, sizeof(status_))) {... }
    //OatWriter内部还有相关成员变量用于记录对应信息所需的空间，详情我们后续再介绍
    oat_writer->size_oat_class_status_ += sizeof(status_);
	
    //输出type_
    if (!out->WriteFully(&type_, sizeof(type_))) {....}
    oat_writer->size_oat_class_type_ += sizeof(type_);
	
    //如果有位图，则输出位图
    if (method_bitmap_size_ != 0) {
        if (!out->WriteFully(&method_bitmap_size_, sizeof(method_bitmap_size_))) {
				.....
			}
		oat_writer->size_oat_class_method_bitmaps_ += sizeof(method_bitmap_size_); 
		method_bitmap_size_)) {......}
        oat_writer->size_oat_class_method_bitmaps_ += method_bitmap_size_;
    }
	
    //输出 method_offsets_。
    if (!out->WriteFully(method_offsets_.data(), GetMethodOffsetsRawSize())){
			......
	}
    oat_writer->size_oat_class_method_offsets_ += GetMethodOffsetsRawSize();
    return true;
}







//【9.4.3.2】　WriteAndOpenDexFiles
//现在来看Setup代码段三中的WriteAndOpenDexFiles函数，代码如下所示。
//[oat_writer.cc->OatWriter::WriteAndOpenDexFiles]
bool OatWriter::WriteAndOpenDexFiles( OutputStream* rodata, File* file,
										InstructionSet instruction_set,
										const InstructionSetFeatures* instruction_set_features,
										SafeMap<std::string, std::string>* key_value_store,
										bool verify,
										/*out*/ std::unique_ptr<MemMap>* opened_dex_files_map,
										/*out*/ std::vector<std::unique_ptr<const DexFile>>* opened_dex_files) {
	//InitOatHeader创建OatHeader结构体并设置其中的内容。返回值代表OatHeader之后的内容
	//应该从文件什么位置开始。该函数比较简单，请读者自行阅读。
	size_t offset = InitOatHeader(instruction_set, instruction_set_features, dchecked_integral_cast<uint32_t>(oat_dex_files_.size()), key_value_store);
	
    //InitOatDexFiles用于计算各个OatDexFile的大小及它们在oat文件里的偏移量。
    //offset作为输入时指明OatDexFile开始的位置，作为返回值时表示后续内容应该从文件什么位置开
    //始。该函数非常简单，请读者自行阅读。
    offset = InitOatDexFiles(offset);
    size_ = offset;
    std::unique_ptr<MemMap> dex_files_map;
    std::vector<std::unique_ptr<const DexFile>> dex_files;
	
    //写入Dex文件信息。请读者自行阅读此函数。
    if (!WriteDexFiles(rodata, file)) { return false; }
    for (OatDexFile& oat_dex_file : oat_dex_files_) {
        //计算类型查找表所需空间。
        oat_dex_file.ReserveTypeLookupTable(this);
    }
    size_t size_after_type_lookup_tables = size_;
    for (OatDexFile& oat_dex_file : oat_dex_files_) {
        //计算ClassOffsets所需空间。
        oat_dex_file.ReserveClassOffsets(this);
    }
    ChecksumUpdatingOutputStream checksum_updating_rodata(rodata,
    oat_header_.get());
	
    /*WriteOatDexFiles：将OatDexFile信息写入oat文件的OatDexFile区域。ExtendForType-
      LookupTables：扩充输出文件的长度，使之能覆盖类型查找表所需空间。这两个函数比较简单，读者
      可自行阅读。下文将介绍 OpenDexFiles 和 WriteTypeLookupTables 两个函数。  
	 */
    if (!WriteOatDexFiles(&checksum_updating_rodata) ||
        !ExtendForTypeLookupTables(rodata, file, size_after_type_lookup_tables) ||
        !OpenDexFiles(file, verify, &dex_files_map, &dex_files) ||
        !WriteTypeLookupTables(dex_files_map.get(), dex_files)){
			......
	}
    ......
    *opened_dex_files_map = std::move(dex_files_map);
    *opened_dex_files = std::move(dex_files);
    write_state_ = WriteState::kPrepareLayout;
    return true;
}





//[oat_writer.cc->OatWriter::OpenDexFiles]
bool OatWriter::OpenDexFiles(File* file, bool verify,
            std::unique_ptr<MemMap>* opened_dex_files_map,
            std::vector<std::unique_ptr<const DexFile>>* opened_dex_files) {
    /*注意参数：
    file：代表需要创建的oat文件，对本例而言就是boot.oat文件
    verify：取值为true
    opened_dex_files_map和opened_dex_files为输出参数，详情见下面的代码  */
    size_t map_offset = oat_dex_files_[0].dex_file_offset_;
    size_t length = size_ - map_offset;
    std::string error_msg;
    /*创建一个基于文件的MemMap对象。这个文件是boot.oat。注意，我们map boot.oat的位置是从图9-13
      中DexFile区域开始的，长度覆盖了DexFile区域和TypeLoopupTable区域。另外，提醒读者，调
      用到这个函数的时候，来自jar里的dex项已经写入oat文件对应区域了。代码中有很多计算偏移量的
      地方，读者不必纠结，只需关注各个区域都包含什么内容即可。*/
    std::unique_ptr<MemMap> dex_files_map(MemMap::MapFile(length, PROT_READ | PROT_WRITE, MAP_SHARED,file->Fd(), 
			oat_data_offset_ + map_offset, false, file->GetPath().c_str(),&error_msg));
    .....
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    for (OatDexFile& oat_dex_file : oat_dex_files_) {
        //获取oat DexFile区域中的每一个dex文件的内容
        const uint8_t* raw_dex_file = dex_files_map->Begin() + oat_dex_file.dex_file_offset_ - map_offset;
        ......
        //先调用DexFile Open函数打开这些Dex文件，返回值的类型为DexFile*，将其存储在 dex_files_ 数组中
        dex_files.emplace_back(DexFile::Open(raw_dex_file,
											oat_dex_file.dex_file_size_,oat_dex_file.GetLocation(),
											oat_dex_file.dex_file_location_checksum_,nullptr, verify,
											&error_msg));
    }
    //更新输入参数。
    *opened_dex_files_map = std::move(dex_files_map);
    *opened_dex_files = std::move(dex_files);
    return true;
}




//[oat_writer.cc->OatWriter::WriteTypeLookupTables]
bool OatWriter::WriteTypeLookupTables( MemMap* opened_dex_files_map,
										const std::vector<std::unique_ptr<const DexFile>>& opened_dex_files) {
    ......
    for (size_t i = 0, size = opened_dex_files.size(); i != size; ++i) {
        OatDexFile* oat_dex_file = &oat_dex_files_[i];
        if (oat_dex_file->lookup_table_offset_ != 0u) {
            size_t map_offset = oat_dex_files_[0].dex_file_offset_;
            size_t lookup_table_offset = oat_dex_file->lookup_table_offset_;
            //找到每个Dex文件对应的TypeLookupTable的位置
            uint8_t* lookup_table = opened_dex_files_map->Begin() + (lookup_table_offset - map_offset);
			//在这个位置上创建该Dex文件的TypeLookupTable。
            //读者可自行阅读CreateTypeLookup函数
            opened_dex_files[i]->CreateTypeLookupTable(lookup_table);
        }
    }
    .....
    return true;
}