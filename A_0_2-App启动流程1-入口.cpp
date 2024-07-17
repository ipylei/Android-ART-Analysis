【3.3】 APP启动流程
/*当用户点击桌面上一个APP图标时，这个APP的启动流程大致如下：
(1)点击APP图标，产生Click Event事件；
(2)Launcher 程序接收到Click Event事件，调用startActivity(Intent)，通过Binder IPC机制调用ActivityManager Service的服务；
(3)Activity Manager Service 会调用 startProcessLocked 方法来创建新的进程；
(4)startProcessLocked 方法调用Process类的静态成员函数start与 打开socket与zygote进程进行通信，
    并指定APP进程的入口函数为android.app.ActivityThread 类的静态成员函数main；
    zygote fork出app进程，并执行"android.app.ActivityThread.main"
(5)main方法成功创建 ActivityThread 对象后，再调用 attach 方法完成初始化，然后进入消息循环，直到进程退出
*/



【3.3.1】 指定app的入口方法
/*startProcessLocked方法调用Process类的start方法创建应用程序的进程，
并指定android.app.ActivityThread类的main方法为入口函数
*/
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/services/java/com/android/server/am/ActivityManagerService.java
private final void ActivityManagerService::startProcessLocked(ProcessRecord app, String hostingType, String hostingNameStr) {
    
    ...
    Process.ProcessStartResult startResult = Process.start("android.app.ActivityThread",
                app.processName, uid, uid, gids, debugFlags, mountExternal,
                app.info.targetSdkVersion, app.info.seinfo, null);
}


/*main方法首先创建ActivityThread类对象，并调用attach方法完成初始化，
然后调用Looper.loop()方法进入消息循环。
*/
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
public static void ActivityThread::main(String[] args) {
    
    ......
    Looper.prepareMainLooper();

    ActivityThread thread = new ActivityThread();
    thread.attach(false);

    Looper.loop();

    throw new RuntimeException("Main thread loop unexpectedly exited");
}


//（Client 端 -> Service 端）
//调用thread.attach(false)完成一系列初始化准备工作，并完成全局静态变量sCurrentActivityThread的初始化
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
private void ActivityThread::attach(boolean system) {
    sCurrentActivityThread = this;
 
    mSystemThread = system;
    if (!system) {
        ViewRootImpl.addFirstDrawHandler(new Runnable() {
            @Override
            public void run() {
                ensureJitEnabled();
            }
        });
        android.ddm.DdmHandleAppName.setAppName("<pre-initialized>",UserHandle.myUserId());
        RuntimeInit.setApplicationObject(mAppThread.asBinder());
        
        //获取 ActivityManagerService 实例
        IActivityManager mgr = ActivityManagerNative.getDefault();
        try {
            //这里，【ActivityManagerService】对象通过 attachApplication() 绑定 ApplicationThread 对象 mAppThread
            //注意：mAppThread 的类型为 ApplicationThread
            //【*】
            //http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/services/java/com/android/server/am/ActivityManagerService.java
            mgr.attachApplication(mAppThread);  //【进入 ActivityManagerService 端】
        } catch (RemoteException ex) {
            // Ignore
        }
    } 
}


//(内部类: 基于 Binder 通信)
//ApplicationThread 是 ActivityThread 的【内部类】
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
private class ApplicationThread extends ApplicationThreadNative {
    ...
}
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ApplicationThreadNative.java
public abstract class ApplicationThreadNative extends Binder implements IApplicationThread {
    ...
}


//（到达 Service 端）
/*ApplicationThreadNative 抽象类继承了 Binder 类并实现了 IApplicationThread 接口。
所以，传递给 attachApplication()参数的是 ApplicationThread 类型的 Binder 对象，它主要的作用是用来进行进程间的通信。
下面进入 ActivityManagerService， 查看 attachApplication 方法。
*/
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/services/java/com/android/server/am/ActivityManagerService.java
public final void ActivityManagerService::attachApplication(IApplicationThread thread) {
    synchronized (this) {
        int callingPid = Binder.getCallingPid();
        final long origId = Binder.clearCallingIdentity();
        //*
        attachApplicationLocked(thread, callingPid);
        Binder.restoreCallingIdentity(origId);
    }
}


