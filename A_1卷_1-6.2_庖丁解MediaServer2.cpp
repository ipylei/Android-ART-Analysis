//【6.2.4】 注册 MediaPlayerService
//【1.】业务层的工作 p142
//http://androidxref.com/2.2.3/xref/frameworks/base/media/libmediaplayerservice/MediaPlayerService.cpp
void MediaPlayerService::instantiate() {
    defaultServiceManager()->addService(String16("media.player"), new MediaPlayerService());
}

// BpServiceManager实现的 addService() 函数
//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IServiceManager.cpp
virtual status_t addService(const String16& name, const sp<IBinder>& service)
{
    Parcel data, reply; //Parcel： 就把它当作是一个数据包
    data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());
    data.writeString16(name);
    data.writeStrongBinder(service);
    //remote()返回的是mRemote，也就是BpBinder对象
    status_t err = remote()->transact(ADD_SERVICE_TRANSACTION, data, &reply);
    return err == NO_ERROR ? reply.readInt32() : err;
}
/*在addService()函数中把请求数据打包成data后，传给了 BpBinder 的transact函数，
这就是把通信的工作交给了 BpBinder
*/



//【2】 通信层的工作 //p143
//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/BpBinder.cpp
status_t BpBinder::transact(uint32_t code, 
                            const Parcel& data, 
                            Parcel* reply, 
                            uint32_t flags
                            )
{
    // Once a binder has died, it will never come back to life.
    if (mAlive) {
        status_t status = IPCThreadState::self()->transact( mHandle, code, data, reply, flags);
        if (status == DEAD_OBJECT) mAlive = 0;
        return status;
    }

    return DEAD_OBJECT;
}


//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IPCThreadState.cpp
//（1）p144 劳者一份的IPCThreadState：它是进程中真正干活的伙计
//返回一个 IPCThreadState 对象
//注意：每个线程都有一个 IPCThreadState
IPCThreadState* IPCThreadState::self()
{
    if (gHaveTLS) { //第一次进来为false
restart:
        const pthread_key_t k = gTLS;
        
        //从TLS中获得保存在其中的 IPCThreadState 对象
        IPCThreadState* st = (IPCThreadState*)pthread_getspecific(k);
        if (st) return st;
        return new IPCThreadState; //没有则新建一个
    }

    if (gShutdown) return NULL;

    pthread_mutex_lock(&gTLSMutex);
    if (!gHaveTLS) {
        if (pthread_key_create(&gTLS, threadDestructor) != 0) {
            pthread_mutex_unlock(&gTLSMutex);
            return NULL;
        }
        gHaveTLS = true;
    }
    pthread_mutex_unlock(&gTLSMutex);
    goto restart;
}

//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IPCThreadState.cpp#IPCThreadState
//p144 IPCThreadState 的构造函数
IPCThreadState::IPCThreadState(): 
                    mProcess(ProcessState::self()), 
                    mMyThreadId(androidGetTid())
{
    //在构造函数中，把自己设置到线程本地存储中去
    pthread_setspecific(gTLS, this);  
    clearCaller();
    
    //mIn和mOut是两个Parcel。把它看成是发送和接收命令的缓冲区即可。
    mIn.setDataCapacity(256);
    mOut.setDataCapacity(256);
}
/*每个线程都有一个 IPCThreadState， 每个 IPCThreadState中都有一个mIn、mOut，
其中，mIn是用来接收来自Binder设备的数据，
而mOut则是用来存储发往Binder设备数据的。
*/


//（2） P144 勤劳的 transact
//IPCThreadState 的 transact 方法，这个函数时机完成了与Binder通信的工作
//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IPCThreadState.cpp#IPCThreadState
status_t IPCThreadState::transact(int32_t handle, //前面b = new BpBinder(handle=0); 所以此时 handle=0
                                  uint32_t code, 
                                  const Parcel& data,
                                  Parcel* reply, 
                                  uint32_t flags)
{
    status_t err = data.errorCheck();

    flags |= TF_ACCEPT_FDS;

    // BC_TRANSACTION 是应用程序向binder设备发送消息的消息码
    // 而 binder设备向应用程序回复消息的消息码以 BR_ 开头，如 BR_TRANSACTION
    // 消息码定义在 binder_module.h 中
    err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, NULL);
    err = waitForResponse(reply);
    return err;
}


