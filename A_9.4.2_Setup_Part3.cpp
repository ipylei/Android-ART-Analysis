//[dex2oat.cc->Dex2Oat::Setup第三段]
{....
/* rodata_为Dex2Oat的成员变量，类型为vector<OutputStream*>。上文代码中涉及OutputStream的地
   方是在ElfWriter处。每一个输出ElF文件都有一个ElfWriter对象，此处的rodata_就和待创建的
   ELF的.rodata section有关。由于本例中只会创建一个ELF文件—即boot.oat，所以rodata_数
   组的长度为1。 */
    rodata_.reserve(oat_writers_.size());
    //遍历oat_writers_数组
    for (size_t i = 0, size = oat_writers_.size(); i != size; ++i) {
        //ElfWriter的StartRoData返回ElfBuilder里的.rodata Section对象。回顾图9-8可知，Section类是OutputStream的子类。
        rodata_.push_back(elf_writers_[i]->StartRoData());
        
        //两个临时变量，用于存储OatWriter WriteAndOpenDexFiles的结果。
        std::unique_ptr<MemMap> opened_dex_files_map;
        std::vector<std::unique_ptr<const DexFile>> opened_dex_files;
        
        /* WriteAndOpenDexFiles比较关键。下文将详细介绍它。此处先了解它的几个参数：
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
                    &opened_dex_files_map,&opened_dex_files)) {
            
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