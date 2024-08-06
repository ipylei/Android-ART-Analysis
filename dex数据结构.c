header
  ·magic，取值必须是字符串“dex\n035\0”，或者byte数组{0x640x650x780x0a 0x300x330x350x00}。
  ·checksum，文件内容的校验和。不包括magic和checksum自己。该字段的内容用于检查文件是否损坏。
  ·signature，签名信息，不包括 magic、checksum 和 signature。该字段的内容用于检查文件是否被篡改。
  ·file_size，整个文件的长度，单位为字节，包括所有内容。
  ·header_size，默认是0x70个字节。
  ·endian_tag，表示文件内容应该按什么字节序来处理。默认取值为0x12345678，Little Endian格式。
               如果为Big Endian时，该字段取值为0x78563412。
               

string_ids
type_ids         数组，元素类型为 type_id_item。存储类型相关的信息（由TypeDescriptor描述）。
proto_ids        主要功能就是用于描述成员函数的参数、返回值类型，同时包含ShortyDescriptor信息
filed_ids        数组，元素类型为 field_id_item，存储成员变量信息，包括变量名、类型等
method_ids       数组，元素类型为 method_id_item，存储成员函数信息包括函数名、参数和返回值类型等。
class_defs       数组，元素类型为 class_def，存储类的信息
//-----------------------------------------------------------
data             Dex文件重要的数据内容都存在data区域里
link_data        理论上是预留区域，没有特别的作用


//字符串 - 项
struct string_data_item{
    uleb128 utf16_size;      //字符串中字符的个数
    ubyte[] data;            //字符串对应的内容
}


//作为 string_ids 的元素
//字符串 - 指针
struct string_id_item{
    uint string_data_off;    //指明 string_data_item 位于文件的位置
}


//作为 type_ids 的元素
//[类型]：字符串表示
struct type_id_item{
    uint descriptor_idx;     //指向 string_ids 的索引，即对应 string_id_item
}


//作为 filed_ids 的元素
//属性，对应为 ArtField
struct field_id_item{
    //(类型) 所属类
    ushort class_idx;        //指向 type_ids 的索引，即对应 type_id_item
    //(类型) 字段的类型
    ushort type_idx;         //指向 type_ids 的索引，即对应 type_id_item
    //(字符串) 字段名称
    uint name_idx;           //指向 string_ids 的索引，即对应 string_id_item
}


//作为 method_ids 的元素
//方法，对应为 ArtMethod
struct method_id_item{
    //(类型)    所属类
    ushort class_idx;        //指向 type_ids 的索引，即对应 type_id_item
    //(多类型)  方法签名类型
    ushort proto_idx;        //指向 proto_ids 的索引，即对应 proto_id_item
    //(字符串)  方法名称
    uint name_idx;           //指向 string_ids 的索引，即对应 string_id_item
}


//作为 proto_ids 的元素
//方法签名类型：描述成员函数的参数、返回值类型
struct proto_id_item{
    //(字符串)  (简短描述)参数和返回值的类型的简单描述，比如所有引用类型都用"L"统一表示
    uint shorty_idx;         //指向 string_ids 的索引，即对应 string_id_item
    //(类型)    (具体描述)返回值类型
    uint return_type_idx;    //指向 type_ids 的索引，即对应 type_id_item
    //(多类型2) (具体描述)参数类型
    uint parameters_off;     //如果不为0，则文件对应的地方存储 type_list 的结构，用于描述函数参数的类型。
}

//数组，存放多个参数(类型)
struct type_list{
    uint size;               //size表示list数组的个数，而list数组元素类型为 type_item。
    type_item[] list;
}

//类型
struct type_item{
    ushort type_idx;         //指向 type_ids 的索引，即对应 type_id_item
}


//作为 class_defs 的元素
//存储类信息
struct class_def{
    uint class_idx;          //指向type_ids，即对应 type_id_item ，代表本类的类型
    uint access_flags;       //访问标志，比如private、public等。
    uint superclass_idx;     //指向type_ids，代表基类类型，如果没有基类则取值为NO_INDEX（值为-1）
    
    uint interfaces_off;     //如果本类实现了某些接口，则 interfaces_off 指向文件对应位置，
                             //那里存储了一个 type_list。该 type_list 的list数组存储了每一个接口类的 type_idx 索引
    
    uint source_file_idx;    //指向string_ids，该类对应的源文件名(xxx.java)
    uint annotations_off;    //存储和注解有关的信息
    
    uint class_data_off;     //指向文件对应位置，那里将存储更细节的信息，由 class_data_item 类型来描述
    
