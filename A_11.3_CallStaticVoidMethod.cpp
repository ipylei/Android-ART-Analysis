//[jni_internal.cc->CallStaticObjectMethod]
static jobject CallStaticObjectMethod(JNIEnv* env, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    
    ScopedObjectAccess soa(env);
    
    //先调用InvokeWithVarArgs，返回值存储在result中
    JValue result(InvokeWithVarArgs(soa, nullptr, mid, ap));
    jobject local_result = soa.AddLocalReference<jobject>(result.GetL());
    
    va_end(ap);
    return local_result;
}



//[reflection.cc->InvokeWithVarArgs]
JValue InvokeWithVarArgs(const ScopedObjectAccessAlreadyRunnable& soa,
                        jobject obj, 
                        jmethodID mid,  
                        va_list args)  {
    
    .....
    
    ArtMethod* method = soa.DecodeMethod(mid);
    bool is_string_init = .....;
    if (is_string_init) {......}
    mirror::Object* receiver = method->IsStatic() ? nullptr : soa.Decode<mirror::Object*>(obj);
    
    uint32_t shorty_len = 0;
    const char* shorty = method->GetInterfaceMethodIfProxy(sizeof(void*))->GetShorty(&shorty_len);
    
    JValue result;
    ArgArray arg_array(shorty, shorty_len);
    arg_array.BuildArgArrayFromVarArgs(soa, receiver, args);
    
    //调用InvokeWithArgArray
    InvokeWithArgArray(soa, method, &arg_array, &result, shorty);
    
    .....
    return result;
}


//[reflection.cc->InvokeWithArgArray]
static void InvokeWithArgArray(const ScopedObjectAccessAlreadyRunnable& soa,
                               ArtMethod* method,
                               ArgArray* arg_array, 
                               JValue* result,
                               const char* shorty) {
                                   
    uint32_t* args = arg_array->GetArray();
    
    ......
    
    //调用ArtMethod的Invoke函数
    method->Invoke(soa.Self(), args, arg_array->GetNumBytes(), result, shorty);
}