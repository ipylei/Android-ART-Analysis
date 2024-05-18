#include "template_test.h"




void test() {
    /* ①隐式实例化，编译器根据传入的函数实参推导出类型T为int。最终编译器将生成
       int add(const int&a, const int&b)函数
    */
    int x = add(10, 20);
    
    //②下面三行代码对应为显示实例化，使用者指定模板参数的类型
    int y = add123<int,int,int>(1,2);//T1,T2,T3均为int
    y = add123<short,short>(1,2);//T1,T2为short,T3为默认类型long
    //T1指定为int,T2通过函数的实参（第二个参数5）推导出类型为int，T3为默认类型long
    add123<int>(4,5);

    add123(0,0);//③隐式实例化，T1、T2为int,T3为默认类型long
}



int main(){
    test();
    return 0;
}