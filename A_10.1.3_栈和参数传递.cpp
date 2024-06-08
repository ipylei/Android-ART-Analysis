//【10.1.3.1】　kSaveAll、kRefsOnly和kRefsAndArgs
//[asm_support.h]
//callee_save_methods_[kSaveAll]是0号元素，它相对于数组基地址的偏移量是0
#define RUNTIME_SAVE_ALL_CALLEE_SAVE_FRAME_OFFSET 0
//callee_save_methods_[kRefsOnly]是1号元素，它相对于数组基地址的偏移量是8。因为该数组元素的数据类型为uint64_t。
#define RUNTIME_REFS_ONLY_CALLEE_SAVE_FRAME_OFFSET 8
//callee_save_methods_[kRefsAndArgs]是二号元素，它相对于数组基地址的偏移量是16。
#define RUNTIME_REFS_AND_ARGS_CALLEE_SAVE_FRAME_OFFSET (2 * 8)




//【10.1.3.1.1】　kSaveAll相关
//本节先介绍kSaveAll的情况，它对应为 callee_save_methods_[kSaveAll] 所指向的那个Runtime ArtMethod对象。
//quick_entrypoints_x86.S有几个宏与之相关，我们先看其中的一个，代码如下所示。
//[quick_entrypoints_x86.S-> SETUP_SAVE_ALL_CALLEE_SAVE_FRAME 宏定义]
#下面是汇编中的宏定义。宏名称为SETUP_SAVE_ALL_CALLEE_SAVE_FRAME，有两个代表寄存器的参数got_reg和temp_reg。
//从宏命名来看，它的功能和建立栈帧有关
MACRO2(SETUP_SAVE_ALL_CALLEE_SAVE_FRAME, got_reg, temp_reg)
    PUSH edi      #接下来的四条指令属于寄存器入栈，并拓展栈空间
    PUSH esi
    PUSH ebp
    subl MACRO_LITERAL(12), %esp
    #下面三行代码（其中第一条 SETUP_GOT_NOSAVE 为宏）用于将Runtime类的instance_对象的地址存储到temp_reg寄存中。
    //注意，这个instance_即是ART虚拟机进程中唯一的一个Runtime对象。在C++代码中，Runtime::Current函数返回的就是这个instance_。
    SETUP_GOT_NOSAVE RAW_VAR(got_reg)
    movl SYMBOL(_ZN3art7Runtime9instance_E)@GOT(REG_VAR(got_reg)),REG_VAR(temp_reg)
    movl (REG_VAR(temp_reg)), REG_VAR(temp_reg)
    #下面这行指令将instance_-> callee_save_methods_[kSaveAll]的地址压入栈中
    pushl RUNTIME_SAVE_ALL_CALLEE_SAVE_FRAME_OFFSET(REG_VAR(temp_reg))
    #将ESP寄存器的值保存到Thread对象的指定成员变量中。详情见下文介绍。
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET
    
    #下面的几行代码是编译时做校验的。判断 FRAME_SIZE_SAVE_ALL_CALLEE_SAVE 变量的大小是否为32。
    #该变量定义在asm_support_x86.h文件中。详情见下文。
    #if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVE != 3*4 + 16 + 4)
    #error "SAVE_ALL_CALLEE_SAVE_FRAME(X86) size not as expected."
    #endif
END_MACRO




//【10.1.3.1.2】　kRefsOnly相关
//接着来看callee_save_methods_[kRefsOnly]，汇编代码中和它相关的有几个宏，我们先研究其中的两个。代码如下所示。
//[quick_entrypoints_x86.S]
# SETUP_REFS_ONLY_CALLEE_SAVE_FRAME用于处理 callee_save_methods_[kRefsOnly]
MACRO2(SETUP_REFS_ONLY_CALLEE_SAVE_FRAME, got_reg, temp_reg)
#相比上面的宏，下面的SETUP_REFS_ONLY_CALLEE_SAVE_FRAME_PRESERVE_GOT_REG执行完后将恢复 got_reg 寄存器的值。
MACRO2(SETUP_REFS_ONLY_CALLEE_SAVE_FRAME_PRESERVE_GOT_REG, got_reg,temp_reg)


/*

ShadowFrame：该类用于描述解释执行模式下某个函数对应的栈帧。其中一些关键成员变量的含义或作用如下所示。
■method_：ShadowFrame对象所关联的、代表某Java方法的ArtMethod对象。
■code_item_：该Java方法对应的、来自dex文件的dex指令码。
■dex_pc_：从dex指令码dex_pc_指定的位置处执行。该变量往往为0，表示从函数的第一条指令开始执行。但正如本章开头所说，如果一个方法因HDeoptimization而从机器码执行模式进入解释执行模式的话，dex_pc_取值为需要解释执行的指令码的位置。
■link_：函数A有一个ShadowFrame对象，而函数A进入其内部调用的某个函数B时，B对应也有一个ShadowFrame对象。B的ShadowFrame对象通过link_成员变量指向A的ShadowFrame对象。从B返回后，它的ShadowFrame对象将被回收。
■vregs_：代表该函数所需的参数。详情见下文代码介绍。
*/


