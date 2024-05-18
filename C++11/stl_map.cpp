//·使用map的时候离不开pair。pair定义在头文件<utility>中。pair也是模板类，有两个模板参数T1和T2。

//示例代码5-42展示了map类的用法。示例代码5-42　map的使用
/*
 （1）创建一个map、key和value的类型都是string
 （2）可通过索引 "[key]" 方式访问或添加元素
*/
map<string,string> stringMap = { {"1","one"}, {"2","two"},{"3","three"}, };
stringMap["4"] = "four";

/*
 （1）pair包含first和second两个元素。用它做键值对的载体再合适不过了
 （2）insert：添加一个键值对元素
*/
pair<string,string> kv6 = {"6","six"};
cout<<"first="<<kv6.first <<" second="<<kv6.second<<endl;
stringMap.insert(kv6);
/*
 （1）利用iterator遍历map
 （2）iterator有两个变量，first和second，分别是键值对元素的key和元素的value
*/
auto iter = stringMap.begin();
for(iter;iter != stringMap.end();++iter){
   cout<<"key="<<iter->first <<" value="<<iter->second<<endl; }

/*
 （1）make_pair是一个辅助函数，用于构造一个pair对象。C++11之前用得非常多
 （2）C++11支持用花括号来隐式构造一个pair对象了，用法比make_pair更简单
*/
stringMap.insert(make_pair("7","seven"));
stringMap.insert({"8","eight"});

/*
 （1）使用using定义一个类型别名
 （2）find用于搜索指定key值的元素，返回的是一个迭代器。如果没找到，则迭代器的值等于end()的返回值
 （3）erase删除指定Key的元素
*/
using StringMap = map<string,string>;
StringMap::iterator foundIt = stringMap.find("6");
if(foundIt != stringMap.end()){ cout<<"find value with key="<<endl;}
stringMap.erase("4");






//示例代码5-43　为map指定Compare模板参数

// mycompare是一个函数
bool mycompare(int a,int b){ return a<b ; }

// using定义了一个类型别名，其中第一个和第二个模板参数是int
template<typename Compare>
using MyMap = map<int,int,Compare>;

//MyCompare是一个重载了函数操作符的类
class MyCompare{
public:
    bool operator() (int x,int y){ return x < y;  }
};

//测试代码
void map_test(){
    // f是一个lambda表达式，用于比较两个int变量的大小
    auto f = [](int a,int b) -> bool{ return a>b; };
    
    /*为map指定前面三个模板参数。第三个模板参数的类型由decltype关键词得到。
    decltype用于推导括号中表达式的类型，和auto一样，这是在编译期由编译器推导出来的。
    f是匿名函数对象，其类型应该是一个重载了函数调用操作符的类。
    然后创建一个map对象a，其【构造函数】中传入了Compare的实例对象f */
    map<int,int,decltype(f)> a(f);
    a[1] = 1;


    /*
      （1）std::function是模板类，它可以将函数封装成重载了函数操作符的类类型。
            使用时需要包含<functional>头文件。function的模板参数是函数的信息（返回值和参数类型）。
            这个模板信息最终会变成函数操作符的信息。
      （2） b对象构造时传入mycompare 函数
    */
    map< int,int,std::function<bool(int,int)> >  b(mycompare);
    b[1] = 1;


    MyMap<MyCompare> c;// MyCompare为上面定义的类，用它做Compare模板参数的值
    c[1] = 1;
}


















//示例代码5-44　allocator用法
class Item{//Item类，用于测试
public:
    Item(int x):mx{x}{ cout<<"in Item(x="<<mx<<")"<<endl; }
    ~Item(){ cout<<"in ~Item()"<<endl;  }
private:
    int mx = 0;
};

void allocator_test(){
    //创建一个allocator对象，模板参数为Item
    allocator<Item> itemAllocator;
    
    //调用allocate函数分配可容纳5个Item对象的内存。注意，allocate函数只是分配内存
    Item* pItems = itemAllocator.allocate(5);
    
    /*construct函数用于构造Item对象。该函数第一个参数为内存的位置，第二个参数将作为
      Item构造函数的参数，其实这就是在指定内存上构造对象*/
    for(int i = 0; i < 5; i++){
        itemAllocator.construct(pItems+i,i*i);
    }
    
    //destroy用于析构对象
    for(int i = 0; i < 5; i++){ 
        itemAllocator.destroy(pItems+i); 
    }
    
    //deallocate用于回收内存
    itemAllocator.deallocate(pItems,5);
}