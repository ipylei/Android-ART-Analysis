class Test{
public:
    //①定义一个参数为initializer_list的构造函数
    Test(initializer_list<int> a_list){
        //②遍历initializer_list，它也是一种容器
        for(auto item:a_list)
            cout<<”item=”<<item<<endl;
        }
    }
}

Test a = {1,2,3,4};//只有Test类定义了①，才能使用列表初始化构造对象

initializer_list<string> strlist = {”1”,”2”,”3”};

using ILIter = initializer_list<string>::iterator;
//③通过iterator遍历initializer_list
for(ILIter iter = strlist.begin();iter != strlist.end();++iter){
    cout<<”item = ” << *iter << endl;
}


//【】5.8.2　带作用域的enum

//在C++11之前的传统enum，C++11继续支持
enum Color{red,yellow,green};
//在C++11之后，enum有一个新的形式：enum class或者enum struct
enum class ColorWithScope{red,yellow,green}


//对传统enum而言：
int a_red = red; //传统enum定义的color仅仅是把一组整型值放在一起罢了
//对enum class而言，必须按下面的方式定义和使用枚举变量。

//注意，green是属于ColorWithScope范围内的
ColorWithScope a_green = ColorWithScope::green;//::是作用域符号
//还可以定义另外一个NewColor，这里的green则是属于AnotherColorWithScope范围内
enum class AnotherColorWithScope{green,red,yellow};

//同样的做法对传统enum就不行，比如下面的enum定义将导致编译错误，
//因为green等已经在enum Color中定义过了
enum AnotherColor{green,red,yellow};


//【】5.8.3　constexpr
const int x = 0;//定义一个整型常量x，值为0
constexpr int y = 1; //定义一个整型常量y，值为1

//测试函数
int expr(int x){
    if(x == 1) return 0;
    if(x == 2) return 1;
    return -1;
}
const int x = expr(9);
x = 8;                    //编译错误，不能对只读变量进行修改
constexpr int y = expr(1);//编译错误，因为expr函数不是常量表达式


//【】5.8.4　static_assert
static_assert ( bool_constexpr , message )