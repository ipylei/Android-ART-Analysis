void string_test(){
    //定义三个string对象，string支持+操作符
    string s1("this is s1");
    string s2="this is s2";
    string s3 = s1 + ", " + s2;
    cout<<"s3="<<s1<<endl;// string也支持<<操作符
    /*
      （1）size()：返回字符串所占的字节个数。注意，返回的不是字符串中字符的个数。
      对于多字节字符而言，一个字符可能占据不止一个字节。
      （2）empty()：判断字符串是否为空
    */
    string::size_type size = s3.size();
    bool isEmpty = s1.empty();
    /*
      （1）string可支持索引方式访问其中的单个字符，其实它就是重载了[]操作符
      （2）C++ 11支持for-each循环，访问S2中的每一个字符
      （3）clear()：清理string中的字符
  */
    char b = s2[3];
    for(auto item:s2){cout<<item<<endl;}
    s2.clear();
    
    
    /*
      （1）为s2重新赋值新的内容
      （2）find()：查找字符串中的指定内容，返回为找到的匹配字符的索引位置。
      如果没有找到的话，返回值为string类的静态变量npos
    */
    s2 = "a new s2";
    string::size_type pos = s2.find("new");
    if(pos != string:npos) cout <<"fine new"<<endl;
    
    //c_str()函数获取string中的字符串，其类型为const char*，可用于C库中的printf等需要字符串的地方
    const char* c_string = s3.c_str();
    cout<<c_string<<endl;
}