//【10.1.3.2】　遍历栈中的参数
//ShadowFrame 类的代码如下所示。
//[stack.h->ShadowFrame类声明]
class ShadowFrame { //为方便展示，此处对成员变量和成员函数的位置有所调整
    private:
        ...... /*ShadowFrame成员变量定义，我们重点关注下面两个成员变量*/
        /*
        number_of_vregs_：取值为code_item中的register_size，表示本方法用到的虚拟寄存器的个数。读者可回顾3.2.4节中的图3-8。
        vregs_[0]：这是一个数组，实际长度为number_of_vregs_*2。该数组的内容分为前后两个部分，分别是：
        （1）[0,number_of_vregs_)：前半部分位置存储各个虚拟寄存器的值。
        （2）[number_of_vregs_, number_of_vregs_*2)：后半部分位置存储的是引用类型参数的值。
        举个例子，假设一个方法用到了4个虚拟寄存器v0、v1、v2和v3，则number_of_vregs_为4，vregs_ 数组的实际长度为8。
        如果虚拟寄存器v2里：
            （1）保存的是一个整数，值为10，那么 vregs_[2]存储的值是10，而vregs_[4+2]的值是nullptr。
            （2）保存的是一个引用型参数，那么 vregs_[2]和vregs_[6]存储的是这个引用型参数的地址。
        下文代码分析时还会介绍相关的函数。 */
        const uint32_t number_of_vregs_;
        uint32_t vregs_[0];
    public:
        //根据num_vregs计算对应的ShadowFrame实例需要多大的空间。
        static size_t ComputeSize(uint32_t num_vregs) {
                return sizeof(ShadowFrame) + (sizeof(uint32_t) * num_vregs) + (sizeof(StackReference<mirror::Object>) * num_vregs);
        }
};



//在代码中，ShadowFrame 的实例并不是在进程的堆空间中创建，而是利用alloca在调用函数的栈中先分配一块空间，
//然后利用placement new在这块空间中创建ShadowFrame实例。来看代码。
//[stack.h->CREATE_SHADOW_FRAME]
#define CREATE_SHADOW_FRAME(num_vregs, link, method, dex_pc) \
    ({  \
        //先根据num_vregs计算需要多大的空间
        size_t frame_size = ShadowFrame::ComputeSize(num_vregs);  \
        //alloca在调用这个宏的函数的栈空间上分配对应的内存
        void* alloca_mem = alloca(frame_size);  \
        //CreateShadowFrameImpl是一个函数，内部通过placement new在alloca_mem内存空间上构造一个ShadowFrame实例。
        ShadowFrameAllocaUniquePtr( ShadowFrame::CreateShadowFrameImpl((num_vregs), (link), (method), (dex_pc),(alloca_mem)));                                   \
})



//ShadowFrameAllocaUniquePtr 是std unique_ptr智能指针模板类特例化后的类型别名，其定义如下所示。
//[stack.h->ShadowFrameAllocaUniquePtr]
struct ShadowFrameDeleter;
using ShadowFrameAllocaUniquePtr = std::unique_ptr<ShadowFrame, ShadowFrameDeleter>;
//如上文所述，ShadowFrame类的实例是创建在栈上的，所以它的对象需要主动去析构。
//这是由unique_ptr实例在析构时主动调用ShadowFrameDeleter类的函数调用运算符来完成的。
//不熟悉这部分C++的读者可阅读第5章的相关内容。
struct ShadowFrameDeleter {
    inline void operator()(ShadowFrame* frame) {
        if (frame != nullptr) {
            frame->~ShadowFrame();
        }
    }
};




//现在我们来看看如何往 ShadowFrame vregs_ 数组中存取参数。下面的代码展示了【整型参数】的设置和存取。
//[stack.h->ShadowFrame SetVReg 和 GetVRegArgs]
void SetVReg(size_t i, int32_t val) {
    //调试用语句，i必须小于 number_of_vregs_
    DCHECK_LT(i, NumberOfVRegs());
    //取值索引位置为i的元素的地址
    uint32_t* vreg = &vregs_[i];
    //对该元素赋值。
    *reinterpret_cast<int32_t*>(vreg) = val;
    //kMovingCollector 是编译常量，默认为true，它和GC有关，
    //HasReferenceArray 是ShadowFrame的成员函数，返回值固定为true
    if (kMovingCollector && HasReferenceArray()) {
        References()[i].Clear();
    }
}

//取值索引位置为i的元素的地址。
uint32_t* GetVRegArgs(size_t i) {
    return &vregs_[i];
}



