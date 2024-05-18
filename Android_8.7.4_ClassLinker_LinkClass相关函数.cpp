//在介绍LinkClass之前，我们先来看后续代码中常用的几个函数。
//[class.h->Class::IsInstantiable]
//该函数（实际为模板函数，此处先去掉模板参数）用于判断某个类是否可实例化。
//对Java来说，就是可否创建该类的实例。显然，我们不能new一个基础数据类、接口类或抽象类（数组除外）的实例。
bool IsInstantiable(){
    return (!IsPrimitive() && !IsInterface() && !IsAbstract()) 
		|| (IsAbstract() && IsArrayClass<kVerifyFlags, kReadBarrierOption>());
}



//[class.h->Class::ShouldHaveEmbeddedImtAndVTable]
/*该函数（实际为模板函数，此处先去掉模板参数）用于判断某个类是否应该包含Class的隐含成员变量
  embedded_imtable_或 embedded_vtable_ 。
  
  从该函数的实现可知，如果一个类是可实例化的，则这两个隐含成员变量将存在。
  这里也再次强调一下为什么要将它们定义成Class的隐含成员变量。原因就是为了
  节省内存。Java代码中往往包含有大量抽象类、接口类。根据下面这个函数的点可知，这些类对应的Mirror
  Class是不需要这两个成员的。所以，代表抽象类或接口类的mirror Class对象就可以节省这部分空间所
  占据的内存  
*/
bool ShouldHaveEmbeddedImtAndVTable(){
    return IsInstantiable<kVerifyFlags, kReadBarrierOption>();
}



//[class.h->Class::IsTemp]
/*判断某个类是否为临时构造的类。根据该函数的实现可知，临时类的状态处于 kStatusResolving 之前，说
  明它还还在解析的过程中。另外，该类必须是可实例化的。*/
bool IsTemp(){
    Status s = GetStatus();
    return s < Status::kStatusResolving && ShouldHaveEmbeddedImtAndVTable();
}



//现在来看 LinkClass 的代码，如下所示。
//[class_linker.cc->ClassLinker::LinkClass]
bool ClassLinker::LinkClass(Thread* self,
							const char* descriptor,
							Handle<mirror::Class> klass, //待link的目标class
							Handle<mirror::ObjectArray<mirror::Class>> interfaces,
							MutableHandle<mirror::Class>* h_new_class_out) {
    /*注意本函数的参数：
    （1）klass代表输入的目标类，其状态是kStatusLoaded。
    （2）h_new_class_out为输出参数，代表LinkClass执行成功后所返回给调用者的、类状态切升级为
        kStatusResolved的目标类。所以，这个变量才是LinkClass执行成功后的结果。
    */
	
	 //Link基类，此函数比较简单，建议读者阅读完本节后再自行研究它。
    if (!LinkSuperClass(klass)) { 
			return false;  
	}
	
    /*下面将创建一个ArtMethod*数组。数组大小为 kImtSize 。它是一个编译时常量，由编译参数ART_IMT_SIZE指定，默认是64。
	IMT是Interface Method Table的缩写。如其名所示，它和接口所实现的函数有关。其作用我们后文再介绍  */
    ArtMethod* imt[mirror::Class::kImtSize];
	//填充这个数组的内容为默认值
    std::fill_n(imt, arraysize(imt), Runtime::Current()->GetImtUnimplementedMethod());
	
    //对该类所包含的方法（包括它实现的接口方法、继承自父类的方法等）进行处理
	//（更新目标类的iftable_、vtable_、相关隐含成员emedded_imf等信息）
    if (!LinkMethods(self, klass, interfaces, imt)) { 
		return false;
	}
	
    //下面两个函数分别对类的成员进行处理。
    if (!LinkInstanceFields(self, klass)) { 
		return false;
	}
    size_t class_size;
	//尤其注意 LinkStaticFields 函数，它的返回值包括 class_size，代表该类所需内存大小。
    if (!LinkStaticFields(self, klass, &class_size)) { 
		return false; 
	}

    //处理Class的 reference_instance_offsets_ 成员变量
	//设置目标类的reference_instance_offsets_
    CreateReferenceInstanceOffsets(klass);
	
    //当目标类是基础数据类、抽象类（不包括数组）、接口类时，下面的if条件满足
	//大致意思是如果目标类是不可实例化的
    if (!klass->IsTemp() ||  (!init_done_ && klass->GetClassSize() == class_size) ){
        .....
		
        //对于非Temp的类，不需要额外的操作，所以 klass 的状态被置为 kStatusResolved，然后再赋
        //值给 h_new_class_out。到此，目标类就算解析完了
        mirror::Class::SetStatus(klass, mirror::Class::kStatusResolved, self);
        h_new_class_out->Assign(klass.Get());
		
    }
	//如果目标类是可实例化的，则需要做额外的处理
	else {
        StackHandleScope<1> hs(self);
		
        //CopyOf很关键，它先创建一个大小为 class_size 的 Class 对象，然后，klass的信息将拷贝到这个新创建的Class对象中。
		//在这个处理过程汇总，Class对象的类状态将被设置为 kStatusResolving 。读者可自行研究此函数。
		auto h_new_class = hs.NewHandle(klass->CopyOf(self, class_size, imt, image_pointer_size_) );
		
		//清理klass的内容
        klass->SetMethodsPtrUnchecked(nullptr, 0, 0);      
		
        ...... //清理klass的其他内容;
		
        {
            ......
            //更新ClassTable中对应的信息
            mirror::Class* existing = table->UpdateClass(descriptor, 
													     h_new_class.Get(), 
														 ComputeModifiedUtf8Hash(descriptor));
            ......
        }
		
        //设置klass的状态为kStatusRetired，表示该类已经被废弃
        mirror::Class::SetStatus(klass, mirror::Class::kStatusRetired, self);
        //设置新类的状态为kStatusResolved，表示该类解析完毕。
        mirror::Class::SetStatus(h_new_class, mirror::Class::kStatusResolved,self);
        h_new_class_out->Assign(h_new_class.Get());      //赋值给输出参数
    }
    return true;
}






