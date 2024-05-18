
//[sigaction结构体介绍]
struct sigaction {
    /*Linux可自定义信号处理方法，下面的sa_handler和sa_sigaction是这两种处理方法对应的函数指针。
    观察这两个函数指针的参数可知：
    sa_handler只有一个代表信号ID的参数，
    而sa_sigaction则多了两个参数（后文再介绍它们是什么）。

    注意，这两种方法只能选择其中一种作为自定义信号处理函数。
    这是由sa_flags来控制的，
    当sa_flags设置SA_SIGINFO标记位时，表示使用sa_sigaction作为自定义信号处理函数*/
    void( * sa_handler)(int);

    void( * sa_sigaction)(int, siginfo_t * , void * );

    /*sa_mask类型是sigset_t。如其名，sigset_t是一个信号容器，
    借助相关方法可以往这个容器中添加或删除某个信号。
    当我们执行信号自定义处理函数时，想临时阻塞某些信号，
    那么就可以将这些信号的ID添加到sa_mask中。
    等信号处理函数返回后，系统将解除这些信号的阻塞*/
    sigset_t sa_mask;

    /*sa_flags可包含多个标志位，如上面提到的SA_SIGINFO。另外还有一个比较常见的标志位是SA_ONSTACK。'
    信号处理说白了就是执行一个函数,而函数执行的时候是需要一段内存来做栈。
    进程可以事先分配一段内存（比如new出来一块内存），
    然后设置这段内存为信号处理函数的内存栈（通过sigaltstack系统调用），
    否则操作系统将使用默认的内存栈（即使用执行信数号处理函所在线程的栈空间）。
    如果使用自定义的内存栈，则需要设置SA_ONSTACK标志。*/
    int sa_flags;

    void( * sa_restorer)(void); //一般不用它
};

//如果想要为某个信号设置信号处理结构体的话，需要使用系统调用sigaction，其定义如下。
//[sigaction系统调用使用说明]
 # include < signal.h > //包含signal.h系统文件
/*sigaction有三个参数：
（1）第一个参数为目标信号。
（2）第二个参数为该目标信号的信号处理结构体（由act表示）。
（3）第三个参数用于返回该信号之前所设置的信号处理结构体（返回值存储在oldact中）。
 */
int sigaction(int signum, const struct sigaction * act, struct sigaction * oldact)

//sigchain.cc->SignalAction类]
class SignalAction { //代码行位置有所调整
    private:
    struct sigaction action_; //自定义的信号处理结构体
    bool claimed_; //表示外界是否设置了action_

    //该变量如果为true，表示将使用sigaction中的sa_handler作为信号处理函数
    bool uses_old_style_;

    //SpecialSignalHandlerFn其实就是sa_sigaction函数指针，外界可单独设置一个信号处理函数，
    //它会在其他信号处理函数之前被调用
    SpecialSignalHandlerFn special_handler_;

    public:
    SignalAction(): claimed_(false),
    uses_old_style_(false),
    special_handler_(nullptr) {}

    //claim用于保存旧的信号处理结构体。使用方法见下文介绍
    void Claim(const struct sigaction & action) {
        action_ = action;
        claimed_ = true;
    }

    void Unclaim(int signal) {
        claimed_ = false;
        sigaction(signal,  & action_, nullptr);
    }

    .....

    //设置信号处理结构体
    void SetAction(const struct sigaction & action, bool oldstyle) {
        action_ = action;
        uses_old_style_ = oldstyle;
    }

    //设置特殊的信号处理函数
    void SetSpecialHandler(SpecialSignalHandlerFn fn) {
        special_handler_ = fn;
    }
};

// _NSIG 表示系统中有多少个信号。所以，下面这行代码将为系统中所有信号都创建一个SignalAction对象
static SignalAction user_sigactions[_NSIG];

