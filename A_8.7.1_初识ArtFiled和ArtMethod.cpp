//【8.7.1.1】
//[art_field.h]
class ArtField { //此处只列举和本章内容相关的成员信息
    //.....
	
	private:
		GcRoot <mirror::Class> declaring_class_; //该成员变量在哪个类中被定义
		uint32_t access_flags_; //该成员变量的访问标记

		//该成员变量在dex文件中field_ids数组中的索引，
		//注意，它是由图8-7中encoded_field结构体中 field_idx_diff 计算而来
		uint32_t field_dex_idx_;

		//offset_和 Class 如何管理它的成员变量有关，详情见下文对 Class 类中成员变量的介绍。
        //【8.7.4.2】
        //如果ArtField所代表的成员变量是类的静态成员变量，则下面的 offset_ 代表是该变量实际的存储空间在图8-13里Class内存布局中的起始位置。
        //如果是非静态成员变量(即实例属性)，则 offset_ 指向图8-13中Object内存布局里对应的位置。
		uint32_t offset_;
};

//[art_method.h]
class ArtMethod { //此处只列举和本章内容相关的成员信息
    
	......
	
    protected:
	
		//下面这四个成员变量的解释可参考图8-7
		GcRoot <mirror::Class> declaring_class_; //本函数在哪个类中声明
		uint32_t access_flags_;
		uint32_t dex_code_item_offset_;
		uint32_t dex_method_index_;

		//与ArtField的 field_index_ 类似，下面这个成员变量和 Class 类如何管理它的成员函数有关。
		//详情见下文对Class的相关介绍。
		/*
		【解释】
		如果这个ArtMethod 对应的是一个 static 或 direct 函数，则 method_index_ 是指向定义它的类的 methods_ 中的索引。
        如果这个ArtMethod 是 virtual函数，则 method_index_ 是指向它的VTable中的索引。
        注意，可能多个类的VTable都包含该ArtMethod对象（比如Object的那11个方法），
        所以要保证这个 method_index_ 在不同VTable中都有相同的值——这也是LinkMethods中那三个函数比较复杂的原因。
		*/
		uint16_t method_index_;

		//热度。函数每被调用一次，该值递增1。一旦超过某个阈值，该函数可能就需要被编译成本地方法以加快执行速度了。
		uint16_t hotness_count_;

		//成员 ptr_sized_fields_ 是一个结构体
		struct PACKED(4)PtrSizedFields {
			//指向 declaring_class_-> dex_cache_ 的 resolved_methods_ 成员，详情需结合下文对 DexCache 的介绍。
			ArtMethod ** dex_cache_resolved_methods_;

			//指针的指针，指向 declaring_class_-> dex_cache_ 的 resolved_types_ 成员，
			//详情需结合下文对DexCache的介绍
			GcRoot <mirror::Class>  * dex_cache_resolved_types_;

			//下面两个变量是函数指针，它们是一个 ArtMethod 对象代表的Java方法的入口函数地址。
			//我们后续章节介绍Java代码执行的时候再来讨论它
			
            //jni机器码入口地址
            void * entry_point_from_jni_;                  //用于jni方法，指向jni方法对应的机器码入口地址
            //机器码入口地址
            void * entry_point_from_quick_compiled_code_;  //用于非jni方法，指向对应的机器码入口
            /*
            请读者注意：
                ·对jni方法而言，它的机器码入口地址和jni机器码入口地址都会被设置。我们后续介绍jni时再详细介绍这两个入口地址的作用。
                ·对非jni方法而言，它的jni机器码入口地址将有其他用途
            */
            
            //(9.6.2.3 oat文件和art文件的关系) art文件里的ArtMethod > ptr_sized_fields_  > entry_point_from_quick_compiled_code_ 指向位于oat文件里对应的code_数组

		} ptr_sized_fields_;
		
}


/*
jni_entrypoints_x86.S
quick_entrypoints_x86.S

http://aospxref.com/android-7.0.0_r7/xref/art/runtime/arch/arm64/quick_entrypoints_arm64.S
*/




//【8.7.1.2】
//[dex_cache.h->DexCache]
class DexCache: public Object { //此处只列举和本章内容相关的成员信息

    ......

    private:
		HeapReference <Object> dex_;

		//dex文件对应的路径
		HeapReference <String> location_;

