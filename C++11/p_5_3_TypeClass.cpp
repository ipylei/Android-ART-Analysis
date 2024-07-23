#include <cstring>
#include "p_5_3_TypeClass.h"

using namespace type_class;
using namespace std;

// ①默认构造函数，指那些没有参数或者所有参数都有默认值的构造函数
// : 和{号之间的是构造函数的初始值列表
Base::Base() : memberA(0), memberB(100), pMemberC(new int[size])
{
    cout << "In Base constructor" << endl;
}
// ②普通构造函数：携带参数。也使用初始化列表来初始化成员变量，注意此处初始化列表里各个成员
// 初始化用的是{}括号
Base::Base(int a) : memberA{a}, memberB{100}, pMemberC{new int[size]}
{
    cout << "In Base constructor 2" << endl;
}
/*③拷贝构造函数，用法如下：
  Base y;   //先创建y对象
  Base x(y);//用y对象直接构造x
  Base z = y;//y拷贝给正创建的对象z
*/
Base::Base(const Base &other) : memberA{other.memberA}, memberB{other.memberB}, pMemberC{nullptr}
{
    cout << "In copy constructor" << endl;
    if (other.pMemberC != nullptr)
    {
        pMemberC = new int[Base::size];
        memcpy(pMemberC, other.pMemberC, size);
    }
}

