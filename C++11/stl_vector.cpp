//vector是模板类，使用它之前需要包含<vector>头文件。
//示例代码5-41展示了vector的一些常见用法。示例代码5-41　vector的使用
void vector_test(){
    /*（1）创建一个以int整型为模板参数的数组，其初始元素为1,2,3,4,5,6
      （2）vector<int>是一个实例化的类，类名就是vector<int>。这个类名写起来不方便，
      所以可通过using为其取个别名IntVector
    */
    vector<int> intvector = {1,2,3,4,5,6};
    using IntVector = vector<int>;

    //（1）大部分容器类都有size和empty函数
    //（2）vector可通过[]访问
    //（3）push_back往数组尾部添加新元素
    IntVector::size_type size = intvector.size();
    bool isempty = intvector.empty();
    intvector[2] = 0;
    intvector.push_back(8);

    /*
     （1）每一个容器类都定义了各自的迭代器类型，所以需要通过容器类名::iterator来访问它们
     （2）begin返回元素的第一个位置，end返回结尾元素+1的位置
     （3）对iterator使用*取值符号可得到对应位置的元素
    */
    
    int i = 0;
    IntVector::iterator it;
    for(it = intvector.begin(); it != intvector.end();++it){
        int value = *it;
        cout<<"vector["<<i++<<"]="<< value<<endl;
    }
    
    /*
     （1）容器类名::iterator的写法太麻烦，所以可以用auto关键词来定义iterator变量。
     编译器会自动推导出正确的数据类型
     （2）rbegin和rend函数用于逆序遍历容器
    */
    i = intvector.size() - 1;
    for(auto newIt = intvector.rbegin();newIt != intvector.rend();++newIt){
         cout<<"vector["<<i--<<"]="<< *newIt<<endl;
    }
    //clear函数清空数组的内容
    intvector.clear();
}