		//实际为 DexFile*，指向所关联的那个Dex文件。
		uint64_t dex_file_;

		/*实际为ArtField**，指向ArtField*数组，成员的数据类型为ArtField*。
		该数组存储了一个Dex文件中定义的所有类的成员变量。
		另外，只有那些经解析后得到的ArtField对象才会存到这个数组里。
		该字段和Dex文件里的 field_ids 数组有关。
		 */
		uint64_t resolved_fields_;

		/*实际为 ArtMethod**，指向ArtMethod*数组，成员的数据类型为ArtMethod*。
		该数组存储了一个Dex文件中定义的所有类的成员函数。
		另外，只有那些经解析后得到的ArtMethod对象才会存到这个数组里。
		该字段和Dex文件里的 method_ids 数组有关。
		 */
		uint64_t resolved_methods_;

		/*实际为 GCRoot<Class>*，指向GcRoot<Class>数组，成员的数据类型为GcRoot<Class>（本质质上就是mirror::Class*）。
		它存储该dex文件里使用的数据类型信息数组。
		该字段和Dex文件里的 type_ids 数组有关。  */
		uint64_t resolved_types_;

		/*实际为 GCRoot <String>*，指向GcRoot<String>数组，包括该dex文件里使用的字符串信息数组。
		注意，GcRoot<String>本质上就是mirror::String*。该字段和Dex文件的string_ids数组有关
		 */
		uint64_t strings_;

		//下面四个变量分别表示上述四个数组的长度
		uint32_t num_resolved_fields_;
		uint32_t num_resolved_methods_;
		uint32_t num_resolved_types_;
		uint32_t num_strings_;
};





//【8.7.1.3】
//[class.h]
class Class: public Object {
    
    ......//先略过和本节内容无关的信息
    
    public:
    /*下面这个枚举变量用于描述类的状态。上文曾介绍过，一个类从dex文件里被加载到最终能被使
    用将经历很多个操作步骤。这些操作并不是连续执行的，而是可能被分散在不同的地方以不同的时
    机来执行不同的操作。所以，需要过类的状态来描述某个类当前处于什么阶段，这样便可知道下一
    步需要做什么工作。Class对象创建之初，其状态为 kStatusNotReady，最终可正常使用的状
    态为 kStatusInitialized。下文分析类加载的相关代码时，读者可看到状态是如何转变的。 */
    enum Status {
        kStatusRetired = -2,
        kStatusError = -1,
        kStatusNotReady = 0,
        kStatusIdx = 1,
        kStatusLoaded = 2,
        kStatusResolving = 3,
        kStatusResolved = 4,
        kStatusVerifying = 5,
        kStatusRetryVerificationAtRuntime = 6,
        kStatusVerifyingAtRuntime = 7,
        kStatusVerified = 8,
        kStatusInitializing = 9,
        kStatusInitialized = 10,
        kStatusMax = 11,
    };
    
    //加载本类的ClassLoader对象，如果为空，则为bootstrap system loader
    HeapReference <ClassLoader> class_loader_;

    //下面这个成员变量对数组类才有意义，用于表示数组元素的类型。比如，对String[][][]类而言，
    //component_type_代表String[][]。本章后文介绍数组类的时候还会讨论它。
    HeapReference <Class> component_type_;

    //该类缓存在哪个DexCahce对象中。注意，有些类是由虚拟机直接创建的，而不是从Dex文件里读取的。
	//比如基础数据类型。这种情况下dex_cache_取值为空。
    HeapReference <DexCache> dex_cache_;
    
	
    /*结合图8-6可知，IfTable 从 ObjectArray<Object>派生，所以它实际上是一个数组容器。
    为什么不直接使用它的父类 ObjectArray<Object>呢？根据ART虚拟机的设计，
	IfTable中的一个索引位置其实包含两项内容，
        第一项是该类所实现的接口类的Class对象
        第二项则是和第一项接口类有关的接口函数信息。笔者先用伪代码来描述IfTable中索引x对应的内容：
    
        第一项内容：具体位置为iftable_内部数组[x+0]，元素类型为 Class*，代表某个接口类
        第二项内容：具体位置为iftable_内部数组[x+1]，元素类型为 PointArray* 。如图8-6可知，

    PointArray 也是一个数组。其具体内容我们下文再详述。
    
    另外，对类A而言，它的iftable_所包含的信息来自于如下三个方面：
    （1）类A自己所实现的接口类。
    （2）类A从父类（direct superclass）那里获取的信息。
    （3）类A从接口父类（direct super interface）那里获取的信息。
    笔者先不介绍上面所谓的信息具体是什么，下文将对IfTable的元素构成做详细代码分析。 
	
	【解释】 保存了该类所直接实现或间接实现的接口信息
	*/
    HeapReference <IfTable> iftable_;
    
