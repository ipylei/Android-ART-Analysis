//【9.4.1】　Setup代码分析之一
//先来看Setup第一段代码。
//[dex2oat.cc->Dex2Oat::Setup第一段]
bool Setup() {
    TimingLogger::ScopedTiming t("dex2oat Setup", timings_);
    //MemMap我们在7.3.1节中见过了
    art::MemMap::Init();
    /* 下面这几个函数的功能：
    （1）PrepareImageClasses：读取--image-classes选项指定的文件，
            本例是/system/etc/pre-loaded-classes，该文件的内容将存储到image_classes_成员变量，
            其数据类型是unique_ptr<unordered_set<string>>。
    （2）PrepareCompiledClasses：读取--compiled-classes 选项指定的文件，
        本例是/system/etc/compiled-classes，其内容将存储到 compiled_classes_ 成员变量，
        其数据类型也是unique_ptr<unordered_set<string>>。
    （3）PrepareCompiledMethods：和--compiled-methods 选项有关，
        对应的成员变量是 compiled_methods_。本例并未使用这个选项。  */
    if (!PrepareImageClasses() || !PrepareCompiledClasses()  || !PrepareCompiledMethods()) {
            return false;  
    }
    // verification_results_：指向一个 VerificationResults 对象，下文将介绍它。
    verification_results_.reset(new VerificationResults(compiler_options_.get()));
    //callbacks_：指向一个 QuickCompilerCallbacks 对象。详情见下文介绍
    callbacks_.reset(
            new QuickCompilerCallbacks(verification_results_.get(),&method_inliner_map_,
                                        IsBootImage() ?       //本例是针对boot镜像的编译
                                        CompilerCallbacks::CallbackMode::kCompileBootImage 
                                        :CompilerCallbacks::CallbackMode::kCompileApp)
            );
            
} 
            
//上述代码的调用流程并不复杂，但其中涉及几个比较重要的数据类型，笔者将先介绍它们。
//【9.4.1.1】　ClassReference 等
            
//[class_reference.h-> ClassReference 类型别名定义]
typedef std::pair<const DexFile*, uint32_t> ClassReference;

/*
ClassReference是类型别名，在 pair 的两个模板参数中，
第一个模板参数代表DexFile对象（标示一个Dex文件），
第二个模板参数为uint_32_t，它用于表示某个类的信息在该Dex文件中类信息表
    （class_defs，读者可回顾第3章的图3-3）里的索引（即第3章图3-6中ClassDef数据结构中的class_idx）。
与ClassReference作用类似的一个数据类型还有 MethodReference，其定义如下。
*/
//[method_reference.h->MethodReference]
struct MethodReference {
    ....... //构造函数
    
    //代表一个Dex文件。
    const DexFile* dex_file; 
    
    //一个Java方法在Dex文件里 method_ids 中的索引，请读者参考第3章图3-3。
    uint32_t dex_method_index;
};            


//ART中还有一个DexFileReference，其代码如下所示。值得指出的是，
//它的index成员变量的含义与MethodReference dex_method_index的含义一样，代表某个Java方法在dex文件里method_ids中的索引。
//[dex_file.h->DexFileReference]
struct DexFileReference {
    ..... //构造函数
    const DexFile* dex_file;
    //该成员变量的含义与MethodReference dex_method_index一样
    uint32_t index; 
};



//[verified_method.cc->VerifiedMethod::GenerateDequickenMap 中创建 DexFileReference 示例]
....
ArtMethod* method = ......
//ArtMethod GetDexMethodIndex函数返回的就是该Java方法在对应dex文件method_ids数组中的索引。
dequicken_map_.Put(dex_pc, DexFileReference(method->GetDexFile(), method->GetDexMethodIndex()));
....





//【9.4.1.2】　VerifiedMethod
//本节来认识VerifiedMethod类，它代表一个校验通过的Java方法，其类声明如下所示。
//[verified_method.h->VerifiedMethod]
class VerifiedMethod {
        public:
        typedef std::vector<uint32_t> SafeCastSet;
        //SafeMap是ART提供的类似STL map的容器类。
        typedef SafeMap<uint32_t, MethodReference> DevirtualizationMap;
        //处理dex中quick指令，下文介绍dex到dex指令优化时将见相关处理过程
        typedef SafeMap<uint32_t, DexFileReference> DequickenMap;
        //创建一个VerifiedMethod对象。下文将重点介绍该函数
        static const VerifiedMethod* Create(verifier::MethodVerifier* method_verifier, bool compile)
        const MethodReference* GetDevirtTarget(uint32_t dex_pc) const;

