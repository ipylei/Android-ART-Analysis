//aosp2.2.3

//Main_MediaServer.cpp
//http://androidxref.com/2.2.3/xref/frameworks/base/media/mediaserver/main_mediaserver.cpp
//p132
int main(int argc, char** argv)
{
    //【6.2.2】
    //【1.】获得一个ProcessState实例
    sp<ProcessState> proc(ProcessState::self());
    
    //【6.2.3】
    //【2.】MediaServer(MS) 作为 ServiceManager 的客户端，要向 ServiceManager 注册服务
    //调用 defaultServiceManager，得到一个 IServiceManager
    sp<IServiceManager> sm = defaultServiceManager();
    LOGI("ServiceManager: %p", sm.get());
    
    //初始化音频系统的AudioFlinger服务
    AudioFlinger::instantiate();
    
    //【6.2.4】
    //【3.】多媒体系统的 MediaPlayer 服务，我们将以它作为主切入点
    MediaPlayerService::instantiate();
    
    //CameraService 服务
    CameraService::instantiate();
    //音频系统的AudioPolicy服务
    AudioPolicyService::instantiate();
    
    //【6.2.5】
    //【4.】 创建新线程，然后在新线程中：IPCThreadState::self()->joinThreadPool();
    //每个线程都有一个IPCThreadState
    //里面新启动线程去读取binder设备，查看是否有请求
    //p152。如果实现的服务负担不是很重，完全可以不用调用 startThreadPool 创建新的线程，使用主线程即可胜任。
    ProcessState::self()->startThreadPool();
    
    //【5.】主线程也调用joinThreadPool()读取binder设备，查看是否有请求
    IPCThreadState::self()->joinThreadPool();
}


//【6.2.2】独一无二的【单例】 ProcessState
//【1.】
//p134 http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/ProcessState.cpp
sp<ProcessState> ProcessState::self()
{
    if (gProcess != NULL) return gProcess;

    AutoMutex _l(gProcessMutex);
    if (gProcess == NULL) gProcess = new ProcessState;  //默认构造函数
    return gProcess;
}


//p134 http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/ProcessState.cpp
//构造函数
ProcessState::ProcessState(): mDriverFD(open_driver())  //打开 /dev/binder 这个设备，它是Android在内核中为完成进程间通信而专门设置的一个虚拟设备
                            , mVMStart(MAP_FAILED)      //映射内存的起始地址
                            , mManagesContexts(false)
                            , mBinderContextCheckFunc(NULL)
                            , mBinderContextUserData(NULL)
                            , mThreadPoolStarted(false)
                            , mThreadPoolSeq(1)
{
    if (mDriverFD >= 0) {
        //为Binder驱动分配一块内存来接收数据
        // mmap the binder, providing a chunk of virtual address space to receive transactions.
        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
    }
}


//p134 http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/ProcessState.cpp
static int open_driver()
{
    if (gSingleProcess) {
        return -1;
    }

    int fd = open("/dev/binder", O_RDWR);
    if (fd >= 0) {
        size_t maxThreads = 15;
        result = ioctl(fd, BINDER_SET_MAX_THREADS, &maxThreads);
    }
    return fd;
}




//【6.2.3】 时空穿越魔术  P135 --》返回一个 IServiceManager 实例【单例】
//【2.】【2.1】魔术前的准备工作 p135
// 通过这个对象，我们可以与另一个进程 【ServiceManager】 进行交互
//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IServiceManager.cpp
sp<IServiceManager> defaultServiceManager()
{
    if (gDefaultServiceManager != NULL) return gDefaultServiceManager;

    {
        AutoMutex _l(gDefaultServiceManagerLock);
        if (gDefaultServiceManager == NULL) {
            //（2.1）：getContextObject  
            //（2.3）：interface_cast
            gDefaultServiceManager = interface_cast<IServiceManager>(ProcessState::self()->getContextObject(NULL));
        }
    }

    return gDefaultServiceManager;
}

//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/ProcessState.cpp#96
sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& caller)
{
    if (supportsProcesses()) {
        return getStrongProxyForHandle(0);
    } else {
        return getContextObject(String16("default"), caller);
    }
}