    //本类的类名
    HeapReference <String> name_;

    //代表父类。如果本类代表Object或基础数据类型，则该成员变量为空
    HeapReference <Class> super_class_;
    
    /*virtual methods table。它指向一个 PointArray 数组，元素的类型为ArtMethod*。
    这个vtable_的内容很丰富，下面的章节会详细介绍它。  
	
	【解释】和 iftable_ 类似，它保存了该类所有直接定义或间接定义的virtual方法信息
	*/
    HeapReference <PointerArray> vtable_;
    
    //类的访问标志。该字段的低16位可虚拟机自行定义使用
    uint32_t access_flags_;
	
    uint64_t dex_cache_strings_;
    
    //指向DexCache的 strings_ 成员变量实际为 LengthPrefixedArray<ArtField>，
    //代表本类声明的非静态成员变量。注意，这个 LengthPrefixedArray 的元素类型是ArtField，不是ArtField*。
    uint64_t ifields_;
    
    /*下面这三个变量需配合使用。其中，methods_实际为 LengthPrefixedArray<ArtMethod>，代表该类自己定义的成员函数。
	它包括类里定义的 virtual 和 direct 的成员函数，也包括从接口类中继承得到的默认函数 
	以及所谓的 miranda 函数（下文将介绍接口类的默认实现函数以及miranda函数）。methods_中元素排列如下：
    （1）[0,virtual_methods_offset_)                     为本类包含的 direct 成员函数
    （2）[virtual_methods_offset_,copied_methods_offset_)为本类包含的 virtual 成员函数
    （3）[copied_methods_offset_,...)                    为剩下的诸如 miranda 函数等内容   
	
	【解释】 methods_只包含本类直接定义的 direct 方法、
										  virtual 方法、
										  那些拷贝过来的诸如Miranda这样的方法（下文将介绍它）
	*/
    uint64_t methods_;
    uint16_t copied_methods_offset_;
    uint16_t virtual_methods_offset_;

    uint64_t sfields_;     //同 ifields_ 类似，只不过保存的是本类的静态成员变量
	
	
	
    uint32_t class_flags_;      //虚拟机内部使用
    uint32_t class_size_;       //当分配一个类对象时，用于说明这个类对象所需的内存大小
    pid_t clinit_thread_id_;    //代表执行该类初始化函数的线程ID
    int32_t dex_class_def_idx_; //本类在dex文件中 class_defs 数组对应元素的索引
    int32_t dex_type_idx_;      //本类在dex文件里type_ids中的索引
    
    //下面两个成员变量表示本类定义的引用类型的非静态和静态成员变量的个数
    uint32_t num_reference_instance_fields_;
    uint32_t num_reference_static_fields_;
    
    //该类的实例所占据的内存大小。也就是我们在Java层new一个该类的实例时，这个实例所需的内存大小
    uint32_t object_size_;
    
    /*下面这个变量的低16位存储的是Primitive::Type枚举值，其定义如下：
    enum Type { 
			kPrimNot = 0, kPrimBoolean, kPrimByte, kPrimChar, kPrimShort,
			kPrimInt, kPrimLong, kPrimFloat, kPrimDouble, kPrimVoid, kPrimLast = kPrimVoid  
        }; 
    其中，kPrimNot 表示非基础数据类型，即它代表引用类型。
    primitive_type_的高16位另有作用，后文碰到再述  */
    uint32_t primitive_type_;
    
    //下面这个变量指向一个内存地址，该地址中存储的是一个位图，
    //它和Class中用于表示引用类型的非静态成员变量的信息（ifields_）有关。
    //【8.7.4.3】
    //这个成员变量提供了一种快速访问类非静态、引用类型变量的方法
    //如何从reference_instance_offsets_的位置推导出ArtField的offset_呢?
    uint32_t reference_instance_offsets_;
    
    Status status_; //类的状态
    
