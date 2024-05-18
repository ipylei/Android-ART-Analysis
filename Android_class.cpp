//拷贝赋值函数，Base类重载赋值操作符=
Base& Base::operator =(const Base& other) {
    this->memberA = other.memberA;
    (*this).memberB = other.memberB;
    //下面if语句表明：既然要接受另外一个对象的内容，就先掏空自己和过去说再见
    if (pMemberC != nullptr) {
        delete[] pMemberC;
        pMemberC = nullptr;
    }

    if (other.pMemberC != nullptr) {//深拷贝other对象的pMemberC
        pMemberC = new int[Base::size];
        memcpy(pMemberC, other.pMemberC, size);
    }
    return *this; //把自己返回，赋值函数返回的是Base&类型
}



//移动构造函数，注意它们的参数中包含&&，是两个&符号
Base::Base(Base&& other):memberA(other.memberA), memberB(other.memberB),pMemberC(other.pMemberC) {
    cout << "in move copy constructor" << endl;
    other.pMemberC = nullptr;
}
//移动赋值函数。
Base& Base::operator =(Base&& other) {
    memberA = other.memberA;
    memberB = other.memberB;
    if (pMemberC != nullptr) {//清理this->pMemberC，因为它要得到新的内容
        delete[] pMemberC;
        pMemberC = nullptr;
    }
    pMemberC = other.pMemberC; //将other.pMemberC赋值给this的pMemberC
    other.pMemberC = nullptr; //将other.pMemberC置为nullptr
    cout << "in move assign constructor" << endl;
}
