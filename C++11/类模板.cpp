//template_test.h
//定义类模板，模板参数有两个，一个是类型模板参数T，另外一个是非类型模板参数N
template<typename T, unsigned int N>
class TObj {
public:
    //类模板中的普通成员函数
    void accessObj(){
        cout<<"TObj<typename T, unsigned int N> N="<<N<<endl;
    }
    //类模板中的函数模板，一般称之为成员模板，其模板参数可以和类模板的参数不相同
    template<typename T1>
    bool compare(const T1& a,const T& b){return a < b; }
public:
    //模板参数中的非类型参数在编译期就需要决定，所以这里声明的实际上是固定大小的数组
    T obj[N];
};





//示例代码5-36　类模板的特例化
//template_test.h
//全特化，T和N指定，同时accessObj,compare内容不同。全特化得到一个实例化的TObj类
template<>
class TObj<int*,5>{
public:
    void accessObj(){ cout<<"Full specialization TObj<int*,5>"<<endl; }
    template<typename T1>
    bool compare(const T1& a,const int* & b){ return a<*b;}
public:
    int* obj[5];
};

//偏特化：偏特化并不确定所有的模板参数，所以偏特化得到的依然是一个类模板
//偏特化时候确定的模板参数不需要在template<模板参数>中列出来
template<unsigned int N>
class TObj<int*,N>{
public:
    void accessObj() {
        cout<<"Partial specialization TObj<int*,N> N="<<N<<endl; }
template<typename T1>
    bool compare(const T1& a,const int* & b){return a < (*b);}
public:
    int* obj[N];
};





//类模板的使用  p173
//template_test.cpp
/*通过 using 关键词，我们定义了一个模板类型别名TObj10，
这个类型别名指定了模板参数N为10。使用者以后只需为这个模板类型单独设置模板参数T即可。
使用类型别名的主要好处在于可少写一些代码，另外能增加代码的可读性*/
template<typename T>
using TObj10 = TObj<T,10>;
void testTemplateClass() {
    //和函数模板不同，类模板必须显示实例化，即明确指明模板参数的取值
    TObj<int, 3> intObj_3;
    intObj_3.accessObj();

    TObj10<long> longObj_10;//TObj10为利用using定义的模板类型别名
    longObj_10.accessObj();

    TObj<int*, 5> intpObj_5;//使用全特化的TObj
    intpObj_5.accessObj();

    TObj<int*, 100> intpObj_100;//使用偏特化的版本并实例化它
    
    int x = 100;
    const int*p = &x;
    /*实例化成员模板compare函数，得到如下两个函数：
      compare(int,const int*&)和compare(long,const int*&) */
    intpObj_5.compare(10, p);
    intpObj_5.compare<long>(1000, p);
}











//示例代码5-38　类外如何定义成员函数
//①注意语法格式
template<typename T,unsigned int N>
void TObj<T,N>::accessObj(){
    cout<<"TObj<T,N>::accessObj()"<<endl;
}
//②全特化得到具体的类，所以前面无需用template修饰
void TObj<int*,5>::accessObj(){
    cout<<"TObj<int*,5>::accessObj()"<<endl;
}
//③偏特化的版本。注意，成员函数内部使用TObj即可表示对应的类类型，不用写成TObj<int*,N>
template<unsigned N>
void TObj<int*,N>::accessObj(){
    TObj* p = this; //不用写成TObj<int*,N>
    p->obj[0] = nullptr;
    cout<<"TObj<int*,N>::accessObj()"<<endl;
}

//④第一个template是类模板的模板信息，第二个template是成员模板的模板信息
template<typename T,unsigned int N>
template<typename T1>
bool TObj<T, N>::compare(const T1& a, const T & b) {
    cout<<"TObj<T, N>::compare"<<endl;
    return false;
}

template<unsigned int N>
template<typename T1>
bool TObj<int*, N>::compare(const T1& a, const int* & b) {
    cout<<"TObj<int*, N>::compare"<<endl;
    return true;
}