//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IPCThreadState.cpp#writeTransactionData
//p145 把命令写到mOut中去，而不是直接发出去
// 把 BC_TRANSACTION(即cmd)、handle 等写到mOut中去，还没有发送请求
status_t IPCThreadState::writeTransactionData(
                    int32_t cmd, 
                    uint32_t binderFlags,
                    int32_t handle, 
                    uint32_t code, 
                    const Parcel& data, 
                    status_t* statusBuffer)
{
    binder_transaction_data tr;
    //果然，handle 的值传递给了target，用来标识目的端，其中0是ServiceManager的标志
    tr.target.handle = handle;
    //code是消息码，是用来switch/case的!
    tr.code = code;
    tr.flags = binderFlags;

    const status_t err = data.errorCheck();
    if (err == NO_ERROR) {
        tr.data_size = data.ipcDataSize();
        tr.data.ptr.buffer = data.ipcData();
        tr.offsets_size = data.ipcObjectsCount()*sizeof(size_t);
        tr.data.ptr.offsets = data.ipcObjects();
    } else if (statusBuffer) {
        tr.flags |= TF_STATUS_CODE;
        *statusBuffer = err;
        tr.data_size = sizeof(status_t);
        tr.data.ptr.buffer = statusBuffer;
        tr.offsets_size = 0;
        tr.data.ptr.offsets = NULL;
    } else {
        return (mLastError = err);
    }
    
    //把命令写到mOut中去，而不是直接发出去，可见这个函数有点名不副实
    mOut.writeInt32(cmd);
    mOut.write(&tr, sizeof(tr));

    return NO_ERROR;
}


//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IPCThreadState.cpp#writeTransactionData
//p146 真正的发送请求和接收回复
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    int32_t cmd;
    int32_t err;

    while (1) {
        //talkWithDriver! 真正的发送请求
        if ((err=talkWithDriver()) < NO_ERROR) break;
        err = mIn.errorCheck();
        if (err < NO_ERROR) break;
        if (mIn.dataAvail() == 0) continue;

        cmd = mIn.readInt32();
        switch (cmd) {
        case BR_TRANSACTION_COMPLETE:
            if (!reply && !acquireResult) goto finish;
            break;
        ......
        default:
            err = executeCommand(cmd);  //这个是接收回复
            if (err != NO_ERROR) goto finish;
            break;
        }
    }

finish:
    if (err != NO_ERROR) {
        if (acquireResult) *acquireResult = err;
        if (reply) reply->setError(err);
        mLastError = err;
    }

    return err;
}


//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IPCThreadState.cpp#talkWithDriver
//p148 如何和Binder设备交互 (使用ioctl)
status_t IPCThreadState::talkWithDriver(bool doReceive)
{
    //binder_write_read是用来与binder设备交换数据的结构
    binder_write_read bwr;
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();
    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;
    //请求命令的填充
    bwr.write_size = outAvail;
    bwr.write_buffer = (long unsigned int)mOut.data();

    // This is what we'll read.
    if (doReceive && needRead) {
        //接收数据缓冲区信息的填充。 如果以后接收到数据，就直接填在mIn中了。
        bwr.read_size = mIn.dataCapacity();
        bwr.read_buffer = (long unsigned int)mIn.data();
    } else {
        bwr.read_size = 0;
    }
    
    // Return immediately if there is nothing to do.
    if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;
    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    status_t err;
    do {
#if defined(HAVE_ANDROID_OS)
        //看来不是read/write调用，而是ioctl方式
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
        else
            err = -errno;
#else
        err = INVALID_OPERATION;
#endif
        IF_LOG_COMMANDS() {
            alog << "Finished read/write, write size = " << mOut.dataSize() << endl;
        }
    } while (err == -EINTR);
    
    if (err >= NO_ERROR) {
        if (bwr.write_consumed > 0) {
            if (bwr.write_consumed < (ssize_t)mOut.dataSize())
                mOut.remove(0, bwr.write_consumed);
            else
                mOut.setDataSize(0);
        }
        if (bwr.read_consumed > 0) {
            mIn.setDataSize(bwr.read_consumed);
            mIn.setDataPosition(0);
        }
        return NO_ERROR;
    }

    return err;
}



