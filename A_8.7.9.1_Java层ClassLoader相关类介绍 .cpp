//8.7.9.1.1
//ClassLoader的委托关系体现在它的loadClass函数中，来看代码。
//[ClassLoader.java->ClassLoader::loadClass]
//这个loadCLass是public的，外界可访问。
public Class<?> loadClass(String name) throws ClassNotFoundException {
	return loadClass(name, false); //这个loadClass是protected，外界不能访问
}

//ClassLoader.java
protected Class<?> loadClass(String name, boolean resolve)... {
    //先看虚拟机是否已经加载目标类了
    Class c = findLoadedClass(name);
	
    if (c == null) { //如果没有，则尝试加载
        long t0 = System.nanoTime();
        try { //如果parent不为空，则委托它去加载。
            if (parent != null) { 
				c = parent.loadClass(name, false);
			}
            else { 
				c = findBootstrapClassOrNull(name);
			}
        }
        catch (ClassNotFoundException e) {  
			.....
        }
        //如果委托者加载不成功，则尝试自己加载
        if (c == null) { 
			c = findClass(name);
		}
    }
    return c;
}








//8.7.9.1.2
//[ClassLoader.java->BootClassLoader]
class BootClassLoader extends ClassLoader {
    //BootClassLoader采用了单例构造的方式。所以一个Java进程只存在一个BootClassLoader对象
    private static BootClassLoader instance;
    public static synchronized BootClassLoader getInstance() {
        if (instance == null) {  
			instance = new BootClassLoader(); 
		}
        return instance;
    }
	
    //调用父类的构造函数，其参数用于设置parent成员。注意，此处传的是null，这说明BootClassLoader没有可委托的其他加载器了。
    public BootClassLoader() { 
		super(null); 
	}
	
	@Override
    protected Class<?> loadClass(String className, boolean resolve) throws ClassNotFoundException {
        Class<?> clazz = findLoadedClass(className);

        if (clazz == null) {
            clazz = findClass(className);
        }

        return clazz;
    }


    //加载目标类，下文将分析其代码。
	protected Class<?> findClass(String name) throws ClassNotFoundException {
        return Class.classForName(name, false, null);
    }
}


//[java_lang_Class.cc->Class_classForName]
static jclass Class_classForName(JNIEnv* env, jclass, 
							    jstring javaName, 
								jboolean initialize,  //false
								jobject javaLoader) { //null
    //注意传入的参数，javaName 为JLS规范里定义的类名（如java.lang.String），initialize为false，javaLoader为null
    ScopedFastNativeObjectAccess soa(env);
    ScopedUtfChars name(env, javaName);
    
	......
   
   //转成JVM规范使用的类名，如Ljava/lang/String;
    std::string descriptor(DotToDescriptor(name.c_str()));
    StackHandleScope<2> hs(soa.Self());
	
    //由于javaLoader为空，所以 class_loader 等同于nullptr
    Handle<mirror::ClassLoader> class_loader(hs.NewHandle(soa.Decode<mirror::ClassLoader*>(javaLoader)));
	
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
	
    /*调用ClassLinker 的 FindClass，由于class_loader 等于nullptr，
	根据8.7节对 FindClass 的介绍可知，它将只在ClassLinker 的 boot_class_table_ 中搜索目标类 
	*/
    Handle<mirror::Class> c(hs.NewHandle(class_linker->FindClass(soa.Self(),descriptor.c_str(), class_loader)));
	......
	
    if (initialize) {      //initialize为false的话，将只加载和链接目标类，不初始化它
        class_linker->EnsureInitialized(soa.Self(), c, true, true);
    }
    return soa.AddLocalReference<jclass>(c.Get());
}











//8.7.9.1.3　BaseDexClassLoader 介绍
//[BaseDexClassLoader.java]
public class BaseDexClassLoader extends ClassLoader {
    //关键成员pathList，类型为DexPathList。该变量存储了路径信息
    private final DexPathList pathList;
    
	//findClass的代码：
    protected Class<?> findClass(String name) ... {
        List<Throwable> suppressedExceptions = new ArrayList<Throwable>();
        //由DexPathList findClass来完成相关工作。下文会介绍该函数。
        Class c = pathList.findClass(name, suppressedExceptions);
        
		.......
		
        return c;
    }
}