//SetSpecialSignalHandlerFn介绍
//使用第一种方法的话必须调用 SetSpecialSignalHandlerFn 函数，代码如下所示。
//[sigchain.cc->SetSpecialSignalHandlerFn]
extern "C" void SetSpecialSignalHandlerFn(int signal, SpecialSignalHandlerFn fn) {
    CheckSignalValid(signal); //检查signal是否在_NSIG之内

    //从数组中找到对应的SignalAction对象
    user_sigactions[signal].SetSpecialHandler(fn);

    //如果该SignalAction对象之前没有声明使用过（代码中由claim单词来描述），则进行if里的处理
    if (!user_sigactions[signal].IsClaimed()) {
        struct sigaction act,
        old_act;
        //设置一个信号处理函数。注意，这个函数和SignalAction无关，它是ART单独提供的一个信号处理函数总入口。
        act.sa_sigaction = sigchainlib_managed_handler_sigaction;
        //清空信号集合，即在上述信号处理函数执行时，不阻塞任何信号
        sigemptyset( & act.sa_mask);
        act.sa_flags = SA_SIGINFO | SA_ONSTACK; //设置标志位

        ...

        //为该信号设置新的信号处理结构体，并返回之前设置的信号处理结构体
        if (sigaction(signal,  & act,  & old_act) != -1) {
            user_sigactions[signal].Claim(old_act); //将旧的信号处理结构体保存起来
        }
    }
}

//sigchainlib_managed_handler_sigaction介绍
//sigchainlib_managed_handler_sigaction的代码如下所示。
//[sigchain.cc->sigchainlib_managed_handler_sigaction]
static void sigchainlib_managed_handler_sigaction(int sig, siginfo_t * info, void * context) {
    InvokeUserSignalHandler(sig, info, context); //调用InvokeUserSignalHandler
}

//直接来看InvokeUseSignalHandler的代码，先不讨论第二个和第三个参数的含义
extern "C" void InvokeUserSignalHandler(int sig, siginfo_t * info, void * context) {
    CheckSignalValid(sig);
    
    //该信号对应的SignalAction对象必须声明被占用，否则不会走到这个函数
    if (!user_sigactions[sig].IsClaimed()) {
        abort();
    }

    //如果该SignalAction对象设置过特殊处理函数，则执行它
    SpecialSignalHandlerFn managed = user_sigactions[sig].GetSpecialHandler();
    if (managed != nullptr) {
        sigset_t mask,
        old_mask;
        sigfillset( & mask); //sigfillset将为mask集合添加所有的信号
        /*sigprocmask：用于设置进程的信号队列。SIG_BLOCK表示要屏蔽哪些信号。
        它是参数mask和之前被屏蔽信号的合集。参数old_mask返回之前设置过的被屏蔽的信号集合*/
        sigprocmask(SIG_BLOCK,  & mask,  & old_mask);
        if (managed(sig, info, context)) { //执行自定义的信号处理函数
            //SIG_SETMASK标志：更新进程需要被屏蔽的信号队列为old_mask。下面这行代码表示
            //当执行完信号处理函数后，我们需要恢复信号队列的内容。
            sigprocmask(SIG_SETMASK,  & old_mask, nullptr);
            return;
        }
        //如果信号处理函数执行失败（managed函数调用返回0），也需要恢复信号队列的内容。
        //然后再执行后面的代码
        sigprocmask(SIG_SETMASK,  & old_mask, nullptr);
    }
    //如果没有设置过特殊的信号处理函数或者特殊信号处理函数返回0，则直接使用SignalAction
    //对象里设置的信号处理结构体。
    const struct sigaction & action = user_sigactions[sig].GetAction();
    /*注意，由于我们已经在信号处理函数里了（sigchainlib_managed_handler_sigaction），所以此处只能自己来调用信号结构体里的自定义信号处理函数，而不是由操作系统来调用。*/
    if (user_sigactions[sig].OldStyle()) {
        .....
    } //以旧方式调用信号处理函数
    else {
        //如果使用新方式调用信号处理函数
        if (action.sa_sigaction != nullptr) { //如果有设置过信号处理函数
            sigset_t old_mask;
            sigprocmask(SIG_BLOCK,  & action.sa_mask,  & old_mask);
            action.sa_sigaction(sig, info, context); //执行该信号处理函数
            sigprocmask(SIG_SETMASK,  & old_mask, nullptr);
        } else {
            //signal是sigaction函数的对应简化版本，它用于给sig信号设置一个信号处理函数。
            //SIG_DFL表示默认处理。下面的代码表示恢复sig信号的信号处理函数为默认处理
            signal(sig, SIG_DFL);
            //raise用于投递一个信号。sig信号将重新被投递，并且交由默认信号处理函数来处理
            raise(sig);
        }
    }
}
