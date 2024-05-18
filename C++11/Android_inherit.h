class Base{....}//①内容与示例代码5-13一样


//②定义一个VirtualBase类
class VirtualBase {
public:
    //构造函数
    VirtualBase()  = default;
    //虚析构函数
    virtual ~VirtualBase() { cout << "in virtualBase:~VirtualBase" << endl; }
    //虚函数
    virtual void test1(bool test){cout<<"in virtualBase:testBase1"<<endl;}
    //纯虚函数
    virtual void test2(int x, int y) = 0;
    //普通函数
    void test3() {cout << "in virtualBase:test3" << endl; }
    int vbx;
    int vby;
};

//③从Base和VirtualBase派生属于多重继承。:号后边是派生列表，也就是基类列表
class Derived: private Base, public VirtualBase {
    public:
        //派生类构造函数
        Derived(int x, int y):Base(x),VirtualBase(),mY(y){};
        //派生类虚析构函数
        virtual ~Derived() {cout << "in Derived:~Derived" << endl; }
        
    public:
        //重写（override）虚函数test1
        void test1(bool test) override {cout << "in Derived::test1" << endl;}
        //实现纯虚函数test2
        void test2(int x, int y) override {cout << "in Derived::test2" << endl;}
        //重定义（redefine）test3
        void test3() { cout << "in Derived::test3" << endl; }
        
    private:
        int mY;
};