//app_main.cpp
int main(int argc, const char * const argv[]){
    AppRuntime runtime;
    
    //AndroidRuntime::start(...)
    runtime.start(className="com.android.internal.os.ZygoteInit", bool startSystemServer){
        //在ART虚拟机对应的核心动态库加载到zyogte进程后，该函数将启动ART虚拟机。
        if(startVm(&mJavaVM, &env, zygote) != 0) { 
            ...;
        }
        
        //注册JNI函数，即给虚拟机注册一些JNI函数
        if(startReg(env)<0){
            goto bail;
        }
        
        jclass stringClass;
        jobjectArray strArray;
        strArray =  new String[2];
        strArray[0] = className;
        strArray[1] = "true";
        
        //Welcomt to Java World
       
        char* slashClassName = toSlashClassName(className);  //将"."换成"/"
        jclass startClass = env->FindClass(slashClassName);
        //找到ZygoteInit类的static main函数的jMethodId
        startMeth = env->GetStaticMethodID(startClass, "main", "([Ljava/lang/String;)V");
        env->callStaticVoidMethod(startClass, startMeth, strArray);
        
        //p69  等价上面
        com.android.internal.os.ZygoteInit.main(String argv[]){
            try{
                //1
                registerZygoteSocket();
                
                //2
                preloadClasses();
                preloadResources();
                
                //3.p72  启动system_server   
                startSystemServer(){
                    args = {
                        "--setuid=1000",
                        "--setgid=1000",
                        "--nice-name=system_server",
                        "com.android.server.SystemServer"
                    }
                    parsedArgs = new ZygoteConnection.Arguments(args);
                    //fork一个子进程，这个子进程就是system_server进程
                    //p75 //是一个native函数，实现为：Dalvik_dalvik_system_Zygote_forkSystemServer()
                    pid = Zygote.forkSystemServer(parsedArgs.uid, parsedArgs.gid, 
                            parsedArgs.gids, debugFlags, null){ 
                        
                        pid = forkAndSpecializeCommon(args);    
                        return pid;
                    }
                    
                    if(pid ==0 ){
                        //system_server进程的工作
                        //p77
                        handleSystemServerProcess(parsedArgs){
                            //p78 http://androidxref.com/2.2.3/xref/frameworks/base/core/java/com/android/internal/os/RuntimeInit.java
                            RuntimeInit.zygoteInit(parsedArgs.remainingArgs){
                                //1.native层的初始化
                                //p78
                                zygoteInitNative(){ //是一个native函数，实现为：com_android_internal_os_RuntimeInit_zygoteinit
                                    gCurRuntime->onZygoteInit(){
                                        proc->startThreadPool(); //启动一个线程，用于Binder通信
                                    }
                                }
                                //2.调用"com.android.server.SystemServer" 类的main函数
                                String startClass = argv[curArg++];
                                String[] startArgs = new String[argv.length - curArg];
                                System.arraycopy(argv, curArg, startArgs, 0, startArgs.length);
                                //p79
                                invokeStaticMain(startClass, startArgs){
                                    c1 = Class.forName(startClass);
                                    //找到com.android.server.SystemServer类的main函数
                                    m = c1.getMethod("main", new Class[]{String[].class});
                                    throw new ZygoteInit.MethodAndArgsCaller(m, argv);  //抛出异常，在ZygoteInit.main中截获
                                }
                            }
                        }
                    }
                }
                
                //4.p73 有求必应之等待请求
                //(使用1注册的用于IPC的sockct)
                runSelectLoopMode(){
                    
                    
                    if (index < 0) {
                        throw new RuntimeException("Error in select()");
                    } 
                    //处理客户端连接
                    else if (index == 0) {
                        ZygoteConnection newPeer = acceptCommandPeer();
                        peers.add(newPeer);
                        fds.add(newPeer.getFileDesciptor());
                    } 
                    //处理客户端请求，peers.get(index返回的是ZygoteConnection
                    //后续处理将交给ZygoteConnection.runOnce()函数完成
                    else {
                        
                        boolean done;
                        //【4.4.2】
                        //p86 http://androidxref.com/2.2.3/xref/frameworks/base/core/java/com/android/internal/os/ZygoteConnection.java
                        done = peers.get(index).runOnce(){
                            args = readArgumentList();  //读取SS(system_server进程发送过来的参数)
                            parsedArgs = new Arguments(args);
                            descriptors = mSocket.getAncillaryFileDescriptors();
                            //根据函数名，可知Zygote又分裂出了一个子进程
                            pid = Zygote.forkAndSpecialize(parsedArgs.uid, parsedArgs.gid, parsedArgs.gids, parsedArgs.debugFlags, rlimits);
                            //子进程处理
                            if(pid == 0){
                                //p87 
                                handleChildProc(parsedArgs, descriptors, newStderr){
                                    //system_server进程发来的参数有"--runtime-init"，所以这里为true
                                    if (parsedArgs.runtimeInit) {
                                        RuntimeInit.zygoteInit(parsedArgs.remainingArgs){
                                            commonInit();
                                            //1.native层的初始化
                                            zygoteInitNative(){  //同处理system_server，是一个native函数，实现为：com_android_internal_os_RuntimeInit_zygoteinit
                                                gCurRuntime->onZygoteInit(){
                                                    proc->startThreadPool(); //启动一个线程，用于Binder通信
                                                }
                                            }
                                            //2.调用"android.app.ActivityThread" 类的main函数
                                            String startClass = argv[curArg++];
                                            String[] startArgs = new String[argv.length - curArg];
                                            System.arraycopy(argv, curArg, startArgs, 0, startArgs.length);
                                            invokeStaticMain(startClass, startArgs);
                                        }
                                    }
                                }
                                return true;
                            }
                            //zygote进程
                            else{
                                return handleParentProc(pid, descriptors, parsedArgs);  
                            }
                                
                            
                        }

                        if (done) {
                            peers.remove(index);
                            fds.remove(index);
                        }
                    }
                    
                }
                
                caller.run();
            }catch(MethodAndArgsCaller caller){
                caller.run(){  //5.很重要的caller run函数
                    mMethod.invoke(null, new Object[] {mArgs});
                }
            }catch(RuntimeException ex){
                closeServerSocket();
                throw ex;
            }
            
        }
    }
    
}


