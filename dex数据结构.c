struct string_data_item{
    uleb128 utf16_size;      //字符串中字符的个数
    ubyte[] data;            //字符串对应的内容
}

//字符串
struct string_id_item{
    uint string_data_off;    //指明 string_data_item 位于文件的位置，也就是索引    
}

//[类型]：字符串表示
struct type_id_item{
    uint descriptor_idx;     //指向string_ids的索引，即对应 string_id_item
}

struct field_id_item{
    //(类型) 所属类
    ushort class_idx;        //指向type_ids的索引，即对应 type_id_item
    //(类型) 字段的类型
    ushort type_idx;         //指向type_ids的索引，即对应 type_id_item
    //(字符串) 字段名称
    uint name_idx;           //指向string_ids的索引，即对应 string_id_item
}


struct method_id_item{
    //(类型)    所属类
    ushort class_idx;        //指向type_ids的索引，即对应 type_id_item
    //(多类型)  方法签名类型
    ushort proto_idx;        //指向proto_ids的索引，即对应 proto_id_item
    //(字符串)  方法名称
    uint name_idx;           //指向string_ids的索引，即对应 string_id_item
}


struct proto_id_item{
    //(字符串)  参数和返回值的类型的简单描述，，比如所有引用类型都用"L"统一表示
    uint shorty_idx;         //指向string_ids的索引，即对应 string_id_item
    //(类型)    返回值类型
    uint return_type_idx;    //指向type_ids的索引，即对应 type_id_item
    //(多类型2) 参数类型
    uint parameters_off;     //不为0，存储 type_list 的结构，用于描述函数参数的类型。
}


//数组，存放多个参数(类型)
struct type_list{
    uint size;               //size表示list数组的个数，而list数组元素类型为 type_item。
    type_item[] list;
}

struct type_item{
    ushort type_idx;         //指向type_ids的索引，即对应 type_id_item
}


//存储类信息
struct class_def{
    uint class_idx;          //指向type_ids，即对应 type_id_item ，代表本类的类型
    uint access_flags;       //访问标志，比如private、public等。
    uint superclass_idx;     //指向type_ids，代表基类类型，如果没有基类则取值为NO_INDEX（值为-1）
    
    uint interfaces_off;     //如果本类实现了某些接口，则 interfaces_off 指向文件对应位置，
                             //那里存储了一个 type_list。该type_list的list数组存储了每一个接口类的type_idx索引
    
    uint source_file_idx;    //指向string_ids，该类对应的源文件名
    uint annotations_off;    //存储和注解有关的信息
    
    uint class_data_off;     //指向文件对应位置，那里将存储更细节的信息，由 class_data_item 类型来描述
    
    uint static_values_off;  //储用来初始化类的静态变量的值，静态变量如果没有显示设置初值的话，默认是0或者null。
                             //如果有初值的话，初值信息就存储在文件static_values_off的地方，
                             //对应的数据结构名为encoded_array_item
                             //本章不拟讨论它，读者可自行阅读参考资料[2]。
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
    uleb128 access_flags;              //访问标志，比如private、public等
}

struct endoded_method{
    uleb128 method_idx_diff;           //指向 method_id_item
    uleb128 access_flags;              //访问权限
    uleb128 code_off;                  //指向文件对应位置处，那里有一个类型为 code_item 的结构体
                                       //code_item 类似于Class文件的Code属性，即存放指令 
}

//p60  存储方法的源码信息
stuct code_item{
    ushort regisrers_size;             //此函数需要用到的寄存器个数。
    ushort ins_size;                   //输入参数所占空间，以双字节为单位
    ushort out_size;                   //该函数表示内部调用其他函数时，所需参数占用的空间。同样以双字节为单位
    
    ushort tries_size;                 //①
    uint debug_info_size;
    
    uint insns_size;                   //指令码数组的长度
    ushort[] insns;                    //指令码的内容。Dex文件格式中JVM指令码长度为2个字节，而Class文件中JVM指令码长度为1个字节
    
    ushort padding;                    //① 用于将tries数组（如果有，并且insns_size是奇数长度的话）进行4字节对齐。
    try_item[] tries;                  //①
    encode_catch_handler_list handlers; //catch语句对应的内容，也是可选项。如果tries_size不为零才有handlers域。
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
    