//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IPCThreadState.cpp#executeCommand
//P147 处理回复
status_t IPCThreadState::executeCommand(int32_t cmd)
{
    BBinder* obj;
    RefBase::weakref_type* refs;
    status_t result = NO_ERROR;

    switch (cmd) {
    case BR_ERROR:
        result = mIn.readInt32();
        break;

    ......
    case BR_TRANSACTION:
        {
            binder_transaction_data tr;
            result = mIn.read(&tr, sizeof(tr));
            if (result != NO_ERROR) break;

            Parcel buffer;
            Parcel reply; 
            if (tr.target.ptr) {
                //p147 这里的b实际上就是实现 BnServiceXXX(对应注册的各个Service)，关于它的作用，我们要在6.5节中讲解
                //p160
                sp<BBinder> b((BBinder*)tr.cookie);
                const status_t error = b->transact(tr.code, buffer, &reply, 0);
                if (error < NO_ERROR) reply.setError(error);

            } 
            else {
                //the_context_object.是 IPCThreadState.cpp中定义的一个全局变量，
                //可通过 setTheContextObject 函数设置
                const status_t error = the_context_object->transact(tr.code, buffer, &reply, 0);
                if (error < NO_ERROR) reply.setError(error);
            }
            ......
        }
        break;

    case BR_DEAD_BINDER:
        {
            //收到 Binder 驱动发来的 Service死掉的消息，看来只有Bp端能收到了
            BpBinder *proxy = (BpBinder*)mIn.readInt32();
            proxy->sendObituary();
            mOut.writeInt32(BC_DEAD_BINDER_DONE);
            mOut.writeInt32((int32_t)proxy);
        } break;
    ......
    case BR_SPAWN_LOOPER:
        //特别注意，这里将收到来自驱动的指示以创建一个新线程，用于和Binder通信
        mProcess->spawnPooledThread(false);
        break;

    default:
        printf("*** BAD COMMAND %d received from Binder driver\n", cmd);
        result = UNKNOWN_ERROR;
        break;
    }

    if (result != NO_ERROR) {
        mLastError = result;
    }

    return result;
}


//【6.2.5】 startThreadPool 和 joinThreadPool
//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/ProcessState.cpp
void ProcessState::startThreadPool()
{
    AutoMutex _l(mLock);
    if (!mThreadPoolStarted) {
        mThreadPoolStarted = true;
        spawnPooledThread(true);
    }
}

//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/ProcessState.cpp
void ProcessState::spawnPooledThread(bool isMain)
{
    if (mThreadPoolStarted) {
        int32_t s = android_atomic_add(1, &mThreadPoolSeq);
        char buf[32];
        sprintf(buf, "Binder Thread #%d", s);
        LOGV("Spawning new pooled thread, name=%s\n", buf);
        
        //又创建了一个线程，并调用.joinThreadPool()
        sp<Thread> t = new PoolThread(isMain);
        t->run(buf);
    }
}

//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/ProcessState.cpp
class PoolThread : public Thread
{
public:
    PoolThread(bool isMain): mIsMain(isMain)
    {
    }

protected:
    virtual bool threadLoop()
    {
        IPCThreadState::self()->joinThreadPool(mIsMain);
        return false;
    }

    const bool mIsMain;
};




//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IPCThreadState.cpp
//p151
void IPCThreadState::joinThreadPool(bool isMain)
{
    //注意，如果isMain为true,我们则需要循环处理。 把请求信息写到mOut中，待会一起发出去
    mOut.writeInt32(isMain ? BC_ENTER_LOOPER : BC_REGISTER_LOOPER);

    androidSetThreadSchedulingGroup(mMyThreadId, ANDROID_TGROUP_DEFAULT);

    status_t result;
    do {
        int32_t cmd;

        // When we've cleared the incoming command queue, process any pending derefs
        if (mIn.dataPosition() >= mIn.dataSize()) {
            size_t numPending = mPendingWeakDerefs.size();
            if (numPending > 0) {
                for (size_t i = 0; i < numPending; i++) {
                    RefBase::weakref_type* refs = mPendingWeakDerefs[i];
                    refs->decWeak(mProcess.get());
                }
                mPendingWeakDerefs.clear();
            }
            
            //处理已死亡的BBinder对象
            numPending = mPendingStrongDerefs.size();
            if (numPending > 0) {
                for (size_t i = 0; i < numPending; i++) {
                    BBinder* obj = mPendingStrongDerefs[i];
                    obj->decStrong(mProcess.get());
                }
                mPendingStrongDerefs.clear();
            }
        }

        // 发送命令读取请求
        result = talkWithDriver();
        if (result >= NO_ERROR) {
            size_t IN = mIn.dataAvail();
            if (IN < sizeof(int32_t)) continue;
            cmd = mIn.readInt32();
            result = executeCommand(cmd);
        }
        if(result == TIMED_OUT && !isMain) {
            break;
        }
    } while (result != -ECONNREFUSED && result != -EBADF);

    LOG_THREADPOOL("**** THREAD %p (PID %d) IS LEAVING THE THREAD POOL err=%p\n",
        (void*)pthread_self(), getpid(), (void*)result);

    mOut.writeInt32(BC_EXIT_LOOPER);
    talkWithDriver(false);
}