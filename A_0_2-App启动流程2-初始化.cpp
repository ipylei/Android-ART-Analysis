【3.3.3】 创建LoadedApk对象
//（step 1: 创建 LoadedApk 对象）
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
public final LoadedApk ActivityThread::getPackageInfoNoCheck(ApplicationInfo ai, CompatibilityInfo compatInfo) {
    return getPackageInfo(ai, compatInfo, null, false, true);
}

//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
private LoadedApk ActivityThread::getPackageInfo(ApplicationInfo aInfo, CompatibilityInfo compatInfo,
        ClassLoader baseLoader, boolean securityViolation, boolean includeCode) {
            
    synchronized (mResourcesManager) {
        WeakReference<LoadedApk> ref;
        if (includeCode) {
            //goto here
            ref = mPackages.get(aInfo.packageName);
        } else {
            ref = mResourcePackages.get(aInfo.packageName);
        }
        LoadedApk packageInfo = ref != null ? ref.get() : null;
        if (packageInfo == null || (packageInfo.mResources != null && !packageInfo.mResources.getAssets().isUpToDate())) {     
            //创建 LoadedApk 对象
            packageInfo = new LoadedApk(this,  //倒反天罡第1步：this为ActivityThread
                                        aInfo, 
                                        compatInfo, 
                                        baseLoader, //null
                                        securityViolation, 
                                        includeCode && (aInfo.flags&ApplicationInfo.FLAG_HAS_CODE) != 0);
            
            if (includeCode) {
                //添加到 mPackages 列表中
                mPackages.put(aInfo.packageName, new WeakReference<LoadedApk>(packageInfo));
            } else {
                mResourcePackages.put(aInfo.packageName, new WeakReference<LoadedApk>(packageInfo));
            }
        }
        return packageInfo;
    }
}

//（step 4: 根据 step 1 创建的LoadedApk对象，继续创建 Application 对象）
//在 makeApplication 函数中调用了 Instrumentation::newApplication，
//在 newApplication 函数中又调用了 app.attach(context)，
//在 attach 函数中调用了 Application.attachBaseContext 函数
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/LoadedApk.java
public Application LoadedApk::makeApplication(boolean forceDefaultAppClass,
                                              Instrumentation instrumentation) {
    if (mApplication != null) {
        return mApplication;
    }

    Application app = null;
    
    //从 LoadedApk 中取出 Application类的完整包名
    String appClass = mApplicationInfo.className;
    if (forceDefaultAppClass || (appClass == null)) {
        appClass = "android.app.Application";
    }

    try {
        //【3.3.4】 获取类加载器
        java.lang.ClassLoader cl = getClassLoader();
        ContextImpl appContext = ContextImpl.createAppContext(mActivityThread, this);
        
        //【3.3.5】 创建 Application 对象
        //http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/Instrumentation.java
        app = mActivityThread.mInstrumentation.newApplication(cl, appClass, appContext){
            return Instrumentation::newApplication(cl.loadClass(className), context){
                //直接创建app对象
                Application app = (Application)clazz.newInstance();
                //http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/Application.java
                app.attach(context){
                    this.attachBaseContext(context);
                    mLoadedApk = ContextImpl.getImpl(context).mPackageInfo;
                }
                return app;
            }
        }
        appContext.setOuterContext(app);
    
    } catch (Exception e) {
        if (!mActivityThread.mInstrumentation.onException(app, e)) {
            throw new RuntimeException("Unable to instantiate application " + appClass + ": " + e.toString(), e);
        }
    }
    mActivityThread.mAllApplications.add(app);
    //倒反天罡第2步
    mApplication = app;

    if (instrumentation != null) {
        try {
            instrumentation.callApplicationOnCreate(app);
        } catch (Exception e) {
            if (!instrumentation.onException(app, e)) {
                throw new RuntimeException("Unable to create application " + app.getClass().getName() + ": " + e.toString(), e);
            }
        }
    }

    return app;
}



