//【9.1 概述】
//[dex2oat.cc->main]
int main(int argc, char** argv) {
    int result = art::dex2oat(argc, argv); //调用dex2oat函数
    return result;
}

//[dex2oat.cc->dex2oat]
static int dex2oat(int argc, char** argv) {
    TimingLogger timings("compiler", false, false);
    //MakeUnique：art中的辅助函数，用来创建一个由unique_ptr智能指针包裹的目标对象
    std::unique_ptr<Dex2Oat> dex2oat = MakeUnique<Dex2Oat>(&timings);
    
	//①解析参数
	//【9.2】
    dex2oat->ParseArgs(argc, argv);
    
	.... //是否基于profile文件对热点函数进行编译。本书不讨论与之相关的内容
    
	//②打开输入文件
	//【9.3】
	dex2oat->OpenFile(); 
    
	//③准备环境
	//【9.4】
	dex2oat->Setup(); 

    bool result;
    //镜像有boot image和app image两大类，镜像文件是指.art文件
    if (dex2oat->IsImage()) {
		
		//④编译boot镜像或app镜像
        result = CompileImage(*dex2oat); 
		
    } else {
        //编译app，但不生成art文件（即镜像文件）。其内容和CompileImage差不多，只是少了生成.art文件的步骤。
        result = CompileApp(*dex2oat);
    }
    dex2oat->Shutdown(); //清理工作
    return result;
}