//现在来看【引用型参数】的设置。如上文所述，引用型参数在 vregs_ 数组中两个部分均需要设置。下面的函数用于返回vregs_后半部分。
//[stack.h->ShadowFrame::References]
const StackReference<mirror::Object>* References() const {
    /*直接将vregs_后半部分的空间作为一个 StackReference<Object>数组返回。这段代码同时表明引
      用型参数的数据类型是StackReference<Object>，简单点说，就是Object*。
      注意，此处为行文简便，省略了Object类型前的命名空间mirror。 
    */
    const uint32_t* vreg_end = &vregs_[NumberOfVRegs()];
    return reinterpret_cast<const StackReference<mirror::Object>*>(vreg_end);
}

//下面两个 ShadowFrame 的成员函数用于设置和获取引用型参数。
//[stack.h->ShadowFrame::SetVRegReference 和 GetVRegReference]
template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
void SetVRegReference(size_t i, mirror::Object* val) {
    ...... //注意，输入参数val的类型是Object*，代表一个引用型参数。
    uint32_t* vreg = &vregs_[i];
    //先存储在vregs_前半部分对应的索引位置上
    reinterpret_cast<StackReference<mirror::Object>*>(vreg)->Assign(val);
    //接下来设置vregs_后半部分对应的索引
    if (HasReferenceArray()) {　//HasReferenceArray永远返回true
        References()[i].Assign(val);
    }
}

template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
mirror::Object* GetVRegReference(size_t i) const {
    //注意，该函数返回值的类型是Object*。
    mirror::Object* ref;
    if (HasReferenceArray()) {
        ref = References()[i].AsMirrorPtr();
    } else {
        .....
    }
    ......
    return ref;
}



/*前文介绍
·QuickArgumentVisitor：辅助类，用于访问 kRefsAndArgs 对应的栈帧中的参数。kRefsAndArgs 对应的栈帧布局可参考图10-4。下文将在此基础上展开介绍。
·BuildQuickShadowFrameVisitor：派生自QuickArgumentVisitor。用于访问解释执行模式下栈帧中的参数——解释执行模式下，
    函数对应的栈帧使用ShadowFrame来描述，所以该类的类名中包含ShadowFrame一词。
*/

//BuildQuickShadowFrameVisitor 用于将参数填充到一个 ShadowFrame 对象中。先来看它的构造函数。
//[quick_trampoline_entrypoints.cc->BuildQuickShadowFrameVisitor]
class BuildQuickShadowFrameVisitor FINAL : public QuickArgumentVisitor {
    public:
        BuildQuickShadowFrameVisitor(ArtMethod** sp, 
                                    bool is_static,
                                    const char* shorty,
                                    uint32_t shorty_len, 
                                    ShadowFrame* sf,
                                    size_t first_arg_reg) :
                //调用基类QuickArgumentVisitor的构造函数。
                QuickArgumentVisitor(sp, is_static, shorty, shorty_len),
                sf_(sf),                //sf_指向要处理的ShadowFrame对象
                cur_reg_(first_arg_reg) //遍历时用于记录当前位置的成员变量
                {
            ......
        }
        
        //调用下面这个函数即可实现参数遍历
        void Visit() SHARED_REQUIRES(Locks::mutator_lock_) OVERRIDE;
        ......
};



//在x86平台上，函数调用时的输入参数是通过栈来传递的。假设当前在函数A中，其内部将调用函数B。
//那么，如何将栈上B的参数存储到它的ShadowFrame对象中呢？来看 QuickArgumentVisitor 构造函数。
//[quick_trampoline_entrypoints.cc->QuickArgumentVisitor]
QuickArgumentVisitor(ArtMethod** sp, 
                     bool is_static, 
                     const char* shorty,
                     uint32_t shorty_len) :
    /*注意QuickArgumentVisitor 构造函数的第一个参数，它的类型是ArtMethod**。读者还记得本章
      “KSaveAll、kRefs和 kRefsAndArgs ”一节的内容吗？其中的图10-2、图10-3和图10-4中
      栈单元的顶部空间存储的都数据的类型就是 ArtMethod**。 
    */
    is_static_(is_static), shorty_(shorty), shorty_len_(shorty_len),
    //gpr_args_ 指向栈中存储通用寄存器位置的地方
    gpr_args_(reinterpret_cast<uint8_t*>(sp) + kQuickCalleeSaveFrame_RefAndArgs_Gpr1Offset),
    //fpr_args_ 指向栈中存储浮点寄存器位置的地方
    fpr_args_(reinterpret_cast<uint8_t*>(sp) + kQuickCalleeSaveFrame_RefAndArgs_Fpr1Offset),
    //stack_args_ 指向栈中存储输入参数的地方
    stack_args_(reinterpret_cast<uint8_t*>(sp) + kQuickCalleeSaveFrame_RefAndArgs_FrameSize + sizeof(ArtMethod*)),
    gpr_index_(0), fpr_index_(0), fpr_double_index_(0), stack_index_(0),
    cur_type_(Primitive::kPrimVoid), is_split_long_or_double_(false) {
        
    .......
    
}