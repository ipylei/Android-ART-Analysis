int main(int argc, char** argv)
{
    //【6.2.2】
    //【1.】获得一个ProcessState实例
    sp<ProcessState> proc(ProcessState::self());
    
    //【6.2.3】
    //【2.】MediaServer(MS) 作为 ServiceManager 的客户端，要向 ServiceManager 注册服务
    //调用 defaultServiceManager，得到一个 IServiceManager
    sp<IServiceManager> sm = defaultServiceManager(){
        sp<IBinder> tmp = ProcessState::self()->getContextObject(NULL){
            return getStrongProxyForHandle(0){
                sp<IBinder> result;
                b = new BpBinder(handle=0);  //创建一个BpBinder，所以下面的 mHandle = 0
                e->binder = b;             //填充 entry 的内容
                result = b;
                return result;
            }
        }      
        gDefaultServiceManager = interface_cast<IServiceManager>(tmp){
            const sp<IBinder>& obj = tmp;
            return IServiceManager::asInterface(obj){
                sp<IServiceManager> intr;       
                //调用有参构造函数，即转换函数？ obj即将作为其mRemote属性
                //BpServiceManager能向上转型为IServiceManager吗？
                intr = new BpServiceManager(obj); 
                return intr;         
            }
        }
        return gDefaultServiceManager;
    }
    LOGI("ServiceManager: %p", sm.get());
    
    //初始化音频系统的AudioFlinger服务
    AudioFlinger::instantiate();
    
    //【6.2.4】
    //【3.】多媒体系统的 MediaPlayer 服务，我们将以它作为主切入点
    MediaPlayerService::instantiate(){
        defaultServiceManager()->addService(String16("media.player"), new MediaPlayerService()){
            //remote()返回的是 mRemote，也就是【BpBinder】对象
            //【data】包含对象：new MediaPlayerService()
            status_t err = remote()->transact(ADD_SERVICE_TRANSACTION, data, &reply){
                
                //【IPCThreadState 是进程中真正干活的伙计】
                //注意参数：mHandle 是 new BpBinder(0) 中的参数 0
                //status_t status = IPCThreadState::self()->transact( mHandle, code, data, reply, flags);
                //-----------------------------------------------
                //拆分分析-1
                IPCThreadState ipc = IPCThreadState::self(){
                    //没有则新建一个
                    //注意构造函数！ mProcess = ProcessState::self()，所以与/dev/binder联系上了！
                    return new IPCThreadState(): mProcess(ProcessState::self()),  
                                                 mMyThreadId(androidGetTid()){
                        //在构造函数中，把自己设置到线程本地存储中去
                        pthread_setspecific(gTLS, this);  
                        clearCaller();
                        //mIn和mOut是两个Parcel。把它看成是发送和接收命令的缓冲区即可。
                        mIn.setDataCapacity(256);
                        mOut.setDataCapacity(256);
                    }
                }
                //拆分分析-2
                status_t status = ipc->transact( handle = mHandle, code, data, reply, flags){
                    //把命令写到mOut中去，而不是直接发出去
                    err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, NULL){
                        //把命令写到mOut中去，而不是直接发出去，可见这个函数有点名不副实
                        mOut.writeInt32(cmd);
                        mOut.write(&tr, sizeof(tr));
                    }
                    
                    //【*】真正的发送请求和接收回复
                    err = waitForResponse(reply){
                        //cmd的值？
                        cmd = mIn.readInt32();                        
                        case BR_TRANSACTION_COMPLETE:
                            //【*】真正的发送请求: talkWithDriver()
                            err = talkWithDriver(){
                                //请求命令的填充
                                bwr.write_size = outAvail;
                                bwr.write_buffer = (long unsigned int)mOut.data();
                                //接收数据缓冲区信息的填充
                                bwr.read_size = mIn.dataCapacity();                            
                                bwr.read_buffer = (long unsigned int)mIn.data();
                                //系统调用:   
                                //mProcess = ProcessState::self()
                                //mDriverFD = mDriverFD(open_driver()) 
                                ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0
                            }
                            
                        default:
                            //这个是处理回复
                            err = executeCommand(cmd){
                                // 【BBinder】
                                if (tr.target.ptr) {
                                    //BnServiceManager 与 BpServiceManager(向上转型为IServiceManager) 对应！
                                    //p147 这里的b实际上就是实现 BnServiceManager，关于它的作用，我们要在6.5节中讲解
                                    sp<BBinder> b((BBinder*)tr.cookie);
                                    const status_t error = b->transact(tr.code, buffer, &reply, 0);
                                    if (error < NO_ERROR) reply.setError(error);
                                }
                                else{
                                    const status_t error = the_context_object->transact(tr.code, buffer, &reply, 0);
                                    if (error < NO_ERROR) reply.setError(error);
                                }
                                
                            }
                    }
                    
                }
                return status;
            }
        }
    }
    
    //CameraService 服务
    CameraService::instantiate();
    //音频系统的AudioPolicy服务
    AudioPolicyService::instantiate();
    
    //【6.2.5】
    //【4.】 创建新线程，然后在新线程中：IPCThreadState::self()->joinThreadPool();
    //每个线程都有一个IPCThreadState
    //里面新启动线程去读取binder设备，查看是否有请求
    ProcessState::self()->startThreadPool(){
        spawnPooledThread(true){
            sp<Thread> t = new PoolThread(isMain);
            t->run(buf){
                IPCThreadState::self()->joinThreadPool(mIsMain){
                    //注意，如果isMain为true,我们则需要循环处理。 把请求信息写到mOut中，待会一起发出去
                    mOut.writeInt32(isMain ? BC_ENTER_LOOPER : BC_REGISTER_LOOPER); 
                    
                    //循环处理
                    do {
                        //处理已死亡的 BBinder 对象
                        ...
                        
                        //【*】发送命令读取请求
                        result = talkWithDriver(); //IPCThreadState有属性.mProcess
                    } while (result != -ECONNREFUSED && result != -EBADF);    
                }
            }
        }
    }
    
    //【5.】主线程也调用joinThreadPool()读取binder设备，查看是否有请求
    IPCThreadState::self()->joinThreadPool(){
        //注意，如果isMain为true,我们则需要循环处理。 把请求信息写到mOut中，待会一起发出去
        mOut.writeInt32(isMain ? BC_ENTER_LOOPER : BC_REGISTER_LOOPER); 
        
        //循环处理
        do {
            //处理已死亡的 BBinder 对象
            ...
            
            //【*】发送命令读取请求
            result = talkWithDriver();  //IPCThreadState有属性.mProcess
        } while (result != -ECONNREFUSED && result != -EBADF); 
    }
}
