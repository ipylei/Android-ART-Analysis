//[dex2oat.cc->Dex2Oat::CompileImage]
static int CompileImage(Dex2Oat& dex2oat) {
    //加载profile文件，对基于profile文件的编译有效，本例不涉及它
    dex2oat.LoadClassProfileDescriptors();
	
	//①编译
    dex2oat.Compile(); 
	
    if(!dex2oat.WriteOatFiles()){
		//②输出.oat文件
		......
	};   
	
    .....
	
    //③处理.art文件
    if (!dex2oat.HandleImage()) {
			......
	}
    .....　//其他处理，内容非常简单。感兴趣的读者可自行阅读
	
    return EXIT_SUCCESS;
}