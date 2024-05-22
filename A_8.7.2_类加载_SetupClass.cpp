//[class_linker.h->ClassLinker::SetupClass]
//这个函数将把类的状态从kStatusNotReady切换为kStatusIdx
void SetupClass(const DexFile& dex_file,
				const DexFile::ClassDef& dex_class_def,
				Handle<mirror::Class> klass, 
				mirror::ClassLoader* class_loader);
			
			
			
			
//[class_linker.cc->ClassLinker::SetupClass]
void ClassLinker::SetupClass(const DexFile& dex_file,
							const DexFile::ClassDef& dex_class_def,
							Handle<mirror::Class> klass, mirror::ClassLoader* class_loader) {
    const char* descriptor = dex_file.GetClassDescriptor(dex_class_def);
	
    //SetClass是Class的基类mirror Object中的函数。Class也是一种Object，所以此处设置它的
    //类类型为”java/lang/Class”对应的那个Class对象
    klass->SetClass(GetClassRoot(kJavaLangClass));
    uint32_t access_flags = dex_class_def.GetJavaAccessFlags();
	
    //设置访问标志及该类的加载器对象
    klass->SetAccessFlags(access_flags);
    klass->SetClassLoader(class_loader);
	
    //设置klass的状态为kStatusIdx。
    mirror::Class::SetStatus(klass, mirror::Class::kStatusIdx, nullptr);
	
    //设置klass的dex_class_def_idx_和dex_type_idx_成员变量。
    klass->SetDexClassDefIndex(dex_file.GetIndexForClassDef(dex_class_def));
	klass->SetDexTypeIndex(dex_class_def.class_idx_);
	
}