//SystemServer的真面目：ZygoteInit分裂产生的system_server，其实就是为了调用 com.android.server.SystemServer 的main函数！
//p81 http://androidxref.com/2.2.3/xref/frameworks/base/services/java/com/android/server/SystemServer.java
//    http://androidxref.com/2.2.3/xref/frameworks/base/services/jni/com_android_server_SystemServer.cpp
com.android.server.SystemServer.main(String[] args){
    System.loadLibrary("android_servers");
    //调用native的 initl 函数
    init1(args){  //是一个native函数，实现为：com_android_server_SystemServer_init1(JNIEnv * env, jobject clazz)
        system_init(){  //调用另外一个函数(在system_init.cpp中)
            
            AndroidRuntime * runtime = AndroidRuntime::getRuntime();
            //1.调用 init2 函数
            runtime->callStatic("com.android.server.SystemServer", "init2"){
                Thread thr = new ServerThread();
                thr.setName("android.server.ServerThread");
                thr.start(){  //启动一个ServerThread
                    //启动Entropy Service
                    //启动电源管理服务
                    //启动电池管理服务
                    //初始化看门狗
                    Watchdog.getInstannce().init(context, battery, power, alarm, ActivityManagerService.self());
                    //启动WindowManager服务
                    //启动ActivityManager服务
                    ...//启动其他服务
                    
                    Looper.loop() //进行消息循环，然后处理消息
                    
                }
            }
            
            //2.当前线程也加入到 Binder 通信的大潮中
        }
    }
    
}


//请求一个Activity时，而这个Activity附属于一个还未启动的进程，那么这个进程该如何启动呢？
//ActivityManagerService也是由ServiceManager创建的
//ActivityManagerService 发送请求
//p84 http://androidxref.com/2.2.3/xref/frameworks/base/services/java/com/android/server/am/ActivityManagerService.java
private final void startProcessLocked(ProcessRecord app, String hostingType, String hostingNameStr) {

    if ("1".equals(SystemProperties.get("debug.checkjni"))) {
        debugFlags |= Zygote.DEBUG_ENABLE_CHECKJNI;
    }
    if ("1".equals(SystemProperties.get("debug.assert"))) {
        debugFlags |= Zygote.DEBUG_ENABLE_ASSERT;
    }
    
    //p84 
    int pid = Process.start("android.app.ActivityThread", mSimpleProcessManagement ? app.processName : null, 
        uid, uid, gids, debugFlags, null){
        //p84 http://androidxref.com/2.2.3/xref/frameworks/base/core/java/android/os/Process.java
        //processClass="android.app.ActivityThread"
        return startViaZygote(processClass, niceName, uid, gid, gids, debugFlags, zygoteArgs){
            argsForZygote.add("--runtime-init");
            argsForZygote.add("--setuid=" + uid);
            argsForZygote.add("--setgid=" + gid);
            argsForZygote.add(processClass);
             
            pid = zygoteSendArgsAndGetPid(argsForZygote:args){
                //是不是打开了和zygote通信的Socket
                //p85
                openZygoteSocketIfNeeded(){
                
                    //果真如此!
                    sZygoteSocket = new LocalSocket();
                    sZygoteSocket.connect(new LocalSocketAddress(ZYGOTE_SOCKET, LocalSocketAddress.Namespace.RESERVED));
                    sZygoteInputStream = new DataInputStream(sZygoteSocket.getInputStream());
                    sZygoteWriter = new BufferedWriter(new OutputStreamWriter(sZygoteSocket.getOutputStream()), 256);

                }
                
                //把请求的参数发到Zygote，参数中有一个字符串:"android.app.ActivityThread"
                sZygoteWriter.write(Integer.toString(args.size()));
                sZygoteWriter.newLine();
                sZygoteWriter.write(arg);
                sZygoteWriter.newLine();
                
                //读取Zygote处理完的结果，便可知是某个进程的pid
                sZygoteWriter.flush();
                pid = sZygoteInputStream.readInt();
                return pid;
            }
            return pid;
        }            
                
    }
      
}
/*P86 由于 ActivityManagerService 驻留于SystemServer进程中，所以正是system_server进程向Zygote发送了消息
*/