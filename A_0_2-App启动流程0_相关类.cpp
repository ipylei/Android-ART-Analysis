ActivityThread
Application
Instrumentation
LoadedApk
ApplicationLoaders


//【3.2】 几个关键类的关系和用途
//【3.2.1】 ActivityThread 类
(1) 用途：
    一个App启动时会创建一个ActivityThread，它管理着app进程中主线程的执行，其main方法作为app启动的入口。
    它根据Activity Manager发送的请求，对activities、broadcasts和其他操作进行调度、执行。

(2) 关键成员和方法
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ActivityThread.java
public final class ActivityThread {
    //关键成员
    //作为一个Binder，用于通信 ActivityManagerService
    final ApplicationThread mAppThread = new ApplicationThread();
    
    //H对象接收并处理各种消息，包括启动、暂停、关闭各个组件等操作
    //用switch/case， 如： case LAUNCH_ACTIVITY： case BIND_APPLICATION:
    final H mH = new H();
    
    
    
    /*在 ActivityThread::handleBindApplication() 中初始化
        Application app = data.info.makeApplication(data.restrictedBackupMode, null);
        mInitialApplication = app;
    */
    Application mInitialApplication;
    
    //在 ActivityThread::attach() 中初始化，直接为自己的实例
    private static ActivityThread sCurrentActivityThread;
    
    //在 ActivityThread::handleBindApplication() 中初始化
    //mInstrumentation = new Instrumentation();
    Instrumentation mInstrumentation;
    
    //在 ActivityThread::getPackageInfo() 中初始化
    //mPackages.put(aInfo.packageName, new WeakReference<LoadedApk>(packageInfo));
    final ArrayMap<String, WeakReference<LoadedApk>> mPackages = new ArrayMap<String, WeakReference<LoadedApk>>();
    
    
    //关键方法
    public static void main(String[] args) {...}
    
    private void attach(boolean system){...}
    
    public final void bindApplication(String processName,
                ApplicationInfo appInfo, List<ProviderInfo> providers,
                ComponentName instrumentationName, String profileFile,
                ParcelFileDescriptor profileFd, boolean autoStopProfiler,
                Bundle instrumentationArgs, IInstrumentationWatcher instrumentationWatcher,
                IUiAutomationConnection instrumentationUiConnection, int debugMode,
                boolean enableOpenGlTrace, boolean isRestrictedBackupMode, boolean persistent,
                Configuration config, CompatibilityInfo compatInfo, Map<String, IBinder> services,
                Bundle coreSettings) {}
                
    private void handleBindApplication(AppBindData data) {...}
                    
}


//【3.2.2】 Application 类
(1) 用途：
    通过ActivityThread类的定义我们知道，ActivityThread是单例模式，而且是应用程序的主线程。
    其中，Application 对象 mInitialApplication 是该类的成员。
    可见，应用程序中有且仅有一个 Application 组件，它是全局的单例的。
    而且Application对象的生命周期跟应用程序的生命周期一样长，从应用启动开始到应用退出结束。

    Application跟 Activity、Service一样，是Android系统的一个组件，
    当Android程序启动时会创建一个Application对象，用来存储系统的一些信息。
    默认情况下，系统会帮我们自动创建一个Application对象，
    我们也可以在AndroidManifest.xml中指定并创建自己的Application做一些全局初始化的工作。

(2) 关键成员和方法
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/Application.java
public class Application extends ContextWrapper implements ComponentCallbacks2 {
    //关键成员
    //在attach()方法中初始化：mLoadedApk = ContextImpl.getImpl(context).mPackageInfo;
    public LoadedApk mLoadedApk;
    
    //关键方法
    public void onCreate(){}
    
    final void attach(Context context) {
        attachBaseContext(context);
        mLoadedApk = ContextImpl.getImpl(context).mPackageInfo;
    }
    
    //继承而来的方法
    //http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/content/ContextWrapper.java#67
    protected void attachBaseContext(Context base) {
        if (mBase != null) {
            throw new IllegalStateException("Base context already set");
        }
        mBase = base;
    }
}


//【3.2.3】 Instrumentation 类
(1) 用途:
    同样，通过上面ActivityThread类的源码，我们知道在ActivityThread静态单例模式类中，
    有一个Instrumentation实例成员mInstrumentation。可见，Instrumentation同样也是全局的单例的。
    Instrumentation 主要是用来监控系统和应用的交互，
    并为 ActivityThread 创建 Application、Activity等组件。

