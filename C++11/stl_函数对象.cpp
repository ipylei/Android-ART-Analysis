//示例代码5-47　bind函数示例
//fnotbind是一个lambda表达式，调用它时需要传入x、y、z三个参数
auto fnotbind = [](int x,int y, int z){
        cout<<"x="<<x<<" y="<<y<<" z="<<z<<endl;
        return x+y+z;
};


fnotbind(1,2,3);// fnotbind执行结果"x=1 y=2 z=3"
/*bind是一个作用尤为奇特的函数，它能为原可调用对象（本例是fnotbind lambda表达式）绑定一些参数，
从而得到一个新的可调用对象。这个新的可调用对象：
  （1）可以不用传入那么多参数。
  （2）新可调用对象的参数位置和原可调用对象的参数位置可以不同。 */
auto fbind_12 = bind(fnotbind,1,2,placeholders::_1);//第一个bind
fbind_12(3);// fbind_12执行结果"x=1 y=2 z=3"

auto fbind_321 = bind(fnotbind, placeholders::_3, placeholders::_2, placeholders::_1);//第二个bind
fbind_321(1,2,3);// fbind_321执行结果"x=3 y=2 z=1"


/*
bind的第一个参数是原可调用对象，它是函数指针、函数对象或lambda表达式，其后的参数就
是传递给原可调用对象的参数，其核心难点在于新可调用对象的参数与原可调用对象的参数的绑定规则：
（1）参数按顺序绑定。以第一个bind为例，1、2先出现。这相当于fnotbind的第一个和第二个参数将是1和2，
    第三个参数是placeholders::_1。_1是占位符，它是留给新可调用对象用的，代表新对象的输入参数
    
（2）占位符用_X表示，X是一个数字，其最大值由不同的C++实现给出。
    bind时，最大的那个X表示新可调用对象的参数的个数。
    比如第一个bind中只用了_1，它表示得到的fbind_12只有一个参数。
    第二个bind用到了_1,_2,_3，则表示新得到的fbind_321将会有三个参数。

（3）_X的位置和X的取值决定了新参数和原参数的绑定关系。
    以第二个bind为例，_3在bind原可调用对象的参数中排第一个，
    但是X取值为3。这表明新可调用对象的第一个参数将和原可调用对象的第三个参数绑定。 */
*/ 






//示例代码5-48　函数对象示例
/*multiplies是一个模板类，也是一个重载了函数调用操作符的类。
它的函数调用操作符用于计算输入的两个参数（类型由模板参数决定）的乘积。
通过bind，我们得到一个计算平方的新的函数调用对象squareBind */
auto squareBind = bind(multiplies<int>(),placeholders::_1,placeholders::_1);
int result = squareBind(10);
cout<<"result="<<result<<endl;

/*和multipies类似，less也是一个重载了函数调用操作符的模板类。
它的实例对象可以比较输入参数的大小。StringLessCompare是less<string>的别名，
它定义了一个对象 strLessCompare 对strLessCompare执行()
即可比较输入参数（类型为string，由模板参数实例化时决定）的大小 
*/
using StringLessCompare = less<string>;
StringLessCompare strLessCompare;
bool isless = strLessCompare("abcdef","abCDEf");//函数对象，执行它
cout<<"isless = " << isless<<endl;
/*
示例代码5-48展示了下列内容。
·mutiplies模板类：它是一个重载了函数操作符的模板类，用于计算两个输入参数的乘积。
输入参数的类型就是模板参数的类型。
·less 模板类：和mutiplies类似，它用于比较两个输入参数的大小。
最后，关于<algorithm>的全部内容，请读者参考：http://en.cppreference.com/w/cpp/header/functional。
*/