//【10.2.4】　调用栈的管理和遍历
/*
在ART虚拟机中，从虚拟机执行层进入机器码执行层，或者从虚拟机执行层进入解释执行层的过程叫transition（过渡或转变）。
另外，请读者注意，当我们在方法E中进行栈回溯（比如打印调用栈的信息）时，
输出的应该是Java方法A、B、C、D的信息，而不能包含虚拟机执行层自身所调用的函数。


栈管理：
    ·PushManagedStackFramgnet 和 PopManagedStackFragment。这两个函数成对出现。
            当从虚拟机执行层进入机器码执行层或者解释执行层时，虚拟机会创建一个Managed-Stack对象，并调用Thread PushManagedStackFragment函数。
            当目标方法返回后，Thread PopManagedStackFragment会被调用。
            
    ·PushShadowFrame 和 PopShadowFrame。这两个函数专供解释执行层使用，代表某个被调用的Java方法所用的栈帧。

    ·机器码执行层和虚拟机执行层本身不需要单独的栈管理对象。
*/



//马上来看Thread类中相关的成员变量和函数。
//[thread.h->Thread]
/*每一个Thread对象有一个唯一的 tlsPtr_ 对象，其中包含一个 managed_stack 成员变量。如上文所述，
  Thread将按照栈的方式（先入后出）来管理ManagedStack对象。
  tlsPtr_.managed_stack代表栈顶的那个ManagedStack对象。  
*/
struct tls_ptr_sized_values {
    ManagedStack managed_stack;      //这是一个对象，不是指针
}tlsPtr_;

//ManagedStack的Push和Pop处理
void Thread::PushManagedStackFragment(ManagedStack* fragment) {
    //fragment入栈，详情见下文代码分析
    tlsPtr_.managed_stack.PushManagedStackFragment(fragment);
}
void Thread::PopManagedStackFragment(const ManagedStack& fragment) {
    tlsPtr_.managed_stack.PopManagedStackFragment(fragment);
}

//ShaodowFrame的Push和Pop处理
ShadowFrame* Thread::PushShadowFrame(ShadowFrame* new_top_frame) {
    return tlsPtr_.managed_stack.PushShadowFrame(new_top_frame);
}
ShadowFrame* Thread::PopShadowFrame() {
    return tlsPtr_.managed_stack.PopShadowFrame();
}



//[stack.h->ManagedStack]
void PushManagedStackFragment(ManagedStack* fragment) {
    //先拷贝this的内容到fragment中。此后，fragment将保存this的内容
    memcpy(fragment, this, sizeof(ManagedStack));
    //清空this。注意，this以前的内容通过上行代码已存储到 fragment 里了。
    memset(this, 0, sizeof(ManagedStack));
    //设置link_为新的fragment。
    link_ = fragment;
}

void PopManagedStackFragment(const ManagedStack& fragment) {
    //复制fragment的内容到this
    memcpy(this, &fragment, sizeof(ManagedStack));
}

//处理ShadowFrame
ShadowFrame* PushShadowFrame(ShadowFrame* new_top_frame) {
    ShadowFrame* old_frame = top_shadow_frame_;
    top_shadow_frame_ = new_top_frame;
    new_top_frame->SetLink(old_frame);
    return old_frame;
}

ShadowFrame* PopShadowFrame() {
    ShadowFrame* frame = top_shadow_frame_;
    top_shadow_frame_ = frame->GetLink();
    return frame;
}


//tlsPtr_.managed_stack.top_shadow_frame_