#ifndef _TYPE_CLASS_H_
#define _TYPE_CLASS_H_

namespace type_class
{                // �����ռ�
    void test(); // �����������Եĺ���
    class Base
    {
    public: // ����Ȩ�ޣ���Javaһ������Ϊpublic��private��protected����
        // �ٹ��캯����������������ֵ�������ǳ���Ҫ
        Base();                             // Ĭ�Ϲ��캯��
        Base(int a);                        // ��ͨ���캯��
        Base(const Base &other);            // �������캯��
        Base &operator=(const Base &other); // ��ֵ����
        Base(Base &&other);                 // �ƶ����캯��
        Base &operator=(Base &&other);      // �ƶ���ֵ����
        ~Base();                            // ��������
        Base operator+(const Base &a1);

    protected:
        // �ڳ�Ա������������ͷ�ļ���ֱ��ʵ�֣�����getMemberB��Ҳ����ֻ������ʵ�֣�����deleteC
        int getMemberB()
        { // ��Ա����:��ͷ�ļ���ʵ��
            return memberB;
        }
        // ��Ա����:��ͷ�ļ�������,��Դ�ļ���ʵ��
        int deleteC(int a, int b = 100, bool test = true);

    private:         // �����ǳ�Ա����������
        int memberA; // ��Ա����
        int memberB;
        static const int size = 512; // ��̬��Ա����
        int *pMemberC;
    };
}
#endif