#ifndef _TYPE_CLASS_H_
#define _TYPE_CLASS_H_
namespace type_class { //命名空间
    void test(); //笔者用来测试的函数
    class Base {
        public: //访问权限：和Java一样，分为public、private和protected三种
            //①构造函数，析构函数，赋值函数，非常重要
            Base();                   //默认构造函数
            Base(int a);              //普通构造函数
            Base(const Base & other); //拷贝构造函数
            Base & operator = (const Base & other); //赋值函数
            ~Base();                  //析构函数

            Base(Base && other);               //移动构造函数
            Base & operator = (Base && other); //移动赋值函数
            Base operator + (const Base & a1);
            
        protected:
            //②成员函数：可以在头文件里直接实现，比如getMemberB。也可以只声明不实现，比如deleteC
            int getMemberB() { //成员函数:在头文件里实现
                return memberB;
            }
            //成员函数:在头文件里声明,在源文件里实现
            int deleteC(int a, int b = 100, bool test = true);
        
        private: //下面是成员变量的声明
            int memberA;                 //成员变量
            int memberB;
            static const int size = 512; //静态成员变量
            int * pMemberC;
    };
}