        private:
        ......
        void GenerateDevirtMap(verifier::MethodVerifier* method_verifier)
        ......
        DevirtualizationMap devirt_map_;
        DequickenMap dequicken_map_;
        SafeCastSet safe_cast_set_;
        ......
};


//[verified_method.cc->VerifiedMethod::Create]
const VerifiedMethod* VerifiedMethod::Create(verifier::MethodVerifier* method_verifier, bool compile) {
    //创建一个VerifiedMethod对象，调用VerifiedMethod的构造函数。这部分内容比较简单，请读者自行阅读
    std::unique_ptr<VerifiedMethod> verified_method(
        new VerifiedMethod(method_verifier->GetEncounteredFailureTypes(),
                         method_verifier->HasInstructionThatWillThrow()));

    if (compile) {//compile为true时表示这个方法将会被编译。
        //如果这个Java方法中有invoke-virtual或invoke-interface相关的指令，则下面if的条件满足
        if (method_verifier->HasVirtualOrInterfaceInvokes()) {
            //去虚拟化。下面将介绍这个函数
            verified_method->GenerateDevirtMap(method_verifier);
        }
        ......
    return verified_method.release();
}



//【优化：去虚拟化】
//[verified_method.cc->VerifiedMethod::GenerateDevirtMap]
void VerifiedMethod::GenerateDevirtMap(verifier::MethodVerifier* method_verifier) {
    if (method_verifier->HasFailures()) { return; }
    
    /*method_verifier类型为MethodVerifier，它包含了已经校验通过的Java方法的信息。
        CodeItem函数返回这个Java方法对应的dex字节码。CodeItem类对应第3章图3-8中的code_item伪数据结构。*/
    const DexFile::CodeItem* code_item = method_verifier->CodeItem();
    
    //insns 为存储dex字节码的数组
    const uint16_t* insns = code_item->insns_;
    const Instruction* inst = Instruction::At(insns);
    const Instruction* end = Instruction::At(insns +code_item->insns_size_in_code_units_);
    
    //遍历 dex 字节码数组
    for (; inst < end; inst = inst->Next()) {
        //如果该指令的操作码是invoke-virtual，则说明该Java方法中包含虚拟函数调用
        const bool is_virtual = inst->Opcode() == Instruction::INVOKE_VIRTUAL || inst->Opcode() == Instruction::INVOKE_VIRTUAL_RANGE;
        const bool is_interface = ......;      //判断Java方法里是否有调用接口函数
        
        //略过非invoke-virtual和invoke-interface相关的指令
        if (!is_interface && !is_virtual) { continue; }
        uint32_t dex_pc = inst->GetDexPc(insns);
        //获取这条函数调用指令里用到的操作数。
        verifier::RegisterLine* line = method_verifier->GetRegLine(dex_pc);
        const bool is_range = .....;
        //VRregC_35c是ART提供的获取指令中指定虚拟寄存器值的辅助函数。reg_type存储了该虚拟
        //寄存器的值。对invoke-virtual/interface调用指令来说，它代表在哪个对象上发起了函数调用
        const verifier::RegType& reg_type(line->GetRegisterType(method_verifier, is_range ? inst->VRegC_3rc() :inst->VRegC_35c()));
        if (!reg_type.HasClass()) { continue; }
        // reg_class代表发起函数调用的对象所属类的类型
        mirror::Class* reg_class = reg_type.GetClass();
        //如果reg_class类是一个接口类，则无法做去虚拟化优化
        if (reg_class->IsInterface()) { continue; }
        ......
        //可结合图9-5的示例来理解下面的代码
        auto* cl = Runtime::Current()->GetClassLinker();
        size_t pointer_size = cl->GetImagePointerSize();
        //获取所调用的目标函数的ArtMethod对象，以图9-5中OatTest为例，这里得到的是Object的toString函数
        ArtMethod* abstract_method = method_verifier->GetDexCache()->GetResolvedMethod(is_range ? inst->VRegB_3rc() : inst->VRegB_35c(), pointer_size);
        
        if (abstract_method == nullptr) { continue; }
        ArtMethod* concrete_method = nullptr;
        if (is_interface) {......}
        //reg_type 所属类为OatTest，我们搜索该类中重载了toString的函数，所以， concrete_method 返回的是OatTest toString函数
        if (is_virtual) {
            concrete_method = reg_type.GetClass()->FindVirtualMethodForVirtual(abstract_method, pointer_size);
        }
        ......
        if (reg_type.IsPreciseReference() || concrete_method->IsFinal() ||
            concrete_method->GetDeclaringClass()->IsFinal()) {
            //将函数调用指令的位置（dex_pc）和具体方法的信息保存到 devirt_map_ 中，后续编译为机器码时将做相关替换。
            devirt_map_.Put(dex_pc, concrete_method->ToMethodReference());
        }
    }
}