【3.3.4】 创建PathClassLoader加载dex
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/LoadedApk.java
public ClassLoader LoadedApk::getClassLoader() {
    synchronized (this) {
        if (mClassLoader != null) {
            return mClassLoader;
        }
        
        //第三方应用程序，且 mIncludeCode = true
        if (mIncludeCode && !mPackageName.equals("android")) {
            String zip = mAppDir;
            String libraryPath = mLibDir;
            String instrumentationAppDir = mActivityThread.mInstrumentationAppDir;
            String instrumentationAppLibraryDir =  mActivityThread.mInstrumentationAppLibraryDir;
            String instrumentationAppPackage = mActivityThread.mInstrumentationAppPackage;
            String instrumentedAppDir = mActivityThread.mInstrumentedAppDir;
            String instrumentedAppLibraryDir = mActivityThread.mInstrumentedAppLibraryDir;
            String[] instrumentationLibs = null;

            if (mAppDir.equals(instrumentationAppDir) || mAppDir.equals(instrumentedAppDir)) {
                zip = instrumentationAppDir + ":" + instrumentedAppDir;
                libraryPath = instrumentationAppLibraryDir + ":" + instrumentedAppLibraryDir;
                if (! instrumentedAppDir.equals(instrumentationAppDir)) {
                    instrumentationLibs = getLibrariesFor(instrumentationAppPackage);
                }
            }

            if ((mSharedLibraries != null) || (instrumentationLibs != null)) {
                zip = combineLibs(mSharedLibraries, instrumentationLibs) + ':' + zip;
            }

            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            
            //mBaseClassLoader = null;
            //参考下面
            mClassLoader = ApplicationLoaders.getDefault().getClassLoader(zip, libraryPath, mBaseClassLoader);
            initializeJavaContextClassLoader();

            StrictMode.setThreadPolicy(oldPolicy);
        } 
        else {
            if (mBaseClassLoader == null) {
                mClassLoader = ClassLoader.getSystemClassLoader();
            } else {
                mClassLoader = mBaseClassLoader;
            }
        }
        return mClassLoader;
    }
}

//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ApplicationLoaders.java
public ClassLoader ApplicationLoaders::getClassLoader(String zip, 
                                                      String libPath, 
                                                      ClassLoader parent  //parent为null
                                                      ){
    
    // ClassLoader.getSystemClassLoader() 返回类型为 PathClassLoader
    // 所以 baseParent 为 BootClassLoader
    ClassLoader baseParent = ClassLoader.getSystemClassLoader().getParent();

    synchronized (mLoaders) {
        if (parent == null) { 
            //[*]goto here
            parent = baseParent;
        }
        
        if (parent == baseParent) { //goto here
            //如果mLoaders列表中已存在该类的类加载器，则返回，
            //否则就调用下面的代码创建一个给该apk创建新的PathClassLoader加载器，
            //并将app的目录名和对应的类加载器添加到mLoaders列表中。
            ClassLoader loader = mLoaders.get(zip);
            if (loader != null) {
                return loader;
            }
            
            //[*]goto here
            Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, zip);
            PathClassLoader pathClassloader =Nnew PathClassLoader(zip, libPath, parent);
            Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);

            mLoaders.put(zip, pathClassloader);
            return pathClassloader;
        }

        Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, zip);
        PathClassLoader pathClassloader = new PathClassLoader(zip, parent);
        Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
        return pathClassloader;
    }
}  



【3.3.5】 创建Application对象
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/Instrumentation.java
public Application Instrumentation::newApplication(
                                   ClassLoader cl, 
                                   String className,
                                   Context context) throws InstantiationException, IllegalAccessException, ClassNotFoundException {
    //c1 是前面获取的类加载器：PathClassLoader 实例
    //此时类不会初始化，即不会 执行静态代码块
    return newApplication(cl.loadClass(className), context);
}
static public Application Instrumentation::newApplication(Class<?> clazz, Context context) throws InstantiationException, IllegalAccessException, ClassNotFoundException {
    //创建Application对象
    //此时类会初始化，即执行静态代码块，然后调用构造函数创建实例
    Application app = (Application)clazz.newInstance();
    app.attach(context);
    return app;
}

 

【3.3.6】启动Activity
//（Service 端）
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/services/java/com/android/server/am/ActivityManagerService.java
private final boolean ActivityManagerService::attachApplicationLocked(IApplicationThread thread, int pid) {
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
    
    //启动 Activity
    mStackSupervisor.attachApplicationLocked(app, mHeadless)
    
    //启动 Service
    didSomething |= mServices.attachApplicationLocked(app, processName);
}

//（Service 端）
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/services/java/com/android/server/am/ActivityStackSupervisor.java
boolean ActivityStackSupervisor::attachApplicationLocked(ProcessRecord app, boolean headless) throws Exception {
    boolean didSomething = false;
    final String processName = app.processName;
    for (int stackNdx = mStacks.size() - 1; stackNdx >= 0; --stackNdx) {
        final ActivityStack stack = mStacks.get(stackNdx);
        if (!isFrontStack(stack)) {
            continue;
        }
        ActivityRecord hr = stack.topRunningActivityLocked(null);
        if (hr != null) {
            if (hr.app == null && app.uid == hr.info.applicationInfo.uid && processName.equals(hr.processName)) {
                try {
                    if (headless) {
                    } 
                    //[*]goto here
                    else if (realStartActivityLocked(hr, app, true, true)) {
                        didSomething = true;
                    }
                } catch (Exception e) {
                    throw e;
                }
            }
        }
    }
    if (!didSomething) {
        ensureActivitiesVisibleLocked(null, 0);
    }
    return didSomething;
}


