//OatFileManager介绍


//[oat_file_manager.h->OatFileManager声明]
class OatFileManager {
	
private: //代码行位置有所调整
    .....
    //OatFile是oat文件在代码中的表示。OatFileManager管理多个oat文件，所以它有一个容器成员。
	//此处使用了std的set容器
    std::set<std::unique_ptr<const OatFile>> oat_files_
    bool have_non_pic_oat_file_;
    static CompilerFilter::Filter filter_;
    //OafFileManager禁止使用拷贝和赋值构造函数，读者还记得这是怎么实现的吗？
    DISALLOW_COPY_AND_ASSIGN(OatFileManager);
	
public:
	//构造函数
    OatFileManager() : have_non_pic_oat_file_(false) {}
    //析构函数
	~OatFileManager();
    
    //往OatManager中添加一个OatFile对象
    const OatFile* RegisterOatFile(std::unique_ptr<const OatFile> oat_file)
    
    void UnRegisterAndDeleteOatFile(const OatFile* oat_file)
    
    ....
    
    /*ART虚拟机 会 加载多个OAT文件。其中：
     （1）zygote 作为第一个Java进程会首先加载一些基础与核心的oat文件。
            这些oat文件里包含了Android系统中所有Java程序所依赖的基础功能类（比如Java标准类）。
            这些oat文件称之为boot oat文件
            （【与之对应】的一个名词叫boot image，下文将详细介绍boot image的相关内容）
			
      （2）APP进程通过zygote fork 得到，然后该 APP进程将加载APK包经过dex2oat得到的oat文件。
            该APP对应的oat文件叫app image。下面的 GetBootOatFiles 将返回boot oat文件信息，
            返回的是一个vector数组
			
			
		根据art文件的来源（比如它是从哪个jar包或apk包编译得来的），Image分为boot镜像（boot image）和app镜像（app image）。
		·来源于某个apk的art文件称为App镜像。
		另外，一般而言，针对核心库的编译都会生成boot.art镜像文件，
		而针对app的编译则通过dex2oat相关选项来控制是否生成对应的art文件。
    */
    std::vector<const OatFile*> GetBootOatFiles() const;
    
    /*将包含在boot镜像里的oat【文件信息】注册到OatFileManager中。
    ImageSpace 代表一个由映射内存构成的区域（Space）。
    zygote启动后，会将boot oat文件通过memap映射到内存，从而得到一个ImageSpace。
    下文将详细介绍和ImageSpace相关的内容
	*/
    std::vector<const OatFile*> RegisterImageOatFiles(std::vector<gc::space::ImageSpace*> spaces)
    
    //从Oat文件中找到指定的dex文件。dex文件由 DexFile 对象表示
    std::vector<std::unique_ptr<const DexFile>> OpenDexFilesFromOat(const char* dex_location,
																	const char* oat_location,
																	jobject class_loader, 
																	jobjectArray dex_elements,
																	/*out*/ const OatFile** out_oat_file,
																	/*out*/ std::vector<std::string>* error_msgs)
};



//往OatFileManager中注册OatFile对象非常简单，其代码如下所示。
//[oat_file_manager.cc->OatFileManager::RegisterOatFile]
const OatFile* OatFileManager::RegisterOatFile(std::unique_ptr<const OatFile> oat_file) {
    WriterMutexLock mu(Thread::Current(), *Locks::oat_file_manager_lock_);
    have_non_pic_oat_file_ = have_non_pic_oat_file_ || !oat_file->IsPic();
    const OatFile* ret = oat_file.get();
    //就是将OatFile对象保存到oat_files_（类型为std set）容器中
    oat_files_.insert(std::move(oat_file));
    return ret;
}