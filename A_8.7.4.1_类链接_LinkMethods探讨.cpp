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