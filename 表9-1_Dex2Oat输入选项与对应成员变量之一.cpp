std::unique_ptr<CompilerOptions> compiler_options_                
输入选项：无
说明：保存dex2oat编译时所需的一些编译选项，比如 CompilerFilter 的取值等


std::vector<const char*> dex_filenames_
--dex-file
指定待编译的jar包。图中有13个输入jar包

 
std::vector<const char*> image_filenames_
--image
指定编译输出的.art镜像文件名


std::vector<const char*> oat_filenames_ 
--oat-file
指定编译输出的oat文件名

const char* image_classes_filename_
--image-classes
指定一个文件，这个文件里包含了需要包含到boot镜像中的类的类名
示例值：取值为/system/etc/preloaded-classes

const char*  compiled_classes_filename_
--compiled-classes
也指定了一个文件。该文件包含了需要编译到boot镜像中的类的类名
示例值：取值为/system/etc/preloaded-phone


uintptr_t image_base
--base
用于指定一个内存基地址
示例值：取值为0x70aba000

InstructionSet instruction_set_
--instruction-set
指定机器码运行的CPU架构名
枚举变量，示例值:x86 -> KX86


std::unique_ptr<const InstructionSetFeatures> instruction_set_features_;
--instruction-set-features
指定CPU支持的特性
示例值：x86 => X86OmstuctionSetFeatures


std::vector<const char*> runtime_args_;
--runtime-arg
存储紧接--runtime-arg后面的那个字符串
比如：--runtime-arg -Xms64m就会存储 -Xms64m

std::unique_ptr<SafeMap<std::string, std::string> > key_value_store_;
存储参数和值对
ART自定义的容器类，和STL map类似