//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/ProcessState.cpp#getStrongProxyForHandle
//p136
sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);
    
    //根据索引查找对应的资源，如果lookupHandleLocked发现没有对应的资源项，则会创建一个新的项并返回
    //这个新项的内容需要填充
    handle_entry* e = lookupHandleLocked(handle);

    if (e != NULL) {
        IBinder* b = e->binder;  //对于新项，它的binder为空
        if (b == NULL || !e->refs->attemptIncWeak(this)) {
            b = new BpBinder(handle);  //创建一个BpBinder
            e->binder = b;             //填充 entry 的内容
            if (b){e->refs = b->getWeakRefs();}
            result = b;
        } else {
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }

    return result;
}




//【2.2】 魔术表演的道具 --BpBinder  p136
//【Binder系统】  BpBinder：正向代理；BBinder：反向代理
//客户端 -> [BpBinder  ---->----  BBinder] -> Server
//客户端 -> [BpBinder  ---->----  BBinder] -> ServerManager
//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/BpBinder.cpp
//p137 构造函数
BpBinder::BpBinder(int32_t handle)
    : mHandle(handle)
    , mAlive(1)
    , mObitsSent(0)
    , mObituaries(NULL)
{
    LOGV("Creating BpBinder %p handle %d\n", this, mHandle);

    extendObjectLifetime(OBJECT_LIFETIME_WEAK);
    IPCThreadState::self()->incWeakHandle(handle);
}


//所以前面 gDefaultServiceManager = interface_cast<IServiceManager>(ProcessState::self()->getContextObject(NULL));
//等价于==》gDefaultServiceManager = interface_cast<IServiceManager>(new BpBinder(0));



//【2.3】 障眼法 -- interface_cast   p138
//http://androidxref.com/2.2.3/xref/frameworks/base/include/binder/IInterface.h#42
//p138
template<typename INTERFACE>
inline sp<INTERFACE> interface_cast(const sp<IBinder>& obj)
{
    return INTERFACE::asInterface(obj);
}
//===> 模板函数
//interface_cast<IServiceManager> 等价于
inline sp<IServiceManager> interface_cast(const sp<IBinder>& obj)
{
    return IServiceManager::asInterface(obj);
}



//【2.4】 拨开浮云见明月 -- IServiceManager  p138
//（1）定义业务逻辑  P138
//http://androidxref.com/2.2.3/xref/frameworks/base/include/binder/IServiceManager.h
class IServiceManager : public IInterface
{
public:
    //关键宏 http://androidxref.com/2.2.3/xref/frameworks/base/include/binder/IInterface.h#74
    DECLARE_META_INTERFACE(ServiceManager);
    
    //下面是 ServiceManager 所提供的业务函数，后面由  BpServiceManager  实现了！
    // http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IServiceManager.cpp
    virtual sp<IBinder>         getService( const String16& name) const = 0;
    virtual sp<IBinder>         checkService( const String16& name) const = 0;
    virtual status_t            addService( const String16& name, const sp<IBinder>& service) = 0;
    virtual Vector<String16>    listServices() = 0;

    enum {
        GET_SERVICE_TRANSACTION = IBinder::FIRST_CALL_TRANSACTION,
        CHECK_SERVICE_TRANSACTION,
        ADD_SERVICE_TRANSACTION,
        LIST_SERVICES_TRANSACTION,
    };
};



//（2）业务与通信挂钩  P139
/* http://androidxref.com/2.2.3/xref/frameworks/base/include/binder/IInterface.h#74
#define DECLARE_META_INTERFACE(INTERFACE)                               \
    static const String16 descriptor;                                   \
    static sp<I##INTERFACE> asInterface(const sp<IBinder>& obj);        \
    virtual const String16& getInterfaceDescriptor() const;             \
    I##INTERFACE();                                                     \
    virtual ~I##INTERFACE();  
*/

//使用宏：DECLARE_META_INTERFACE(ServiceManager);
// ------- 对宏进行相应的替换， 发现只是声明函数和变量， 下面那个宏才是定义它们
//定义一个描述字符串
static const String16 descriptor;     
//定义一个 asInterface 函数
static sp <IServiceManager> asInterface(const android::sp<android::IBinder> &obj) 
//定义一个 getInterfaceDescriptor 函数
virtual const String16& getInterfaceDescriptor() const; 
//定义 IServiceManager 的构造函数和析构函数
IServiceManager();
virtual ~IServiceManager();  