//（Service 端 -> Client 端）
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/services/java/com/android/server/am/ActivityManagerService.java
private final boolean ActivityManagerService::attachApplicationLocked(IApplicationThread thread, int pid) {
    //创建app对象！
    ProcessRecord app;
    if (pid != MY_PID && pid >= 0) {
        synchronized (mPidsSelfLocked) {
            app = mPidsSelfLocked.get(pid);
        }
    } 
    else {
        app = null;
    }
    
    ensurePackageDexOpt(app.instrumentationInfo != null ? app.instrumentationInfo.packageName: app.info.packageName);
    if (app.instrumentationClass != null) {
        ensurePackageDexOpt(app.instrumentationClass.getPackageName());
    }
    
    //【*】（发送回 Client 端）
    //传递过来的thread对象是 ApplicationThread 类型的Binder对象
    //所以这时是在 Service 端，是跨进程调用 bindApplication() 
    thread.bindApplication(processName, appInfo, providers,
        app.instrumentationClass, profileFile, profileFd, profileAutoStop,
        app.instrumentationArguments, app.instrumentationWatcher,
        app.instrumentationUiAutomationConnection, testMode, enableOpenGlTrace,
        isRestrictedBackupMode || !normalMode, app.persistent,
        new Configuration(mConfiguration), app.compat, getCommonServicesLocked(),
        mCoreSettingsObserver.getCoreSettingsLocked());
        
    updateLruProcessLocked(app, false, null);
    
    //【*】继续往下执行！ 参考A_0_2-App启动流程2-初始化.cpp
    //启动 Activity
    // See if the top visible activity is waiting to run in this process...
    if (normalMode) {
        try {
            //http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/services/java/com/android/server/am/ActivityStackSupervisor.java
            if (mStackSupervisor.attachApplicationLocked(app, mHeadless)) {
                didSomething = true;
            }
        } catch (Exception e) {
            badApp = true;
        }
    }    
    
    //启动 Service
    // Find any services that should be running in this process...
    if (!badApp) {
        try {
            didSomething |= mServices.attachApplicationLocked(app, processName);
        } catch (Exception e) {
            badApp = true;
        }
    }
    
    ......
}



//（回到 Client 端 : Binder）
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
public final void ApplicationThread::bindApplication(String processName,
        ApplicationInfo appInfo, List<ProviderInfo> providers,
        ComponentName instrumentationName, String profileFile,
        ParcelFileDescriptor profileFd, boolean autoStopProfiler,
        Bundle instrumentationArgs, IInstrumentationWatcher instrumentationWatcher,
        IUiAutomationConnection instrumentationUiConnection, int debugMode,
        boolean enableOpenGlTrace, boolean isRestrictedBackupMode, boolean persistent,
        Configuration config, CompatibilityInfo compatInfo, Map<String, IBinder> services,
        Bundle coreSettings) {

    if (services != null) {
        // Setup the service cache in the ServiceManager
        ServiceManager.initServiceCache(services);
    }

    setCoreSettings(coreSettings);

    AppBindData data = new AppBindData();
    data.processName = processName;
    data.appInfo = appInfo;
    data.providers = providers;
    data.instrumentationName = instrumentationName;
    data.instrumentationArgs = instrumentationArgs;
    data.instrumentationWatcher = instrumentationWatcher;
    data.instrumentationUiAutomationConnection = instrumentationUiConnection;
    data.debugMode = debugMode;
    data.enableOpenGlTrace = enableOpenGlTrace;
    data.restrictedBackupMode = isRestrictedBackupMode;
    data.persistent = persistent;
    data.config = config;
    data.compatInfo = compatInfo;
    data.initProfileFile = profileFile;
    data.initProfileFd = profileFd;
    data.initAutoStopProfiler = false;
    //调用 ActivityThread::sendMessage(int what, Object obj)
    sendMessage(H.BIND_APPLICATION, data);  
}



【3.3.2】 发送BIND_APPLICATION消息并处理
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
private void ActivityThread::sendMessage(int what, Object obj) {
    sendMessage(what, obj, 0, 0, false);
}


//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
private void ActivityThread::sendMessage(int what, Object obj, int arg1, int arg2, boolean async) {
    Message msg = Message.obtain();
    msg.what = what;  //what = H.BIND_APPLICATION
    msg.obj = obj;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    if (async) {
        msg.setAsynchronous(true);
    }
    // mH对象发送消息
    mH.sendMessage(msg);  //final H mH = new H();
}


