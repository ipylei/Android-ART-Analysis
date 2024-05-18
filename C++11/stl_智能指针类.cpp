/*shared_ptr：共享式指针管理类。内部有一个引用计数，
每当有新的shared_ptr对象指向同一个被管理的内存资源时，其引用计数会递增。
该内存资源直到引用计数变成0时才会被释放。

·unique_ptr：独占式指针管理类。被保护的内存资源只能赋给一个unique_ptr对象。
当unique_ptr对象销毁、重置时，该内存资源被释放。
一个unique_ptr源对象赋值给一个unique_ptr目标对象时，内存资源的管理从源对象转移到目标对象。
shared_ptr 和 unique_ptr 的思想其实都很简单，就是借助引用计数的概念来控制内存资源的生命周期。
相比shared_ptr的共享式指针管理，unique_ptr的引用计数最多只能为1罢了。
*/



//示例代码5-49　shared_ptr用法
class SPItem{//测试类
public:
    SPItem(int x):mx{x}{cout<<"in SPItem mx="<<mx<<endl;}
    ~SPItem(){ cout<<"in ~SPItem mx="<<mx<<endl; }
    int mx;
};


//测试代码
/*make_shared是很常用的辅助函数模板
 （1）new一个SPItem对象。make_shared的参数为SPItem构造函数的参数
 （2）并返回一个shared_ptr对象,shared_ptr重载了->和*操作符*/
shared_ptr<SPItem> item0 = make_shared<SPItem>(1);
cout<<"item0->mx="<<item0->mx<<endl;

/*（1）将item0赋值给一个新的shared_ptr对象，这将递增内部的引用计数
  （2）get：返回所保护的内存资源地址。两个对象返回的值必然相同*/
shared_ptr<SPItem> item1 = item0;
if(item0.get() == item1.get()){
    cout<<"item0 and item1 contains same pointer"<<endl;
}
//use_count返回引用计数的值，此时有两个shared_ptr指向同一块内存，所以输出为2
cout<<"use count = "<<item1.use_count()<<endl;


/*
 （1）reset函数：释放之前占用的对象并指向 新的对象释放将导致引用计数递减。如果引用计数变成0，则内存被释放
 （2）item1指向新的SPItem
 （3）use_count变成1   
 */
item1.reset(new SPItem(2));
cout<<"(*item1).mx="<<(*item1).mx<<endl;
cout<<"use count = "<<item1.use_count()<<endl;
/*
在示例代码5-49中：
·STL提供一个帮助函数make_shared来构造被保护的内存对象以及一个的shared_ptr对象。
·当item0赋值给item1时，引用计数（通过use_count函数返回）递增。
·reset函数可以递减原被保护对象的引用计数，并重新设置新的被保护对象。
关于shared_ptr更多的信息，请参考：http://en.cppreference.com/w/cpp/memory/shared_ptr。
*/





//示例代码5-50　unique_ptr用法
/* （1）直接new一个被保护的对象，然后传给unique_ptr的构造函数
   （2）release将获取被保护的对象，注意，从此以后unique_ptr的内存管理权也将丢失。
       所以，使用者需要自己delete内存对象  */
unique_ptr<SPItem> unique0(new SPItem(3));
SPItem*pUnique0 = unique0.release();//release后，unique0将抛弃被保护对象
delete pUnique0;

/* （1）unique_ptr重载了bool类型转换操作符，用于判断被保护的指针是否为nullptr
   （2）get函数可以返回被保护的指针
   （3）reset将重置被保护的内存资源，原内存资源将被释放。*/
unique_ptr<SPItem> unique1(new SPItem(4));
unique_ptr<SPItem> unique2 = std::move(unique1);
if(unique1 == false){
    cout<<"after move,the pointer is "<<unique1.get()<<endl;
}
unique2.reset();