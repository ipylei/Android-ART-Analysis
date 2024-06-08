/*
请读者特别注意，正如Java语言的特性一样，数组中的元素更像是一个指针，而不是将对应内容直接存储在数组中。
所以，上述的image_roots_、 image_methods_ 等都是指针，它们所指向的内容存储在Object Section和RuntimeMethod Section中。

【9.6.2.3】  oat文件和art文件的关系

art和oat文件的关系：
    .art文件里的ArtMethod对象的成员变量 ptr_sized_fields_结构体 的 entry_point_from_quick_compiled_code_ 指向位于oat文件里对应的code_数组。


通过本节对art文件格式构成以及它和oat文件关系的介绍可知：
    ·简单来说，可以将art文件看作是很多对象通过类似序列化的方法保存到文件里而得来的。
    当art文件通过mmap加载到内存时，这些文件里的信息就能转换成对象直接使用。

    ·art文件里保存的对象有几类，包括mirror Object及派生类的实例，比如Class、String、enum、DexCache等。
    除此之外还有ArtMethod、ArtField、ImtConflictTable等对象。

    ·如果在dex2oat时不生成art文件的话，那么上述这些对象只能等到程序运行时才创建，如此将耗费一定的运行时间。
    考虑到boot包含的内容非常多（13个jar包，14个dex文件），所以在Android 7.0中，boot镜像必须生成art文件。
    而对app来说，默认只生成oat文件。其art文件会根据profile的情况由系统的后台服务择机生成。这样能减少安装的时间，提升用户体验。
*/