//[DexPathList.java::DexPathList]
final class DexPathList {
    private static final String DEX_SUFFIX = ".dex";
    private static final String zipSeparator = "!/";
	
    //definingContext指向包含本DexPathList对象的加载器对象
    private final ClassLoader definingContext;
	
    //dexElements：描述defingContext ClassLoader 所加载的 dex文件
    private Element[] dexElements;

	private final Element[] nativeLibraryPathElements;
	
    //ClassLoader除了加载类之外，加载native动态库的工作也可由它来完成。下面这个变量描述了 definingContext ClassLoader可从哪几个目录中搜索目标动态库
    private final List<File> nativeLibraryDirectories;
    
	......
	
    //这是Element类的定义，读者不用关心其成员的含义
    package static class Element {
        private final File dir;
        private final boolean isDirectory;
        private final File zip;
        private final DexFile dexFile;
        ......
    }
}
/*
通过上述代码可知：
	·一个BaseDexClassLoader对象和一个DexPathList对象互相关联。
	·DexPathList可描述一个或多个dex文件信息。这表明BaseDexClassLoader应该从DexPathList指定的dex文件中去搜索和加载目标类。
	·DexPathList同时包含native动态库搜索目录列表。
*/




//[DexPathList.java->DexPathList::findClass]
public Class findClass(String name, List<Throwable> suppressed) {
    for (Element element : dexElements) {
        DexFile dex = element.dexFile;
        //遍历所关联的dex文件，然后调用DexFile的loadClassBinaryName函数，定义加载器由definigContext指定。
        if (dex != null) {
            Class clazz = dex.loadClassBinaryName(name, definingContext, suppressed);
            if (clazz != null) { 
				return clazz;  
			}
        }
    }

	......
    return null;
}


//·bootstrap类由【Bootstrap加载器】加载。
//·system_server和APP进程的内容由【APP加载器】加载。注意，system_server其实也是一个应用程序，只不过是一个核心应用程序罢了。
//那么，三种加载器里只剩【System加载器】没有介绍了。其实，本章早在8.6节介绍Create-SystemClassLoader函数时就已经提到过这个System ClassLoader了。我们先回顾它的Java层代码，非常简单。
//[ClassLoader.java->ClassLoader::createSystemClassLoader]
private static ClassLoader createSystemClassLoader() {
    //读取系统属性"java.class.path"的值，默认取值为"."。
    String classPath = System.getProperty("java.class.path", ".");
    String librarySearchPath = System.getProperty("java.library.path", "");
    //SystemClassLoader的数据类型是PathClassLoader，并且使用BootClassLoader
    //作为它的委托对象。
    return new PathClassLoader(classPath, librarySearchPath, BootClassLoader.getInstance());
}





//这里简单看一下 system_server 是如何创建APP ClassLoader的，代码如下所示。
//[ZygoteInit.java->ZygoteInit::handleSystemServerProcess]
private static void handleSystemServerProcess(ZygoteConnection.Arguments parsedArgs) ...... {

    closeServerSocket();
    //获取SYSTEMSERVERCLASSPATH属性值
    final String systemServerClasspath = Os.getenv("SYSTEMSERVERCLASSPATH");

    if (parsedArgs.invokeWith != null) { 
		......
	}
    else {
        ClassLoader cl = null;
        if (systemServerClasspath != null) {
			//创建APP ClassLoader
            cl = createSystemServerClassLoader(systemServerClasspath, parsedArgs.targetSdkVersion);
            Thread.currentThread().setContextClassLoader(cl);
            ......
            RuntimeInit.zygoteInit(parsedArgs.targetSdkVersion, parsedArgs.remainingArgs, cl);
		}
    }
}




//而createSystemServerClassLoader的代码如下所示。
//[ZygoteInit.java->ZygoteInit::createSystemServerClassLoader]
private static PathClassLoader createSystemServerClassLoader(String systemServerClasspath, int targetSdkVersion) {
    String librarySearchPath = System.getProperty("java.library.path");
    //创建APP加载器，委托对象是System加载器
    return PathClassLoaderFactory.createClassLoader(systemServerClasspath, librarySearchPath, null, ClassLoader.getSystemClassLoader(), targetSdkVersion, true);
}