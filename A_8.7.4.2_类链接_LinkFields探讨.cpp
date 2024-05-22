//8.7.4.2　LinkFields探讨
//如何计算一个mirror Class对象所需的内存空间大小
//[class.cc->Class::ComputeClassSize]
inline uint32_t Class::ComputeClassSize(bool has_embedded_tables,
                                        uint32_t num_vtable_entries, 
                                        uint32_t num_8bit_static_fields,
                                        uint32_t num_16bit_static_fields, 
                                        uint32_t num_32bit_static_fields,
                                        uint32_t num_64bit_static_fields,
                                        uint32_t num_ref_static_fields,
                                        size_t pointer_size) {
    /*注意输入参数，num_8bit_static_fields 等代表某个Java类中所定义的占据1个字节（比如boolean）、
     2个字节（比如char、short）直到8个字节长度的数据类型及引用类型的静态成员变量的个数。 */
    //首先计算Class类本身所占据的内存大小，方法就是使用SizeOf(Class)，值为128。
    uint32_t size = sizeof(Class);
    if (has_embedded_tables) {      //计算隐含成员变量IMTable和VTable所需空间大小
        //其中，IMTable固定为64项
        const uint32_t embedded_imt_size = kImtSize * ImTableEntrySize(pointer_size);
        //计算VTable所需大小，非固定值
        const uint32_t embedded_vtable_size = num_vtable_entries * VTableEntrySize(pointer_size);
        //汇总两个Table所需的容量总和。另外，还需加上一个4字节空间，这里存储了VTable的表项个数
        size = RoundUp(size + sizeof(uint32_t), pointer_size) + embedded_imt_size + embedded_vtable_size;
    }
    /*VTable之后是类的静态成员所需空间。读者回顾本章在介绍ArtField那一节中最后的说明可知，
        ArtField不为它所代表的变量提供存储空间。这个存储空间有一部分交给了Class来承担。即，类的静
      态成员变量所需要的存储空间紧接在IMTable和VTable之后。  
    */



    //首先存储的是引用类型变量所需的空间。HeapReference<Object>其实就是所引用对象的地址，也就是一个指针。
    size += num_ref_static_fields * sizeof(HeapReference<Object>);
    
    /*接下来按数据类型所占大小由高到低分别存储各种类型静态变量所需的空间。不过，下面的代码会做一些优化  */
    if (!IsAligned<8>(size) && num_64bit_static_fields > 0) {
        /*先判断是否满足优化条件。如果上述计算得到的size值不能按8字节对齐（即不是8的整数倍），
          并且8字节长的静态变量个数不为0，则可以做一些优化。优化的目标也就是先计算size按8对齐
          后能留下来多少空间，然后用它来尽量多存储一些内容。笔者不拟介绍这部分代码。读者可自行
          阅读。*/
        }
    //最终计算得到的size大小
    size += num_8bit_static_fields * sizeof(uint8_t) +
            num_16bit_static_fields * sizeof(uint16_t) +
            num_32bit_static_fields * sizeof(uint32_t) +
            num_64bit_static_fields * sizeof(uint64_t);
    return size;
}

//如上所述，Class的大小除了包含sizeof(Class)之外，还包括IMTable、VTable（如果该类是可实例化的话）
//所需空间以及静态变量的空间。如果要想知道一个Class中存储用于存储静态变量的位置时，可利用下面这个函数获取。
//[class-inl.h->Class::GetFirstReferenceStaticFieldOffsetDuringLinking]
inline MemberOffset Class::GetFirstReferenceStaticFieldOffsetDuringLinking( size_t pointer_size) {
    //返回Class中用于存储静态成员变量的值的空间起始位置
    uint32_t base = sizeof(mirror::Class);
    if (ShouldHaveEmbeddedImtAndVTable()) {
        base = mirror::Class::ComputeClassSize(true, GetVTableDuringLinking()->GetLength(), 0, 0, 0, 0, 0, pointer_size);
    }
    return MemberOffset(base);
}



