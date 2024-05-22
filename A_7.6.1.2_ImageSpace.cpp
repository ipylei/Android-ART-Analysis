//art文件将被映射到art虚拟机进程的内存里，
//该工作是通过ImageSpace的init函数来完成的，其返回结果是一个ImageSpace对象
//[image_space.cc->ImageSpace::Init]
ImageSpace* ImageSpace::Init(const char* image_filename, 
                            const char* image_location, 
                            bool validate_oat_file, 
                            const OatFile* oat_file,
                            std::string* error_msg) {
                                
    /*注意Init参数，尤其是和文件路径有关的参数很容易搞混：
     （1）image_filename：指向/data/dalvik-cache下的某个art文件，
                    比如/data/dalvik-cache/x86/system@framework@boot.art。
					
     （2）image_location：取值为诸如/system/framework/boot.art这样的路径。
                    以x86平台和boot.art文件为例。首先，编译boot镜像时，
                    core-oj.jar所得到的boot.art文件真实路径其实是/system/framework/x86/boot.art。
                    (*)出于简化使用的考虑，凡是要用到这个路径的地方只需要使用/system/framework/boot.art即可，
                    程序内部会将CPU平台名（比如x86）插入到framework后。
                    所以，这也是上面 image_location 取值中不含"x86"的原因。
                    其次，art虚拟机并不会直接使用 framework 目录下的boot.art文件，
                    而是会先将该文件拷贝（某些情况下可能还会做一些特殊处理以加强安全性）
                    到/data/dalvik-cache对应目录下。
                    dalvik-cache 下的boot.art文件名将变成system@framework@boot.art，
                    它其实是把输入字符串"system/framework/boot.art"中的"/"号换成了"@"号。
                    这种处理的目的是可以追溯dalvik-cache下某个art文件的来源。
                    最后，如果framework下的art文件有更新的话（比如系统升级），
                    或者用户通过恢复出厂设置清理了dalvik-cache目录的话，
                    dalvik-cache下的art文件都将重新生成。
    
    总之，请读者牢记两点：
        第一，art虚拟机加载的是dalvik-cache下的art文件。
        第二，dalvik-cache下的art文件和其来源的art文件可能并不完全相同。
                某些情况下，虚拟机直接从frameowrk等源目录下直接拷贝过来，
                有些情况下虚拟机会对源art文件进行一些处理以提升安全性
                （比如，当源art文件是非Position Independent Code模式的话，虚拟机会做重定位处理）。
    */
    std::unique_ptr<File> file;
    { ......
        //打开dalvik-cache下的art文件
        file.reset(OS::OpenFileForReading(image_filename));
        ......
    }
    
    ImageHeader temp_image_header;
    ImageHeader* image_header = &temp_image_header;
    {
        //从该art文件中读取ImageHeader，它位于文件的头部
        bool success = file->ReadFully(image_header, sizeof(*image_header));
        ......
    }
    //获取该art文件的大小
    const uint64_t image_file_size = static_cast<uint64_t>(file->GetLength());

    if (oat_file != nullptr) {
        //oat_file为该art文件对应的oat文件。此段代码用于检查oat文件的校验和与
        //art文件ImageHeader里保存的oat文件校验和字段（图7-16中未展示该字段）是否相同。
    }
    //取出ImageHeader的 section_ 数组中最后一个类型（kSectionImageBitmap）所对应的ImageSection对象。
    //这个section里存储的是位图空间的位置和大小。
    const auto& bitmap_section = image_header->GetImageSection(ImageHeader::kSectionImageBitmap);

    //如图7-16所示，kSectionImageBitmap 所在的section的偏移量需要按内存页大小对齐
    const size_t image_bitmap_offset = RoundUp(sizeof(ImageHeader) + image_header->GetDataSize(),kPageSize);

    //计算 image bitmap_section 的末尾位置
    const size_t end_of_bitmap = image_bitmap_offset + bitmap_section.Size();
    if (end_of_bitmap != image_file_size) {
        .......//检查bitmap的末尾处是不是等于整个art文件的大小
    }
    
    //现在准备将art文件map到内存。addresses是一个数组，
    //第一个元素为ImageHeader所期望的 map 到内存里的地址值
    std::vector<uint8_t*> addresses(1, image_header->GetImageBegin());
    //art文件映射到内存后的MemMap对象就是它
    std::unique_ptr<MemMap> map; 
    std::string temp_error_msg;
    
    //下面是一个循环。某些情况下map到期望的地址可能会失败，
    //这时候会尝试将art文件map到一个由系统设定的地址值上（map的时候传入nullptr为期望地址值即可）。
    for (uint8_t* address : addresses) {
            //art文件一般都比较大。所以，在生成它的时候可以对其内容进行压缩存储。
            //注意，ImageHeader不进行压缩。本章不考虑压缩的情况。
            const ImageHeader::StorageMode storage_mode = image_header->GetStorageMode();
        if (storage_mode == ImageHeader::kStorageModeUncompressed) {
            //将art文件映射到zygote进程的虚拟内存空间。其中，address取值为ImageHeader里的image_begin_，
            //从文件的0处开始映射，映射的大小为image_size_，映射空间支持可读和可写
            map.reset(MemMap::MapFileAtAddress(address,
												image_header->GetImageSize(),
												PROT_READ | PROT_WRITE, 
												MAP_PRIVATE, 
												file->Fd(),0, true,false, 
												image_filename, 
												out_error_msg));
        } else {  
            .....  /*对压缩存储模式的处理 */ 
        }
        if (map != nullptr) { 
			//map不为空，跳出循环
			break;
		}
    }
    
    //下面的语句将检查art文件映射内存的头部是否为 ImageHeader
    DCHECK_EQ(0, memcmp(image_header, map->Begin(), sizeof(ImageHeader)));
    
    /*单独为art文件里image bitmap_section 所描述的空间进行内存映射。
    期望的内存映射起始位置由系统指定(即指定nullptr)，
    它在文件里的起始位置为 image_bitmap_offset，映射空间有section的Size函数返回。
    同时，请读者注意这段映射内存为只读空间。*/
    std::unique_ptr<MemMap> image_bitmap_map(MemMap::MapFileAtAddress(nullptr,bitmap_section.Size(),PROT_READ, MAP_PRIVATE, file->Fd(),image_bitmap_offset,false,false,image_filename, error_msg));
    
    /*上述代码中，我们得到了两个 MemMap 对象，
        一个是art文件里除最后一个Section之外其余部分所映射得到的MemMap对象，
        另外一个是最后一个image bitmap section部分映射得到的MemMap对象。
        下面的代码中将把这两个MemMap对象整合到一起
	*/
    image_header = reinterpret_cast<ImageHeader*>(map->Begin());
    const uint32_t bitmap_index = bitmap_index_.FetchAndAddSequentiallyConsistent(1);
    std::string bitmap_name(StringPrintf("imagespace %s live-bitmap %u", image_filename,   bitmap_index));
    
    //获取art文件里第一个section（即kSectionObjects）的ImageSection对象
    const ImageSection& image_objects = image_header->GetImageSection(ImageHeader::kSectionObjects);
    
    //该section所覆盖的位置。
    uint8_t* const image_end = map->Begin() + image_objects.End();
   
   /*创建一个 ContinuousSpaceBitmap 对象。上文曾介绍过ContinuousSpaceBitmap,
    它其实是SpaceBitmap按8字节进行实例化得到的类。其本质是一个SpaceBitmap。
    回顾我们对HeapBitmap的介绍可知，它包括的信息有：
     （1）位图对象本身所占据的内容：根据下面的代码，这段内存就放在image_bitmap_map中
     （2）位图所对应的那块内存空间：起始地址由map的Begin返回
        （也就是art文件所映射得到的第一个Memap对象的基地址，对应为ImageHeader的image_begin_），
        大小为该section的大小（End函数将计算offset+size）。

    注意，当bitmap对象被创建后，因为image_bitmap_map的release函数被调用，
    所以image_bitmap_map所指向的MemMap对象将由bitmap来管理()*/
        std::unique_ptr<accounting::ContinuousSpaceBitmap> bitmap;
        {
		 
		 ......
         
		 bitmap.reset(accounting::ContinuousSpaceBitmap::CreateFromMemMap(
					bitmap_name,
					image_bitmap_map.release(), 
					reinterpret_cast<uint8_t*>(map->Begin()), 
					image_objects.End()
				)
			);
         
		 .......
        
		}
        {
            TimingLogger::ScopedTiming timing("RelocateImage", &logger);
            //如果这个art镜像文件支持pic，则可能需要对其内容进行重定位处理（也就是将原本放在a处的内容搬移到b处）。
			//我们略过对它的讨论，建议感兴趣的读者阅读完本章和相关章节之后再来研究它。
            if (!RelocateInPlace(*image_header, map->Begin(),bitmap.get(),oat_file, error_msg)) { 
                return nullptr; 
            }
        }
		
        /*创建一个 ImageSpace 对象，其构造参数如下：
          map：代表art文件加载到内存里的MemMap对象。
          bimap：代表art文件中最后一个section对应的位图对象。
          
        image_end：map所在的内存空间并未全部囊括，只包括sections_[kSectionObject]的部分。
        image_end 取值就是map->Begin()+sections_[kSectionObject]的大小。
        
        std::unique_ptr<ImageSpace> space(new ImageSpace(image_filename, image_location, map.release(),bitmap.release(), image_end));
        其他一些处理，包括打开该art文件对应的oat文件。这部分代码比较简单，读者可自行阅读
        */
        ......
        Runtime* runtime = Runtime::Current();
        if (image_header->IsAppImage()) {
			
            ......//如果是APP镜像，则做对应处理
			
        } else if (!runtime->HasResolutionMethod()) {
            //根据ImageHeader的信息设置一些参数，这部分内容我们留待后续章节再介绍
            runtime->SetInstructionSet(space->oat_file_non_owned_->GetOatHeader().GetInstructionSet());
            runtime->SetResolutionMethod(image_header->GetImageMethod(ImageHeader::kResolutionMethod));
            ....
        }
        ......
        return space.release();//返回所创建的space对象
}