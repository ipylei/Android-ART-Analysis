//[thread.cc->Thread::Startup]
void Thread::Startup() {
    is_started_ = true; {
        MutexLock mu(nullptr,  * Locks::thread_suspend_count_lock_);
        //创建一个条件变量。这部分内容属于POSIX多线程方面的知识，本书不拟讨论它们
        resume_cond_ = new ConditionVariable("Thread resumption condition variable", * Locks::thread_suspend_count_lock_);
    }

    /*CHECK_PTHREAD_CALL是一个宏，它用来检查pthread相关函数调用的返回值。
    如果调用返回失败，则打印一些警告信息。读者只需要关注这个宏所调用的函数即可。
    （1）Thread::pthread_key_self_：是Thread类的静态成员变量，代表所创建TLS区域的key。
    通过这个key可获取存取该TLS区域。比如，
		调用pthread_set_specific函数往该区域存入数据，
		调用pthread_get_specific函数读取该数据等。

    （2）当线程退出时，这块区域需要被回收，调用者可以指定一个特殊的回收函数，如下面的 ThreadExitCallback   
	*/
    CHECK_PTHREAD_CALL(pthread_key_create, ( & Thread::pthread_key_self_, Thread::ThreadExitCallback), "self key");
	
    //检查pthread_Key_self_对应的TLS区域里是否有数据。如果是第一次创建，该区域是不能有数据的。
    if (pthread_getspecific(pthread_key_self_) != nullptr) {
        LOG(FATAL) << "Newly-created pthread TLS slot is not nullptr";
    }
}

