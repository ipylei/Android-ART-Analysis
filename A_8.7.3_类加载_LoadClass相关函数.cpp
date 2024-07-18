//[class_linker.cc->ClassLinker::LoadClass]
void ClassLinker::LoadClass(Thread * self,
							const DexFile & dex_file,
							const DexFile::ClassDef & dex_class_def,
							Handle < mirror::Class > klass) {

    //class_data的内容就是图8-7中的 class_data_item
    const uint8_t * class_data = dex_file.GetClassData(dex_class_def);

    if (class_data == nullptr) {
        return;
    }

    bool has_oat_class = false;
    if (Runtime::Current()->IsStarted() && !Runtime::Current()->IsAotCompiler()) {
        //如果不是编译虚拟机的话，则先尝试找到该类经dex2oat编译得到的OatClass信息
        OatFile::OatClass oat_class = FindOatClass(dex_file, klass->GetDexClassDefIndex(),  & has_oat_class);
        if (has_oat_class) {
            LoadClassMembers(self, dex_file, class_data, klass,  & oat_class);
        }
    }

    //不管有没有OatClass信息，最终调用的函数都是 LoadClassMembers。
    if (!has_oat_class) {
        LoadClassMembers(self, dex_file, class_data, klass, nullptr);
    }
}

//8.7.3.1.1　LoadClassMembers  如其名所示，LoadClassMembers 将为目标Class对象加载类的成员，代码如下所示。
//[class_linker.cc->ClassLinker::LoadClassMembers]
void ClassLinker::LoadClassMembers(Thread * self,
									const DexFile & dex_file,   //这是一个脱壳点
									const uint8_t * class_data,
									Handle < mirror::Class > klass,
									const OatFile::OatClass * oat_class) {
										
    //注意这个函数的参数，class_data 为 dex文件里的代表该类的 class_data_item 信息，
    //而 oat_class 描述的是Oat文件里针对这个类提供的一些信息
    {
        LinearAlloc * const allocator = GetAllocatorForClassLoader(klass->GetClassLoader());
        //创建 class_data_item 迭代器
        ClassDataItemIterator it(dex_file, class_data);
		
		
		
        //分配用于存储目标类静态成员变量的固定长度数组sfields
        LengthPrefixedArray < ArtField >  * sfields = AllocArtFieldArray(self, allocator, it.NumStaticFields());
        size_t num_sfields = 0;
        uint32_t last_field_idx = 0u;
        //遍历 class_data_item 中的静态成员变量数组，然后填充信息到 sfields 数组里
        for (; it.HasNextStaticField(); it.Next()) {
            uint32_t field_idx = it.GetMemberIndex();
            if (num_sfields == 0 || LIKELY(field_idx > last_field_idx)) {
                
				//加载这个 ArtField 的内容。下文将单独介绍此函数
				//【*】加载并设置到 ArtField
                LoadField(it, klass, &sfields->At(num_sfields));
                
				++num_sfields;
                last_field_idx = field_idx;
            }
        }
		
		

        // 同理，分配代表该类非静态成员变量的数组
        LengthPrefixedArray < ArtField >  * ifields = AllocArtFieldArray(self, allocator, it.NumInstanceFields());
        size_t num_ifields = 0u;
        last_field_idx = 0u;
		//遍历 class_data_item 中的非静态成员变量数组，然后填充信息到 ifields 数组里
        for (; it.HasNextInstanceField(); it.Next()) {
            uint32_t field_idx = it.GetMemberIndex();
            if (num_ifields == 0 || LIKELY(field_idx > last_field_idx)) {
				//【*】加载并设置到ArtField
                LoadField(it, klass,  & ifields->At(num_ifields)); //类似的处理
                ++num_ifields;
                last_field_idx = field_idx;
            }
        }


        //设置Class类的 sfields_ 和 ifields_成员变量
        klass->SetSFieldsPtr(sfields);
        klass->SetIFieldsPtr(ifields);
        /*设置Class类的 methods_ 成员变量。读者可回顾笔者对该成员变量的解释，
			它是一个 LengthPrefixedArray<ArtMethod> 数组，其元素布局为
			（1）[0,virtual_methods_offset_)为本类包含的 direct 成员函数
			（2）[virtual_methods_offset_,copied_methods_offset_)为本类包含的 virtual 成员函数
			（3）[copied_methods_offset_,...)为剩下的诸如 miranda 函数等内容。
			
		下面代码中，先分配1和2所需要的元素空间，然后设置klass对应的成员变量，其中：
        klass-> methods_ 为AllocArtMethodArray的返回值，
        klass-> copied_methods_offset_ 为类direct和virtual方法个数之和
        klass-> virtual_methods_offset_ 为类direct方法个数   */
        klass->SetMethodsPtr(
				AllocArtMethodArray(self, allocator, it.NumDirectMethods() + it.NumVirtualMethods()), 
				it.NumDirectMethods(), 
				it.NumVirtualMethods()
			);
			
			
			
        size_t class_def_method_index = 0;
        uint32_t last_dex_method_index = DexFile::kDexNoIndex;
        size_t last_class_def_method_index = 0;
        //遍历 direct 方法数组，加载它们然后关联字节码
        for (size_t i = 0; it.HasNextDirectMethod(); i++, it.Next()) {
            ArtMethod * method = klass->GetDirectMethodUnchecked(i, image_pointer_size_);
			
			//【*】加载并设置到ArtMethod
            //加载 ArtMethod 对象，并将其和字节码关联起来。
            LoadMethod(self, dex_file, it, klass, method);
            //注意，oat_class 信息只在LinkCode中用到。LinkCode留待10.1节介绍
            LinkCode(method, oat_class, class_def_method_index);
			
            uint32_t it_method_index = it.GetMemberIndex();
            if (last_dex_method_index == it_method_index) {
                method->SetMethodIndex(last_class_def_method_index);
            } else { 
				//设置ArtMethod的 method_index_ ，该值其实就是这个ArtMethod位于上面 klass methods_ 数组中的位置
                method->SetMethodIndex(class_def_method_index);
                
                last_dex_method_index = it_method_index;
                last_class_def_method_index = class_def_method_index;
            }
            class_def_method_index++;
        }
		
        //处理virtual方法。注意，对表示virtual方法的ArtMethod对象而言，
		//它们的 method_index_ 和 klass  methods_ 数组没有关系，也不在下面的循环中设置。
        for (size_t i = 0; it.HasNextVirtualMethod(); i++, it.Next()) {
			
            //和direct方法处理一样，唯一不同的是，此处不调用ArtMethod的 SetMethodIndex 函数，
			//即不设置它的 method_index_ 成员
			
			ArtMethod* method = klass->GetVirtualMethodUnchecked(i, image_pointer_size_);
			//【*】加载并设置到ArtMethod
            //加载ArtMethod对象，并将其和字节码关联起来。
			LoadMethod(self, dex_file, it, klass, method);
			//注意，oat_class 信息只在LinkCode中用到。LinkCode留待10.1节介绍
			LinkCode(method, oat_class, class_def_method_index);
        
			class_def_method_index++;
		}
    }
	
	// Ensure that the card is marked so that remembered sets pick up native roots.
    Runtime::Current()->GetHeap()->WriteBarrierEveryFieldOf(klass.Get());
    self->AllowThreadSuspension();
}