//8.7.4.1　LinkMethods探讨 
//LinkMethod函数本身的内容非常简单，代码如下所示。
//（更新目标类的iftable_、vtable_、相关隐含成员emedded_imf等信息）
//[class_linker.cc->ClassLinker::LinkMethods]
bool ClassLinker::LinkMethods(Thread* self,
            Handle<mirror::Class> klass,
            Handle<mirror::ObjectArray<mirror::Class>> interfaces,
            ArtMethod** out_imt) {
    ......
    std::unordered_map<size_t, ClassLinker::MethodTranslation> default_translations;
    //下面三个函数很复杂
    return  SetupInterfaceLookupTable(self, klass, interfaces)
            && LinkVirtualMethods(self, klass, &default_translations)
            && LinkInterfaceMethods(self, klass, default_translations, out_imt);
}



/*

	LinkMethods详解

*/

//[示例代码- If0.java]
public interface If0{ //注意，接口类的父类都是java.lang.Object
    public void doInterfaceWork0();
    public void doInterfaceWork1();
}

//[示例代码- If1.java]
//注意：If1使用extends来继承If0。但由于If1是interface，所以它实际上是implements了If0。Java
//语言中，只有接口类可以extends多个父接口类（Super Interface）
public static interface If1 extends If0{
    public void doInterfaceWork2();
    public void doInterfaceWork3();
}


//[示例代码-AbsClass0.java]
public static abstract class AbsClass0 implements If1{
    public void doInterfaceWork0(){ return; }
    public void doInterfaceWork3(){ return; }
	
	//+
    abstract public void doAbsWork0();
    public void doRealWork0(){return;}
}


//[示例代码-ConcreteClass.java]
public static class ConcreteClass extends AbsClass0{
    public void doInterfaceWork0(){ return; }
    public void doInterfaceWork1(){ return; }
    public void doInterfaceWork2(){ return; }
    public void doInterfaceWork3(){ return; }
    public void doAbsWork0(){ return; }
	
	//+
    public void doRealWork1(){ return; }
}


//[示例代码-ConcreteChildClass.java]
public static class ConcreteChildClass extends ConcreteClass{
	//+
	public void doRealWork2(){ return;  }
}





//如何确定某个接口方法在该表中的索引呢？来看代码。
//[class_linker.cc->ClassLinker::GetIMTIndex]
static inline uint32_t GetIMTIndex(ArtMethod* interface_method){
    /*GetDexMethodIndex 返回 ArtMethod dex_method_index_ 成员变量，
	代表该方法在dex中的索引号。然后对 kImtSize 求模，返回值即是 embedded_imtable_ 中的索引号。*/
    return interface_method->GetDexMethodIndex() % mirror::Class::kImtSize;
}


//[class-inl.h->Class::HasVTable/GetVTableLength/GetVTableEntry]
//判断有没有virtual Table，不区分是 vtable_ 还是 embedded_vtable_ 以后我们也统一用VTable来表示
inline bool Class::HasVTable() {
    return GetVTable() != nullptr || ShouldHaveEmbeddedImtAndVTable();
}

inline int32_t Class::GetVTableLength() {      //获取VTable的元素个数
    if (ShouldHaveEmbeddedImtAndVTable()) {
        return GetEmbeddedVTableLength();
    }
    //注意，GetVTable将返回vtable_成员变量
    return GetVTable() != nullptr ? GetVTable()->GetLength() : 0;
}

//获取VTable中的某个元素
inline ArtMethod* Class::GetVTableEntry(uint32_t i, size_t pointer_size) {
    if (ShouldHaveEmbeddedImtAndVTable()) {      //从 embedded_vtable_ 中取元素
        return GetEmbeddedVTableEntry(i, pointer_size);
    }
    auto* vtable = GetVTable();                  //从 vtable_ 中取元素
    return vtable->GetElementPtrSize<ArtMethod*>(i, pointer_size);
}