    /*
	特别注意。虽然下面三个成员变量定义在注释语句中，但实际的Class对象内存空间可能包含
    对应的内容，笔者称之为Class的隐含成员变量。它们的取值情况我们下文会详细介绍
	*/
   
   /*Embedded Imtable（内嵌Interface Method Table）,是一个固定大小的数组。
    数组元素的类型为 ImTableEntry，
	但代码中并不存在这样的数据类型。
    实际上，下面这个隐含成员变量的声明可用 ArtMethod* embedded_imtable_[0]来表示  
	【解释】
	embedded_imtable_ 作用很明确，它只存储这个类所有 virtual 方法里那些属于接口的方法。
    显然，embedded_imtable_ 提供了类似快查表那样的功能。当调用某个接口方法时，可以先到embedded_imtable_里搜索该方法是否存在。
    由于embedded_imtable_ 只能存64个元素，编译时可扩大它的容量，但这会增加Class类所占据的内存。
	*/
    //ImTableEntry embedded_imtable_[0];
    
	
	
	
	
    /*Embedded Vtable（内嵌Virtual Table），是一个非固定大小的数组。
	数组元素为 VTableEntry，
    但代码中也不存在这样的数据类型。
	和上面的embedded_imtable_类似，它的声明也可用ArtMethod* embedded_vtable_[0]来表示   
    【解释】
	特别注意，ConcreteClass的vtable_将为nullptr。所有virtual的Java方法转移到隐含变量embedded_vtable_中了。
	回顾上文对Class ShouldHaveEmbeddedImtAndVTable代码的介绍可知，如果一个类是可以实例化的，
	则它的 embedded_imtable_ 和 embedded_vtable_ 隐含成员变量将存在。
	
	·实际上，不管是 embedded_vtable_ 还是 vtable_ ，
	二者保存的内容（即这个类所有的virtual方法）都是一样的。
	这里的“所有”包括该类自己定义的virtual方法，也包括来自父类、祖父类等通过继承或实现接口而得到的所有 virtual 方法。
	*/
    //VTableEntry embedded_vtable_[0];
    
	
	
	
	
    //下面这个数组存储Class中的静态成员变量的信息
    //uint32_t fields_[0];
    
    
    //再次请读者注意，以上三个隐含成员变量的内容将在下文介绍。
    
    
    //指向代表java/lang/Class的类对象。注意，它是static类型，它不是隐含成员变量
    static GcRoot <Class> java_lang_Class_;
    
};




//【其他】
//[Java 1.8 interface接口类默认方法]
public interface DefaultTestInf{        // DefaultTestInf是一个接口
    public void function_a();   //这是一个接口函数，没有函数实现，需要实现类来处理
    //从Java1.8开始，function_b()可提供默认实现。函数前必须有default关键词来标示
    default public void function_b(){
        ..... //完成相关处理
        return;
    }
    //除了默认实现外，接口类也可以定义静态成员函数。
    static public void function_c(){return; }
}



//8.7.1.3.2　Miranda Methods
//[Miranda方法示例]
//MirandaInterface 是一个接口类，包含一个接口函数inInterface
public interface MirandaInterface {
    public boolean inInterface();
}

//MirandaAbstract 是抽象类，虽然它实现了MirandaInterface接口，但是没有实现inInterface函数
public abstract class MirandaAbstract implements MirandaInterface{
    //注意，MirandaAbstract没有提供 inInterface 的实现
    public boolean inAbstract() { return true; }
}

//MirandaClass 继承了MirandaAbstract，并提供了inInterface的实现
public class MirandaClass extends MirandaAbstract {
    public MirandaClass() {}
    public boolean inInterface() { return true; }
}

//假设我们执行下面这样的代码。
MirandaClass mir = new MirandaClass();     //创建mir对象，类型为MirandaClass
//调用mir的inInterface函数，由于MirandaClass实现了它，所以调用正确，返回true //mir.inInterface();
//mira 是mir的基类对象
MirandaAbstract mira = mir;
mira.inInterface();         //注意这个函数调用




//[Object.java->Object::clone]
protected Object clone() throws CloneNotSupportedException {
    //Object clone函数先检查自己是否实现了Cloneable接口类。如果没有，则抛异常。
	if (!(this instanceof Cloneable)) {
		throw new CloneNotSupportedException("Class " + getClass().getName() + " doesn't implement Cloneable");
	}
	return internalClone();
}