//（Service 端 -> Client 端）
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/services/java/com/android/server/am/ActivityStackSupervisor.java#realStartActivityLocked
final boolean ActivityStackSupervisor::realStartActivityLocked(
                                      ActivityRecord r,
                                      ProcessRecord app, 
                                      boolean andResume,  //true
                                      boolean checkConfig //true
                                      ) throws RemoteException {

    r.startFreezingScreenLocked(app, 0);
    mWindowManager.setAppVisibility(r.appToken, true);
    
    // schedule launch ticks to collect information about slow apps.
    r.startLaunchTickingLocked();

    if (checkConfig) {
        Configuration config = mWindowManager.updateOrientationFromAppTokens(
                mService.mConfiguration,
                r.mayFreezeScreenLocked(app) ? r.appToken : null);
        mService.updateConfigurationLocked(config, r, false, false);
    }

    r.app = app;
    app.waitingToKill = null;
    r.launchCount++;
    r.lastLaunchTime = SystemClock.uptimeMillis();

    if (localLOGV) Slog.v(TAG, "Launching: " + r);

    int idx = app.activities.indexOf(r);
    if (idx < 0) {
        app.activities.add(r);
    }
    mService.updateLruProcessLocked(app, true, null);
    mService.updateOomAdjLocked();

    final ActivityStack stack = r.task.stack;
    try {
        ......
        app.forceProcessStateUpTo(ActivityManager.PROCESS_STATE_TOP);
        //[*] 通过 Binder 机制进入到 ApplicationThread 类 schedulelaunchActivity 方法
        app.thread.scheduleLaunchActivity(new Intent(r.intent), r.appToken,
                System.identityHashCode(r), r.info,
                new Configuration(mService.mConfiguration), r.compat,
                app.repProcState, r.icicle, results, newIntents, !andResume,
                mService.isNextTransitionForward(), profileFile, profileFd,
                profileAutoStop);

        ......

    } catch (RemoteException e) {
        ......
        throw e;
    }
    ......
    return true;
}


//（回到 Client 端：Binder）
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
public final void ApplicationThread::scheduleLaunchActivity(Intent intent, IBinder token, int ident,
        ActivityInfo info, Configuration curConfig, CompatibilityInfo compatInfo,
        int procState, Bundle state, List<ResultInfo> pendingResults,
        List<Intent> pendingNewIntents, boolean notResumed, boolean isForward,
        String profileName, ParcelFileDescriptor profileFd, boolean autoStopProfiler) {

    updateProcessState(procState, false);

    ActivityClientRecord r = new ActivityClientRecord();

    r.token = token;
    r.ident = ident;
    r.intent = intent;
    r.activityInfo = info;
    r.compatInfo = compatInfo;
    r.state = state;

    r.pendingResults = pendingResults;
    r.pendingIntents = pendingNewIntents;

    r.startsNotResumed = notResumed;
    r.isForward = isForward;

    r.profileFile = profileName;
    r.profileFd = profileFd;
    r.autoStopProfiler = autoStopProfiler;

    updatePendingConfiguration(curConfig);
    
    //调用 ActivityThread::sendMessage(int what, Object obj)
    sendMessage(H.LAUNCH_ACTIVITY, r); 
}

//【与3.3.2 BIND_APPLICATION消息并处理一致】
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
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
private class H extends Handler {
    public static final int BIND_APPLICATION        = 110
    public void handleMessage(Message msg) {
        switch (msg.what) {
            //【*】
            case LAUNCH_ACTIVITY: {
                Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "activityStart");
                ActivityClientRecord r = (ActivityClientRecord)msg.obj;

                r.packageInfo = getPackageInfoNoCheck(r.activityInfo.applicationInfo, r.compatInfo);
                //[*] goto here
                //调用 ActivityThread::handleLaunchActivity
                handleLaunchActivity(r, null);
                Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
            } break;  
            
        }
    }
}

