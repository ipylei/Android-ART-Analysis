#include <cstring>
#include "p_5_3_TypeClass.h"

using namespace type_class;
using namespace std;

// ��Ĭ�Ϲ��캯����ָ��Щû�в����������в�������Ĭ��ֵ�Ĺ��캯��
// : ��{��֮����ǹ��캯���ĳ�ʼֵ�б�
Base::Base() : memberA(0), memberB(100), pMemberC(new int[size])
{
    cout << "In Base constructor" << endl;
}
// ����ͨ���캯����Я��������Ҳʹ�ó�ʼ���б�����ʼ����Ա������ע��˴���ʼ���б��������Ա
// ��ʼ���õ���{}����
Base::Base(int a) : memberA{a}, memberB{100}, pMemberC{new int[size]}
{
    cout << "In Base constructor 2" << endl;
}
/*�ۿ������캯�����÷����£�
  Base y;   //�ȴ���y����
  Base x(y);//��y����ֱ�ӹ���x
  Base z = y;//y�������������Ķ���z
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

