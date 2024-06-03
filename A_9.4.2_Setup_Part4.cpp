//[dex2oat.cc->Dex2Oat::Setup第四段]
{
    if (IsBootImage()) {//本例满足此条件，代表编译boot镜像
        //下面这行代码将为创建的Runtime对象设置boot类的来源
        runtime_options.Set(RuntimeArgumentMap::BootClassPathDexList, &opened_dex_files_);
		
        /*创建art runtime对象。CreateRuntime是Dex2Oat的成员函数，比较简单，读者可自行阅
          读。注意，CreateRuntime函数所创建的runtime对象只是做了Init操作，没有执行它的Start
          函数。所以，该runtime对象也叫unstarted runtime。   */
        if (!CreateRuntime(std::move(runtime_options))) {
			return false;
		}
    }
    //初始化其他关键模块
    Thread* self = Thread::Current();
    WellKnownClasses::Init(self->GetJniEnv());
    ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
    .....
    for (const auto& dex_file : dex_files_) {
        ScopedObjectAccess soa(self);
        //往class_linker中注册dex文件对象和对应的class_loader_。对本例而言，class_loader_
        //为空，代表boot类加载器。
        dex_caches_.push_back(soa.AddLocalReference<jobject>(
				class_linker->RegisterDexFile(*dex_file, soa.Decode<mirror::ClassLoader*>(class_loader_))
			)
		);
    }
    return true;
}