//完成Activity的启动
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java#handleLaunchActivity
private void ActivityThread::handleLaunchActivity(ActivityClientRecord r, Intent customIntent) {
    // If we are getting ready to gc after going to the background, well
    // we are back active so skip it.
    unscheduleGcIdler();

    if (r.profileFd != null) {
        mProfiler.setProfiler(r.profileFile, r.profileFd);
        mProfiler.startProfiling();
        mProfiler.autoStopProfiler = r.autoStopProfiler;
    }

    // Make sure we are running with the most recent config.
    handleConfigurationChanged(null, null);
    
    //[*1]调用 performLaunchActivity() 创建Activity对象并调用 OnCreate() 方法
    Activity a = performLaunchActivity(r, customIntent);

    if (a != null) {
        r.createdConfig = new Configuration(mConfiguration);
        Bundle oldState = r.state;
        
        //[*2]完成对象创建后，调用 handleResumeActivity() 调用 OnResume() 方法
        handleResumeActivity(r.token, false, r.isForward, !r.activity.mFinished && !r.startsNotResumed);

        if (!r.activity.mFinished && r.startsNotResumed) {
            try {
                r.activity.mCalled = false;
                mInstrumentation.callActivityOnPause(r.activity);
                if (r.isPreHoneycomb()) {
                    r.state = oldState;
                }
                if (!r.activity.mCalled) {
                    throw new SuperNotCalledException(
                        "Activity " + r.intent.getComponent().toShortString() +
                        " did not call through to super.onPause()");
                }

            } catch (SuperNotCalledException e) {
                throw e;
            } catch (Exception e) {
            }
            r.paused = true;
        }
    } else {
        try {
            ActivityManagerNative.getDefault().finishActivity(r.token, Activity.RESULT_CANCELED, null);
        } catch (RemoteException ex) {
        }
    }
}

//创建Activity对象，并调用其.OnCreate()方法
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
private Activity ActivityThread::performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
    // System.out.println("##### [" + System.currentTimeMillis() + "] ActivityThread.performLaunchActivity(" + r + ")");

    ActivityInfo aInfo = r.activityInfo;
    if (r.packageInfo == null) {
        r.packageInfo = getPackageInfo(aInfo.applicationInfo, r.compatInfo,
                Context.CONTEXT_INCLUDE_CODE);
    }

    ComponentName component = r.intent.getComponent();
    if (component == null) {
        component = r.intent.resolveActivity(mInitialApplication.getPackageManager());
        r.intent.setComponent(component);
    }

    if (r.activityInfo.targetActivity != null) {
        component = new ComponentName(r.activityInfo.packageName,
                r.activityInfo.targetActivity);
    }

    Activity activity = null;
    try {
        java.lang.ClassLoader cl = r.packageInfo.getClassLoader();
        
        //[*1]创建 Activity 对象！
        //http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/Instrumentation.java
        activity = mInstrumentation.newActivity(cl, component.getClassName(), r.intent){
            //[*]使用 PathClassLoader 的实例去加载 Class！
            return (Activity)cl.loadClass(className).newInstance();
        }
        StrictMode.incrementExpectedActivityCount(activity.getClass());
        r.intent.setExtrasClassLoader(cl);
        if (r.state != null) {
            r.state.setClassLoader(cl);
        }
    } catch (Exception e) {
   
    }

    try {
        Application app = r.packageInfo.makeApplication(false, mInstrumentation);
        if (activity != null) {
            Context appContext = createBaseContextForActivity(r, activity);
            CharSequence title = r.activityInfo.loadLabel(appContext.getPackageManager());
            Configuration config = new Configuration(mCompatConfiguration);
            activity.attach(appContext, this, getInstrumentation(), r.token,
                    r.ident, app, r.intent, r.activityInfo, title, r.parent,
                    r.embeddedID, r.lastNonConfigurationInstances, config);
            if (customIntent != null) {
                activity.mIntent = customIntent;
            }
            r.lastNonConfigurationInstances = null;
            activity.mStartedActivity = false;
            int theme = r.activityInfo.getThemeResource();
            if (theme != 0) {
                activity.setTheme(theme);
            }

            activity.mCalled = false;
            
            //[*2]调用Activity的OnCreate()方法
            //http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/Instrumentation.java
            mInstrumentation.callActivityOnCreate(activity, r.state){
                //http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/Activity.java
                    activity.performCreate(icicle){
                        //[*] 调用onCreate()函数！
                        onCreate(icicle);
                        mVisibleFromClient = !mWindow.getWindowStyle().getBoolean(com.android.internal.R.styleable.Window_windowNoDisplay, false);
                        mFragments.dispatchActivityCreated();                     
                } 
            }
            
            r.activity = activity;
            r.stopped = true;
            if (!r.activity.mFinished) {
                activity.performStart();
                r.stopped = false;
            }
            if (!r.activity.mFinished) {
                if (r.state != null) {
                    mInstrumentation.callActivityOnRestoreInstanceState(activity, r.state);
                }
            }
            if (!r.activity.mFinished) {
                activity.mCalled = false;
                mInstrumentation.callActivityOnPostCreate(activity, r.state);
            }
        }
        r.paused = true;

        mActivities.put(r.token, r);

    } catch (SuperNotCalledException e) {
        throw e;

    } catch (Exception e) {
    }
    return activity;
}