(2) 关键成员和方法
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/Instrumentation.java
public class Instrumentation {
    //关键成员
    private ActivityThread mThread = null;
    private Context mInstrContext;
    private Context mAppContext;
    
    //关键方法
    public void callApplicationOnCreate(Application app) {
        app.onCreate();
    }
    
    public Application newApplication(ClassLoader cl, String className, Context context) throws InstantiationException, IllegalAccessException, ClassNotFoundException {
        return newApplication(cl.loadClass(className), context);
    }
    
    static public Application newApplication(Class<?> clazz, Context context) throws InstantiationException, IllegalAccessException, ClassNotFoundException {
        Application app = (Application)clazz.newInstance();
        app.attach(context);
        return app;
    }
}

//【3.2.4】 LoadedApk 类
(1)用途：
    一个应用程序对应一个 LoadedApk 对象。它用来保存当前加载的APK包的各种信息，
    包括app安装路径、资源路径、用户数据保存路径、使用的类加载器、Application信息等。

(2) 关键成员和方法
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/LoadedApk.java
public final class LoadedApk {
    //关键成员
    
    /* ActivityThread::handleBindApplication 
        -> ActivityThread::getPackageInfoNoCheck
          -> packageInfo = new LoadedApk(this,      //倒反天罡第1步：this为ActivityThread
                                        aInfo, 
                                        compatInfo, 
                                        baseLoader, //null
                                        securityViolation, 
                                        includeCode && (aInfo.flags&ApplicationInfo.FLAG_HAS_CODE) != 0);
    */
    
    private final ActivityThread mActivityThread;  //倒反天罡？直接就是 ActivityThread对象
    private final ClassLoader mBaseClassLoader;    //null
    
    /* ||||
    LoadedApk::getClassLoader(){
        mClassLoader = ApplicationLoaders.getDefault().getClassLoader(zip, libraryPath, mBaseClassLoader){
            PathClassLoader pathClassloader =Nnew PathClassLoader(zip, libPath, parent);
            return pathClassloader;
        }
    }
    */
    private ClassLoader mClassLoader;   //所以为一个 PathClassloader的实例
    
    //构造函数中初始化：mApplicationInfo 
    private ApplicationInfo mApplicationInfo;
    
    //public Application makeApplication(...){mApplication = app;}
    private Application mApplication;   //倒反天罡？与 ActivityThread的属性.mInitialApplication是同一个
    
    
    
    //关键方法   
    public Application makeApplication(boolean forceDefaultAppClass,  Instrumentation instrumentation) {
        ...
    }
    public ClassLoader getClassLoader() {
        if (mClassLoader != null) {
            return mClassLoader;
        }
        ...
        return mClassLoader;
    }
}


//【3.2.5】 ApplicationLoaders 类
(1)用途：
    获取当前应用程序的类加载器，通过 LoadedApk 类的 getClassLoader 方法来调用。
    //LoadedApk::getClassLoader()
    java.lang.ClassLoader cl = getClassLoader();
        //ApplicationLoaders::getClassLoader()
        -> ApplicationLoaders.getDefault().getClassLoader(zip, libraryPath, mBaseClassLoader);

(2) 关键成员和方法
//http://androidxref.com/4.4.3_r1.1/xref/frameworks/base/core/java/android/app/ApplicationLoaders.java
class ApplicationLoaders{
    //关键成员
    private final ArrayMap<String, ClassLoader> mLoaders = new ArrayMap<String, ClassLoader>();
    private static final ApplicationLoaders gApplicationLoaders = new ApplicationLoaders();
    
    //关键方法
    public static ApplicationLoaders getDefault(){
        return gApplicationLoaders;
    }
    
    public ClassLoader getClassLoader(String zip, String libPath, ClassLoader parent){
        ClassLoader baseParent = ClassLoader.getSystemClassLoader().getParent();

        synchronized (mLoaders) {
            if (parent == null) {
                parent = baseParent;
            }
            
            if (parent == baseParent) {
                ClassLoader loader = mLoaders.get(zip);
                if (loader != null) {
                    return loader;
                }

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
}



    
    