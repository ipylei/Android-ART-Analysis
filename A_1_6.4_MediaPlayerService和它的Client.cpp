//【6.4】

//【6.4.1】查询 ServiceManager
/*注册服务 与 获取服务
注册服务：
    1. 创建一个 BpBinder，通过障眼法转为 IServiceManager 
    sp<IServiceManager> IServiceManager::asInterface(const sp<IBinder>& obj){
        return new BpServiceManager(obj);   (mRemote 为 BBinder)
    }
    
    2. 调用 MediaPlayerService::instantiate() {
        BpServiceManager::addService(String16("media.player"), new MediaPlayerService())
    }

获取服务：
    1.获取一个 BpBinder， 然后通过障眼法转为 BpMediaPlayerService

*/
http://androidxref.com/2.2.3/xref/frameworks/base/media/libmedia/IMediaDeathNotifier.cpp
/*这个函数通过与 ServiceManager 通信，获得一个能够与 MediaPlayerService 通信的 BpBinder，
   然后再通过障眼法 interface_cast，转换成一个 BpMediaPlayerService
*/
IMediaDeathNotifier::getMediaPlayerService()
{
    LOGV("getMediaPlayerService");
    Mutex::Autolock _l(sServiceLock);
    if (sMediaPlayerService.get() == 0) {
        
        sp<IServiceManager> sm = defaultServiceManager();
        sp<IBinder> binder;
        do {
            //向 ServiceManager 查询对应服务的信息，返回 BpBinder
            binder = sm->getService(String16("media.player"));
            if (binder != 0) {
                break;
             }
            //如果 ServiceManager 上还没有注册对应的服务，则需要等待，
            //直到对应服务注册到 ServiceManager 中为止(break)
            usleep(500000); // 0.5 s
        } while(true);

        if (sDeathNotifier == NULL) {
            sDeathNotifier = new DeathNotifier();
        }
        binder->linkToDeath(sDeathNotifier);
        //通过 interface_cast， 将这个 binder 转化为 BpMediaPlayerService
        // binder 中 handle 标识的一定是 目的端  MediaPlayerService
        sMediaPlayerService = interface_cast<IMediaPlayerService>(binder);
    }
    LOGE_IF(sMediaPlayerService == 0, "no media player service!?");
    return sMediaPlayerService;
}
//IMediaPlayerService.cpp


//【P159】Client进程获取到Service，然后就可以使用了，即可以和 Server 进程(MediaSever->main())通信了

//【6.4.2】 子承父业 (Server 进程对于通信处理：即处理发送过来的消息)
//159 处理回复  假设MediaSever->main()中，有一个线程收到了请求信息
//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/IPCThreadState.cpp#executeCommand
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
                //p147 这里的b实际上就是实现 BnServiceXXX(对应注册的各个Service)，
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


//http://androidxref.com/2.2.3/xref/frameworks/base/libs/binder/Binder.cpp
status_t BBinder::transact(uint32_t code, 
                        const Parcel& data, 
                        Parcel* reply, 
                        uint32_t flags)
{
    data.setDataPosition(0);

    status_t err = NO_ERROR;
    switch (code) {
        case PING_TRANSACTION:
            reply->writeInt32(pingBinder());
            break;
        default:
            //调用子类（BnMediaPlayerService）的 onTransact，这是一个虚函数
            err = onTransact(code, data, reply, flags);
            break;
    }

    if (reply != NULL) {
        reply->setDataPosition(0);
    }

    return err;
}


status_t BBinder::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch (code) {
        case INTERFACE_TRANSACTION:
            reply->writeString16(getInterfaceDescriptor());
            return NO_ERROR;

        case DUMP_TRANSACTION: {
            int fd = data.readFileDescriptor();
            int argc = data.readInt32();
            Vector<String16> args;
            for (int i = 0; i < argc && data.dataAvail() > 0; i++) {
               args.add(data.readString16());
            }
            return dump(fd, args);
        }
        default:
            return UNKNOWN_TRANSACTION;
    }
}


//http://androidxref.com/2.2.3/xref/frameworks/base/media/libmedia/IMediaPlayerService.cpp
status_t BnMediaPlayerService::onTransact(uint32_t code, 
                                          const Parcel& data, 
                                          Parcel* reply, 
                                          uint32_t flags)
{
    switch(code) {
        ...
        case CREATE_MEDIA_RECORDER: {
            CHECK_INTERFACE(IMediaPlayerService, data, reply);
            //从请求数据中解析对应的参数
            pid_t pid = data.readInt32();
            //子类要实现 createMediaRecorder() 函数
            sp<IMediaRecorder> recorder = createMediaRecorder(pid);
            reply->writeStrongBinder(recorder->asBinder());
            return NO_ERROR;
        } break;
        case CREATE_METADATA_RETRIEVER: {
            CHECK_INTERFACE(IMediaPlayerService, data, reply);
            pid_t pid = data.readInt32();
            //子类要实现 createMetadataRetriever() 函数
            sp<IMediaMetadataRetriever> retriever = createMetadataRetriever(pid);
            reply->writeStrongBinder(retriever->asBinder());
            return NO_ERROR;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}