//8.7.3.1.2　LoadFields
//成员变量的信息加载比较简单，入口函数的代码如下所示。
//[class_linker.cc->ClassLinker::LoadField]
void ClassLinker::LoadField(const ClassDataItemIterator& it,
                            Handle<mirror::Class> klass, 
							ArtField* dst) {
    const uint32_t field_idx = it.GetMemberIndex();
	
    dst->SetDexFieldIndex(field_idx);              //设置对应于dex文件里的那个 field_dex_idx_
    dst->SetDeclaringClass(klass.Get()); //设置本成员变量由哪个Class对象定义 => declaring_class_
    dst->SetAccessFlags(it.GetFieldAccessFlags()); //设置访问标记
}




//8.7.3.1.3　LoadMethod
//接着来看成员方法的加载，代码如下所示。
//[class_linker.cc->ClassLinker::LoadMethod]
void ClassLinker::LoadMethod(Thread* self,
							const DexFile& dex_file,
							const ClassDataItemIterator& it,
							Handle<mirror::Class> klass,  
							ArtMethod* dst) {
								
    uint32_t dex_method_idx = it.GetMemberIndex();
    const DexFile::MethodId& method_id = dex_file.GetMethodId(dex_method_idx);
    const char* method_name = dex_file.StringDataByIdx(method_id.name_idx_);
	
    //设置ArtMethod declaring_class_ 和 dex_method_index_ 和成员变量
    dst->SetDexMethodIndex(dex_method_idx);
    dst->SetDeclaringClass(klass.Get());
	
    //设置 dex_code_item_offset_ 成员变量
    dst->SetCodeItemOffset(it.GetMethodCodeItemOffset());
	
    //设置ArtMethod ptr_sized_fields_ 结构体中的 dex_cache_resolved_methods_ 
	//和 dex_cache_resolved_types_ 成员。读者可回顾上文对ArtMethod成员变量的介绍
    dst->SetDexCacheResolvedMethods(klass->GetDexCache()->GetResolvedMethods(), image_pointer_size_);
    dst->SetDexCacheResolvedTypes(klass->GetDexCache()->GetResolvedTypes(), image_pointer_size_);

    uint32_t access_flags = it.GetMethodAccessFlags();
    //处理访问标志。比如，如果函数名为 ”finalize” 的话，设置该类为 finalizable
    if (UNLIKELY(strcmp("finalize", method_name) == 0)) {
        if (strcmp("V", dex_file.GetShorty(method_id.proto_idx_)) == 0) {
            //该类的class loader如果不为空，则表示不是boot class，也就是系统所必需的那些基础类
            if (klass->GetClassLoader() != nullptr){
                //设置类的访问标记，增加 kAccClassIsFinalizable。表示该类重载了finalize函数
                klass->SetFinalizable();
            } else {
				......      
			}
        }
    } else if (method_name[0] == '<') {
        ......//如果函数名为”<init>”或”<clinit>”，则设置访问标志位 kAccConstructor
    }
    dst->SetAccessFlags(access_flags);
}



//8.7.3.2　LoadSuperAndInterfaces 

/*
DefineClass
	LoadClass
	LoadSuperAndInterfaces

上一节的LoadClass中，目标类在dex文件对应的class_def里相关的信息已经提取并分别保存到
代表目标类的Class对象、相应的ArtField和ArtMethod成员中了。

接下来，如果目标类有基类或实现了接口类的话，我们相应地需要把它们“找到”。
“找到”是一个很模糊的词，它到底包含什么工作呢？来看代码
*/
//[class_linker.cc->ClassLinker::LoadSuperAndInterfaces]
bool ClassLinker::LoadSuperAndInterfaces(Handle<mirror::Class> klass,
										const DexFile& dex_file) {
    const DexFile::ClassDef& class_def = dex_file.GetClassDef(klass->GetDexClassDefIndex());
    
	//找到基类的id
    uint16_t super_class_idx = class_def.superclass_idx_;
    //根据基类的id来解析它，返回值是代表基类的Class实例。
    //ResolveType 函数的内容将在8.7.8节中介绍
    //【8.7.8】
    mirror::Class* super_class = ResolveType(dex_file, super_class_idx, klass.Get());
	
    if (super_class == nullptr) { 
		return false;  
	}
    //做一些简单校验。比如，基类如果不允许派生，则返回失败
    if (!klass->CanAccess(super_class)) { 
		return false; 
	}
	
	//设置super_class_成员变量的值
	klass->SetSuperClass(super_class);
                  
    //下面这个检查和编译有关系，笔者不拟讨论它，代码中的注释非常详细
    if (!CheckSuperClassChange(klass, dex_file, class_def, super_class)) {
        return false;
    }
	
	
    //从dex文件里找到目标类实现了哪些接口类。参考图8-7所示class_def结构体中 interfaces_off 的含义
    const DexFile::TypeList* interfaces = dex_file.GetInterfacesList(class_def);
    if (interfaces != nullptr) {
        for (size_t i = 0; i < interfaces->Size(); i++) {
            uint16_t idx = interfaces->GetTypeItem(i).type_idx_;
		  
			//解析这个接口类。下文将介绍ResolveType函数
			mirror::Class* interface = ResolveType(dex_file, idx, klass.Get());
			......      
			//如果接口类找不到或者接口类不允许继承，则返回错误
			if (interface == nullptr) {
				DCHECK(Thread::Current()->IsExceptionPending());
				return false;
			}
        }
    }
    //设置klass的状态为 kStatusLoaded
    mirror::Class::SetStatus(klass, mirror::Class::kStatusLoaded, nullptr);
    return true;
}