    uint static_values_off;  //储用来初始化类的静态变量的值，静态变量如果没有显示设置初值的话，默认是0或者null。
                             //如果有初值的话，初值信息就存储在文件 static_values_off 的地方，
                             //对应的数据结构名为 encoded_array_item。本章不拟讨论它，读者可自行阅读参考资料[2]。
}


//存储类信息：更细节
struct class_data_item{
    uleb128 static_fields_size;         //对应的数组长度
    uleb128 instance_fields_size;
    uleb128 direct_method_size;
    uleb128 virtual_method_size;
    
    endoded_filed[] static_fields;      //类的静态成员信息
    endoded_filed[] instance_fields;    //类的非静态成员信息
    
    endoded_method[] direct_fields;     //非虚函数信息：包含该类中所有static、private函数以及构造函数
    
    endoded_method[] virtual_fields;    //虚函数信息：包含该类中除static、final 以及 构造函数之外的函数，
                                        //并且不包括从父类继承的函数（如果本类没有重载它的话）
}

struct endoded_filed{
    uleb128 field_idx_diff;            //索引值的偏移量，通过它能找到这个成员变量的变量名，数据类型，以及它所在类的类名
                                          //第一个元素的 field_idx_diff 取值为索引；后续的 field_idx_diff 取值为和前一个索引值的差!
                                            
    uleb128 access_flags;              //访问标志，比如private、public等
}

struct endoded_method{
    uleb128 method_idx_diff;           //指向 method_id_item (memthod_ids的下标)
                                         //第一个元素的 method_idx_diff 取值为索引；后续的 method_idx_diff 取值为和前一个索引值的差!
    
    uleb128 access_flags;              //访问权限
    
    uleb128 code_off;                  //指向文件对应位置处，那里有一个类型为 code_item 的结构体
                                         //code_item 类似于Class文件的Code属性，即存放指令 
}

//p60  存储方法的源码信息
stuct code_item{
    ushort regisrers_size;    
    ushort ins_size;     
    ushort out_size;
    ushort tries_size;       
    uint debug_info_off;
    uint insns_size;   
    ushort[] insns;   
    ushort padding;  
    try_item[] tries;                  
    encode_catch_handler_list handlers; //catch语句对应的内容，也是可选项。如果tries_size不为零才有handlers域。
}


stuct code_item{
    /*
    registers_size和ins_size进一步说明
    （1）registers_size：指的是虚拟寄存器的个数。在art中，dex字节码会被编译成本机机器码，此处的寄存器并非物理寄存器。
    （2）ins_size：Dex官方文档的解释是the number of words of incoming arguments to the method that this code is for，
                   而在art优化器相关代码中，ins_size 即是函数输入参数个数，同时也是输入参数占据虚拟寄存器的个数。
                   (registers_size - ins_size 即为函数内部创建变量的个数。
    */ 
    ushort regisrers_size;             //此函数需要用到的寄存器个数。
    ushort ins_size;                   //输入参数所占空间，以双字节为单位  
    ushort out_size;                   //该函数表示内部调用其他函数时，所需参数占用的空间。同样以双字节为单位
    
    //· tries_size 和 tries 数组：如果该函数内部有try语句块，则tries_size和tries数组用于描述try语句块相关的信息。
    //注意，tries 数组是可选项，如果 tries_size 为 0，则此code_item不包含tries数组。
    [*]ushort tries_size;              //①
    [*]try_item[] tries;               //①
    [*]ushort padding;                 //① 用于将tries数组（如果有，并且 insns_size 是奇数长度的话）进行4字节对齐。
    [*]encode_catch_handler_list handlers; //catch语句对应的内容，也是可选项。如果 tries_size 不为零才有 handlers 域。
    
    uint debug_info_off;
    
    uint insns_size;                   //指令码数组的长度
    ushort[] insns;                    //指令码的内容。Dex文件格式中JVM指令码长度为2个字节，而Class文件中JVM指令码长度为1个字节
                                       //低8位才是指令码，高8位是参数
}



Android源码查看：
    http://androidxref.com/8.1.0_r33/xref/dalvik/libdex/Leb128.h
Android源码复制：
    https://www.androidos.net.cn/android/8.0.0_r4/raw/dalvik/libdex/Leb128.h

Dex解析器：Android源码
    http://androidxref.com/8.1.0_r33/xref/dalvik/dexdump/DexDump.cpp


Dex字节码分析
    https://source.android.com/devices/tech/dalvik/instruction-formats
    https://source.android.com/devices/tech/dalvik/dalvik-bytecode
    
