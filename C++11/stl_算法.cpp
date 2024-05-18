/*
在STL中想要使用算法相关的API的话需要包含头文件<algorithm>，
如果要使用一些专门的数值处理函数的话则需额外包含<numeric>头文件。
*/

vector<int> aIntVector = {1,2,3,4,5,6};
/* fill:第一个参数为元素的起始位置，第二个为目标的终点，前开后闭。aIntVector所有元素将赋值为-1  */
fill(aIntVector.begin(),aIntVector.end(),-1);
//使用了rbegin，即逆序操作，后面三个元素将赋值为-2
fill_n(aIntVector.rbegin(),3,-2);

vector<int> bIntVector;//定义一个新的vector
bIntVector.reserve(3);

/*copy将把源容器指定范围的元素拷贝到目标容器中。
注意，程序员必须要保证目标容器有足够的空间能容纳待拷贝的元素。
关于copy中的back_inserter，见正文介绍 */
copy(aIntVector.begin(),aIntVector.end(),back_inserter(bIntVector));

//accumulate将累加指定范围的元素。同时指定了一个初值100。最终sum100的值是91
auto sum100 = accumulate(aIntVector.begin(),aIntVector.end(),100);

// accumulate指定了一个特殊的操作函数。在这个函数里我们累加两个元素的和，然后再加上1000。
//最终，sum1000的值是5991
auto sum1000 = accumulate(aIntVector.begin(), aIntVector.end(), 0, [](int a, int b){
    return a + b + 1000;});
    
    
    
    
//示例代码5-46　sort、binary_search算法函数示例
//sort用于对元素进行排序，less指定排序规则为从小到大排序
sort(bIntVector.begin(),bIntVector.end(), std::less<int>());

//binary_search：二分查找法
bool find4 = binary_search(bIntVector.begin(),bIntVector.end(),4);

/*remove_if：遍历范围内的元素，然后将它传给一个lambda表达式以判断是否需要删除某个元素。
注意remove_if的返回值。详情见正文解释*/
auto newEnd = remove_if(aIntVector.begin(),
    aIntVector.end(),[](int value)->bool{
        if(value == -1) return true;//如果值是-1，则返回true，表示要删除该元素
        return false; });
        
//replace将容器内值为-2的元素更新其值为-3
replace(aIntVector.begin(),aIntVector.end(),-2,-3);

/*打印移除-1元素和替换-2为-3的aIntVector的内容，
  （1）第一行打印为-3 -3 -3
  （2）第二行打印为-3 -3 -3  -3 -3 -3
  为什么会这样？
*/
for(auto it = aIntVector.begin();it!=newEnd; ++it){cout << *it << " ";}
cout<<endl;

for(auto item:aIntVector){ cout << item << " "; }
cout<<endl;    


//newEnd和end()之间是逻辑上被remove的元素，我们需要把它从容器里真正移除!
aIntVector.erase(newEnd,aIntVector.end());