//template_test.h，函数模板一般定义在头文件中，即头文件中会包括函数模板的全部内容
//template是关键词，紧跟其后的是模板参数列表，<>中的是一个或多个模板参数
template<typename T>
T add(const T& a, const T& b) {
    cout << "sizeof(T)=" << sizeof(T) << endl;
    return a + b;
}

template<typename T1, typename T2, typename T3 = long>
T3 add123(T1 a1, T2 a2) {
    cout << "sizeof(T1,T2,T3)=(" << sizeof(T1) << "," << sizeof(T2) << ","
         << sizeof(T3) << ")" << endl;
    return (T3) (a1 + a2);
}


/*
template<>
long add123(int* a1, int*a2);




//在下面这段代码中，T是代表数据类型的模板参数，N是整型，compare则是函数指针,它们都是模板参数。
template<typename T,int N,bool (*compare)(const T & a1,const T &a2)>
void comparetest(const T& a1,const T& a2){
    cout<<"N="<<N<<endl;
    compare(a1,a2);//调用传入的compare函数
}
*/