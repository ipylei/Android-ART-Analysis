//[dex2oat.cc->Dex2Oat::OpenFile]
bool OpenFile() {
    /* 检查 dex_filenames_ 数组中那些通过--dex-file传入的输入文件是否存在，如果不存在，则将
       其从dex_filenames_数组中去掉   
    */
    PruneNonExistentDexFiles();
	
    //本例不使用 multi_image_。
    if (IsBootImage() && multi_image_) {
        ExpandOatAndImageFilenames();
    }
	
    bool create_file = oat_fd_ == -1;    //本例中create_file为true
	
	//下面将创建目标oat文件
    if (create_file) {                  
	
        /*回顾表9-1可知，oat_filenames_ 是dex2oat的成员变量，类型为vector<constchar*>，
          它通过 --oat-file 选项设置。对本例而言，oat_filenames_ 只包含一个元素，
          其值为"/data/dalvik-cache/x86/system@framework@boot.oat"，即boot.oat文件。 
		*/
        for (const char* oat_filename : oat_filenames_) {
            
			//CreateEmptyFile 将创建对应路径的文件对象，由File保存。File是art封装的、用于对文件进行读、写等操作的类。
            std::unique_ptr<File> oat_file(OS::CreateEmptyFile(oat_filename));
			
            .....
			
            //oat_files_ 类型为vector<unique_ptr<File>>，此处将File对象存入 oat_files_ 里
            oat_files_.push_back(std::move(oat_file));
        }
    } 
	
	.... //其他情况的处理，和本例无关
	
    return true;
}




//[os.h->File类型定义]
/*unix_file 是一个命名空间，它包括了unix系统（包括linux）上和文件相关的内容。
	Fd是文件描述符（File Descriptor）的意思。    
  */
typedef ::unix_file::FdFile File      //这行代码表示File是FdFile的别名
								