//8.7.4.2.2　LinkFields代码介绍
//[art_field.h]
class ArtField {
    private
		//如果ArtField所代表的成员变量是类的静态成员变量，则下面的offset_代表是该变量实际的存储空间在图8-13里Class内存布局中的起始位置。
		//如果是非静态成员变量，则offset_指向图8-13中Object内存布局里对应的位置。
		uint32_t offset_;
};

//现在可以来看 LinkFields 的代码了，虽然行数不少，但内容就不难了。
//[class_linker.cc->ClassLinker::LinkFields]
bool ClassLinker::LinkFields(Thread* self,
                             Handle<mirror::Class> klass,
                             bool is_static, size_t* class_size) {
    //确定成员变量的个数
    const size_t num_fields = is_static ? klass->NumStaticFields() : klass->NumInstanceFields();
    //从Class中得到代表静态或非静态成员变量的数组
    LengthPrefixedArray<ArtField>* const fields = is_static ? klass->GetSFieldsPtr() :  klass->GetIFieldsPtr();
    
    MemberOffset field_offset(0);
    if (is_static) {
        //如果是静态变量，则得到静态存储空间的起始位置。
        field_offset = klass->GetFirstReferenceStaticFieldOffsetDuringLinking(image_pointer_size_);
    } else {
        //获取基类的ObjectSize
        mirror::Class* super_class = klass->GetSuperClass();
        if (super_class != nullptr) {
            field_offset = MemberOffset(super_class->GetObjectSize());
        }
    }
    //排序，引用类型放最前面，然后是long/double、int/float等。符合图8-13中的布局要求
    std::deque<ArtField*> grouped_and_sorted_fields;
    for (size_t i = 0; i < num_fields; i++) {
        grouped_and_sorted_fields.push_back(&fields->At(i));
    }
    std::sort(grouped_and_sorted_fields.begin(), grouped_and_sorted_fields.end(),LinkFieldsComparator());
    size_t current_field = 0;
    size_t num_reference_fields = 0;
    FieldGaps gaps;
    
    //先处理引用类型的变量
    for (; current_field < num_fields; current_field++) {
        ArtField* field = grouped_and_sorted_fields.front();
        Primitive::Type type = field->GetTypeAsPrimitiveType();
        bool isPrimitive = type != Primitive::kPrimNot;
        if (isPrimitive) { break; }
        ......
        grouped_and_sorted_fields.pop_front();
        num_reference_fields++;
        field->SetOffset(field_offset);      //设置 ArtField 的 offset_ 变量
        field_offset = MemberOffset(field_offset.Uint32Value() + sizeof(mirror::HeapReference<mirror::Object>));
    }
    //我们在ComputeClassSize中曾提到说内存布局可以优化，下面的ShuffleForward就是处理这种优化。
    // ShuffleForward 是一个模板函数，内部会设置ArtField的offset_。
    ShuffleForward<8>(&current_field, &field_offset, &grouped_and_sorted_fields, &gaps);
    
    ...... //处理4字节、2字节基础数据类型变量
    
    ShuffleForward<1>(&current_field, &field_offset, &grouped_and_sorted_fields, &gaps);
    
    ......
    
    //特殊处理java.lang.ref.Reference类。将它的非静态引用类型变量的个数减去一个。减去的这个
    //变量是java Reference类中的referent成员，它和GC有关，需要特殊对待。后续章节会详细介绍GC。
    if (!is_static && klass->DescriptorEquals("Ljava/lang/ref/Reference;")) {
        --num_reference_fields;
    }
    
    size_t size = field_offset.Uint32Value();
    if (is_static) {
        klass->SetNumReferenceStaticFields(num_reference_fields);
        *class_size = size; //设置Class最终所需内存大小
    } else {
        klass->SetNumReferenceInstanceFields(num_reference_fields);
        ......
        //如果类的对象是固定大小（像数组、String则属于非固定大小），则设置Object所需内存大小
        if (!klass->IsVariableSize()) {
        ......
            klass->SetObjectSize(size);
        }
    }
    return true;
}