//（内部类 ：基于消息队列）
//ActivityThread的内部类 H
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
private class H extends Handler {
    public static final int BIND_APPLICATION        = 110
    public void handleMessage(Message msg) {
        switch (msg.what) {
            case LAUNCH_ACTIVITY: {
                Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "activityStart");
                ActivityClientRecord r = (ActivityClientRecord)msg.obj;

                r.packageInfo = getPackageInfoNoCheck(r.activityInfo.applicationInfo, r.compatInfo);
                handleLaunchActivity(r, null);
                Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
            } break;  
            
            ......
            //【*】
            case BIND_APPLICATION:
                Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "bindApplication");
                AppBindData data = (AppBindData)msg.obj;
                
                //[*]调用 ActivityThread::handleBindApplication() 处理传递过来的data对象数据
                handleBindApplication(data);
                Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
                break;
            ......
        }
    }
}

//（拓展：消息队列 -> 最终会走到 H::handleMessage(Message msg) --> 然后继续）
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/os/Handler.java
public final boolean Handler::sendMessage(Message msg){
    return sendMessageDelayed(msg, 0);
}
public final boolean Handler::sendMessageDelayed(Message msg, long delayMillis){
    if (delayMillis < 0) {
        delayMillis = 0;
    }
    return sendMessageAtTime(msg, SystemClock.uptimeMillis() + delayMillis);
}
public       boolean Handler::sendMessageAtTime(Message msg, long uptimeMillis) {
    MessageQueue queue = mQueue;
    if (queue == null) {
        RuntimeException e = new RuntimeException(
                this + " sendMessageAtTime() called with no mQueue");
        Log.w("Looper", e.getMessage(), e);
        return false;
    }
    return enqueueMessage(queue, msg, uptimeMillis);
}
private      boolean Handler::enqueueMessage(MessageQueue queue, Message msg, long uptimeMillis) {
    msg.target = this;
    if (mAsynchronous) {
        msg.setAsynchronous(true);
    }
    return queue.enqueueMessage(msg, uptimeMillis);
}
public void Handler::dispatchMessage(Message msg) {
    if (msg.callback != null) {
        handleCallback(msg);
    } else {
        if (mCallback != null) {
            if (mCallback.handleMessage(msg)) {
                return;
            }
        }
        handleMessage(msg);
    }
}

/* 【终于又回归主线了！】
    创建LoadedApk对象
    获取类加载器加载apk
    创建Application对象
*/
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
private void ActivityThread::handleBindApplication(AppBindData data) {
    mBoundApplication = data;
    
    //step 1: 创建 LoadedApk 对象
    data.info = getPackageInfoNoCheck(data.appInfo, data.compatInfo);
    
    //step 2: 创建 ContextImpl 对象;
    final ContextImpl appContext = ContextImpl.createAppContext(this, data.info);
    
    //step 3: 创建Instrumentation
    mInstrumentation = new Instrumentation();
    
    /*step 4: 创建 Application 对象; 
        在makeApplication函数中调用了newApplication，
        在newApplication函数中又调用了app.attach(context)，
        在attach函数中调用了Application.attachBaseContext()函数
    */
    //http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/LoadedApk.java
    Application app = data.info.makeApplication(data.restrictedBackupMode, null){
        String appClass = mApplicationInfo.className;
        if (forceDefaultAppClass || (appClass == null)) {
            appClass = "android.app.Application";
        }
        
        //创建 Application 对象
        //http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/Instrumentation.java
        app = mActivityThread.mInstrumentation.newApplication(cl, appClass, appContext){
            return Instrumentation::newApplication(cl.loadClass(className), context){
                //直接创建Application对象
                Application app = (Application)clazz.newInstance();
                //http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/Application.java
                app.attach(context){
                    Application::attachBaseContext(context);
                    mLoadedApk = ContextImpl.getImpl(context).mPackageInfo;
                }
                return app;
            }
        }
        return app;
    }
    mInitialApplication = app;
    
    
    //step 5: 安装providers
    List<ProviderInfo> providers = data.providers;
    installContentProviders(app, providers);
    
    //step 6: 执行Application.Create回调
    //http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/Instrumentation.java
    mInstrumentation.callApplicationOnCreate(app){
         app.onCreate();
    }
    
}


