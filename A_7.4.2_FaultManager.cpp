//[fault_handler.cc->FaultManager构造函数]
FaultManager::FaultManager(): initialized_(false) {
    //获取SIGSEGV信号之前设置的信号处理信息，存储在oldaction_对象中
    sigaction(SIGSEGV, nullptr,  & oldaction_); //获取SIGSEGV
}

//[fault_handler.cc->FaultManager::Init]
void FaultManager::Init() {
    //下面几行代码的含义如下：
    //SetUpArtAction：为action对象设置ART虚拟机指定的信号处理函数
    //然后调用sigaction将这个action对象设置为SIGSEGV信号的信号处理结构体
    struct sigaction action;
    SetUpArtAction( & action);
    int e = sigaction(SIGSEGV,  & action,  & oldaction_);

    //设置SIGSEGV对应的SignalAction对象，保存oldaction_信息，同时声明该信号被使用
    ClaimSignalChain(SIGSEGV,  & oldaction_);

    initialized_ = true;
}

//[fault_handler.cc->FaultManager::SetUpArtAction]
static void SetUpArtAction(struct sigaction * action) {
    action->sa_sigaction = art_fault_handler; //信号处理函数
    sigemptyset( & action->sa_mask); //不block任何信号
    action->sa_flags = SA_SIGINFO | SA_ONSTACK;
     # if !defined(__APPLE__) && !defined(__mips__)
        action->sa_restorer = nullptr;
     # endif
}




//[fault_handler.cc->FaultManager::AddHandler]
void FaultManager::AddHandler(FaultHandler* handler, bool generated_code) {
    /*结合上面的代码可知：
     （1）SuspensionHandler、StackOverflowHandler、NullPointerHandler都
     属于 generated_code 类型的FaultHandler，它们保存在FaultManager对象中的generated_code_handlers成员中（类型为vector<FaultHandler*>）
     （2）JavaStackTraceHandler属于非generated_code类型，所以保存到FaultManager中的 other_handlers_ 数组（类型为vector<FaultHandler*>）
*/
    if (generated_code) {
        generated_code_handlers_.push_back(handler); 
    }
    else { 
        other_handlers_.push_back(handler); 
    }
}