/* http://androidxref.com/2.2.3/xref/frameworks/base/include/binder/IInterface.h#74
#define IMPLEMENT_META_INTERFACE(INTERFACE, NAME)                       \
    const String16 I##INTERFACE::descriptor(NAME);                      \
    const String16& I##INTERFACE::getInterfaceDescriptor() const {      \
        return I##INTERFACE::descriptor;                                \
    }                                                                   \
    sp<I##INTERFACE> I##INTERFACE::asInterface(const sp<IBinder>& obj)  \
    {                                                                   \
        sp<I##INTERFACE> intr;                                          \
        if (obj != NULL) {                                              \
            intr = static_cast<I##INTERFACE*>(                          \
                obj->queryLocalInterface(                               \
                        I##INTERFACE::descriptor).get());               \
            if (intr == NULL) {                                         \
                intr = new Bp##INTERFACE(obj);                          \
            }                                                           \
        }                                                               \
        return intr;                                                    \
    }                                                                   \
    I##INTERFACE::I##INTERFACE() { }                                    \
    I##INTERFACE::~I##INTERFACE() { }                                   \
*/

//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IServiceManager.cpp
//使用宏：IMPLEMENT_META_INTERFACE(ServiceManager, "android.os.IServiceManager");
// ------- 对该宏进行相应的替换
const String16 IServiceManager::descriptor("android.os.IServiceManager"); 
const String16& IServiceManager::getInterfaceDescriptor() const {      
    return IServiceManager::descriptor;                                
}  

sp<IServiceManager> IServiceManager::asInterface(const sp<IBinder>& obj)  
{                                                                   
    sp<IServiceManager> intr;                                          
    if (obj != NULL) {                                              
        intr = static_cast<IServiceManager*>(obj->queryLocalInterface(IServiceManager::descriptor).get());               
        if (intr == NULL) {     
            //obj是前面刚才创建的那个 BpBinder(0)  参考【2.3】障眼法
            intr = new BpServiceManager(obj);    //构造函数！【所以这里就是将 BpBinder转为了 IServiceManager】                      
        }                                                           
    }                                                               
    return intr;                                                    
}  
IServiceManager::IServiceManager() { }                                    
IServiceManager::~I=IServiceManager() { }

//【声明 ！！！！！！！！！！！！！】
//【】
//【】
//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IServiceManager.cpp
//class IServiceManager : public IInterface{}
//class BpServiceManager : public BpInterface<IServiceManager>{}

//（3）IServiceManager 家族  P140
//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IServiceManager.cpp
class BpServiceManager : public BpInterface<IServiceManager>
{
public:
    BpServiceManager(const sp<IBinder>& impl)  //参数是 IBinder类型，实际上是 BpBinder对象
        : BpInterface<IServiceManager>(impl)    //调用基类 BpInterface 的构造函数
    {
        
        //(p142)实现了 IServiceManager 中的业务函数
        //line: 145
        virtual sp<IBinder>         getService( const String16& name) const = 0;
        virtual sp<IBinder>         checkService( const String16& name) const = 0;
        virtual status_t            addService( const String16& name, const sp<IBinder>& service) = 0;
        virtual Vector<String16>    listServices() = 0;
    }
}

//http://androidxref.com/2.2.3/xref/frameworks/base/include/binder/IInterface.h
template<typename INTERFACE>
inline BpInterface<INTERFACE>::BpInterface(const sp<IBinder>& remote)
    : BpRefBase(remote)  //基类构造函数
{
}

//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/Binder.cpp/
BpRefBase::BpRefBase(const sp<IBinder>& o)
    : mRemote(o.get()), mRefs(NULL), mState(0)
{
    extendObjectLifetime(OBJECT_LIFETIME_WEAK);

    if (mRemote) {
        mRemote->incStrong(this);           // Removed on first IncStrong().
        mRefs = mRemote->createWeak(this);  // Held for our entire lifetime.
    }
}