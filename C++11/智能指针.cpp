class SmartPointerOfObj {// SmartPointerOfObj是一个简单的智能指针类
public:
    SmartPointerOfObj(Obj* pObj) : mpObj(pObj) { };
    ~SmartPointerOfObj() {//析构函数，
        if (mpObj != nullptr) {
            delete mpObj;// 释放占用的内存资源
            mpObj = nullptr;
        }
    }
    Obj* operator ->() {//重载->操作符
        cout << "check obj in ->()" << endl;
        return mpObj;
    }
    Obj& operator *() {//重载*操作符
        cout << "check obj in *()" << endl;
        return *mpObj;
    }
private:
    Obj* mpObj;
};
//测试代码：
void testSmartPointer() {
    /*先new一个Obj对象，然后用该对象的地址作为参数构造一个SmartPointerOfObj对象，
    Obj对象的地址存储在mpObj成员变量中。*/
    SmartPointerOfObj spObj(new Obj());
    /*SmartPointerOfObj重载了->和*操作符。spObj本身不是一个指针类型的变量，但是却可以把它当作指针型变量，
    即利用->和*来获取它的mpObj变量。比如下面这两行代码*/
    spObj->getSomethingPublic();
    (*spObj).getSomethingPublic();
}


//5.4.3 (p159)