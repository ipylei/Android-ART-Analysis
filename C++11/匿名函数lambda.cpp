//示例代码5-39　lambda表达式
/*创建lambda表达式后将得到一个闭包，闭包就是匿名的函数对象。
规范中并没有明确说明闭包的数据类型到底是什么，所以我们利用auto关键词来表示它的类型。
  auto关键词是C++11引入的，代表一种数据类型。这个数据类型由编译器根据=号右边的表达式推导出来。*/
auto f1 = [] {
     cout <<"this is f1,no return"<<endl;      };
auto f2 = [] {
     cout <<"this is f2, return int"<<endl;
     return 0;};
     
f1();//执行lambda表达式f1
f2();//执行lambda表达式f2

int x = 0;
string info = "hello world";
bool a = false;

//创建lambda表达式f3。该表达式执行的时候需要传入两个参数
auto f3 = [](int x, bool &a) -> bool {
     cout <<"this is f3"<<endl;
     return false;   };
f3(1, a);

/*[]为捕获列表，x、info和a这三个变量将传给f4，其实就是用这三个变量来构造一个匿名函数对象。
这个函数对象有三个成员变量，名字可能也叫x、info和a。
其中，x和a为值传递，info为引用传递。所以，f4执行的时候可以修改info的值 */
auto f4 = [x,&info,a]() {
     cout<<"x="<<x <<" info="<<info  <<" a="<<a<<endl;
     info = "hello world in f4"; };
//f4还未执行，所以info输出为”hello world”
cout << info << endl;

f4();//执行f4
//f4执行完后，info被修改，所以info输出为"hello world in f4"
cout << info << endl;



此处仅关注捕获列表中的内容
[=,&变量a,&变量b] = 号表示按值的方式捕获该lambda创建时所能看到的全部变量。
如果有些变量需要通过引用方式来捕获的话就把它们单独列出来（变量前带上&符号）

[&,变量a,变量b] &号表示按引用方式捕获该lambda创建时所能看到的全部变量。
如果有些变量需要通过按值方式来捕获的话就把它们单独列出来（变量前不用带上=号）