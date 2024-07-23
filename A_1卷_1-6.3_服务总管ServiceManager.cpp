//【6.3】 服务总管 ServiceManager
//【6.3.1】 ServiceManager的原理 (不需要 BnServiceManager(BBinder)，直接是ServiceManager在处理！)

//http://androidxref.com/2.2.3/xref/frameworks/base/cmds/servicemanager/service_manager.c
//1.ServiceManager的入口函数   p153
int main(int argc, char **argv)
{
    struct binder_state *bs;
    void *svcmgr = BINDER_SERVICE_MANAGER;  //值为NULL, 是一个 magic number

    //1.打开binder设备
    bs = binder_open(128*1024);
    
    //2.成为manager，把自己的handle置为0
    if (binder_become_context_manager(bs)) {
        LOGE("cannot become context manager (%s)\n", strerror(errno));
        return -1;
    }

    svcmgr_handle = svcmgr;
    //3.处理客户端发过来的请求
    binder_loop(bs, svcmgr_handler);  //svcmgr_handler是一个函数
    return 0;
}


//2.打开binder设备  p153
//http://androidxref.com/2.2.3/xref/frameworks/base/cmds/servicemanager/binder.c
struct binder_state *binder_open(unsigned mapsize)
{
    struct binder_state *bs;

    bs = malloc(sizeof(*bs));
    bs->fd = open("/dev/binder", O_RDWR);
    bs->mapsize = mapsize;
    bs->mapped = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, bs->fd, 0);
    return bs;

fail_map:
    close(bs->fd);
fail_open:
    free(bs);
    return 0;
}


//3.成为老大 p154
//http://androidxref.com/2.2.3/xref/frameworks/base/cmds/servicemanager/binder.c
int binder_become_context_manager(struct binder_state *bs)
{
    return ioctl(bs->fd, BINDER_SET_CONTEXT_MGR, 0);
}

//4.死磕Binder  P154
//http://androidxref.com/2.2.3/xref/frameworks/base/cmds/servicemanager/binder.c
void binder_loop(struct binder_state *bs, binder_handler func)
{
    int res;
    struct binder_write_read bwr;
    unsigned readbuf[32];

    bwr.write_size = 0;
    bwr.write_consumed = 0;
    bwr.write_buffer = 0;

    readbuf[0] = BC_ENTER_LOOPER;
    binder_write(bs, readbuf, sizeof(unsigned));
    
    //循环处理！
    for (;;) {
        bwr.read_size = sizeof(readbuf);
        bwr.read_consumed = 0;
        bwr.read_buffer = (unsigned) readbuf;

        res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);

        if (res < 0) {
            LOGE("binder_loop: ioctl failed (%s)\n", strerror(errno));
            break;
        }
        
        //接收到请求，交给 binder_parse，最终会调用 func 来处理这些请求
        res = binder_parse(bs, 0, readbuf, bwr.read_consumed, func);
        if (res == 0) {
            LOGE("binder_loop: unexpected reply?!\n");
            break;
        }
        if (res < 0) {
            LOGE("binder_loop: io error %d %s\n", res, strerror(errno));
            break;
        }
    }
}


//http://androidxref.com/2.2.3/xref/frameworks/base/cmds/servicemanager/binder.c#binder_parse
int binder_parse(struct binder_state *bs, struct binder_io *bio, uint32_t *ptr, uint32_t size, binder_handler func)
{
    int r = 1;
    uint32_t *end = ptr + (size / 4);

    while (ptr < end) {
        uint32_t cmd = *ptr++;
#if TRACE
        fprintf(stderr,"%s:\n", cmd_name(cmd));
#endif
        switch(cmd) {
        case BR_NOOP:
            break;
        case BR_TRANSACTION_COMPLETE:
            break;
        case BR_INCREFS:
        case BR_ACQUIRE:
        case BR_RELEASE:
        case BR_DECREFS:
#if TRACE
            fprintf(stderr,"  %08x %08x\n", ptr[0], ptr[1]);
#endif
            ptr += 2;
            break;
        case BR_TRANSACTION: {
            struct binder_txn *txn = (void *) ptr;
            if ((end - ptr) * sizeof(uint32_t) < sizeof(struct binder_txn)) {
                LOGE("parse: txn too small!\n");
                return -1;
            }
            binder_dump_txn(txn);
            if (func) {
                unsigned rdata[256/4];
                struct binder_io msg;
                struct binder_io reply;
                int res;

                bio_init(&reply, rdata, sizeof(rdata), 4);
                bio_init_from_txn(&msg, txn);
                //func = svcmgr_handler
                res = func(bs, txn, &msg, &reply);
                binder_send_reply(bs, &reply, txn->data, res);   //进行回复
            }
            ptr += sizeof(*txn) / sizeof(uint32_t);
            break;
        }
        case BR_REPLY: {
            struct binder_txn *txn = (void*) ptr;
            if ((end - ptr) * sizeof(uint32_t) < sizeof(struct binder_txn)) {
                LOGE("parse: reply too small!\n");
                return -1;
            }
            binder_dump_txn(txn);
            if (bio) {
                bio_init_from_txn(bio, txn);
                bio = 0;
            } else {
                    /* todo FREE BUFFER */
            }
            ptr += (sizeof(*txn) / sizeof(uint32_t));
            r = 0;
            break;
        }
        case BR_DEAD_BINDER: {
            struct binder_death *death = (void*) *ptr++;
            death->func(bs, death->ptr);
            break;
        }
        case BR_FAILED_REPLY:
            r = -1;
            break;
        case BR_DEAD_REPLY:
            r = -1;
            break;
        default:
            LOGE("parse: OOPS %d\n", cmd);
            return -1;
        }
    }

    return r;
}


//5.集中处理 p154
//void binder_loop(struct binder_state *bs, binder_handler func) 中的 func
//http://androidxref.com/2.2.3/xref/frameworks/base/cmds/servicemanager/service_manager.c
int svcmgr_handler(struct binder_state *bs,
                   struct binder_txn *txn,
                   struct binder_io *msg,
                   struct binder_io *reply)
{
    struct svcinfo *si;
    uint16_t *s;
    unsigned len;
    void *ptr;

//    LOGI("target=%p code=%d pid=%d uid=%d\n",
//         txn->target, txn->code, txn->sender_pid, txn->sender_euid);
    
    //这里要比较target是不是自己
    if (txn->target != svcmgr_handle)   // svcmgr_handle = svcmgr;
        return -1;

    s = bio_get_string16(msg, &len);

    if ((len != (sizeof(svcmgr_id) / 2)) ||
        memcmp(svcmgr_id, s, sizeof(svcmgr_id))) {
        fprintf(stderr,"invalid id %s\n", str8(s));
        return -1;
    }

    switch(txn->code) {
    //得到某个service的信息，service用字符串表示
    case SVC_MGR_GET_SERVICE:            
    case SVC_MGR_CHECK_SERVICE:
        s = bio_get_string16(msg, &len);  //s是字符串表示的service名称
        ptr = do_find_service(bs, s, len);   
        if (!ptr)
            break;
        bio_put_ref(reply, ptr);
        return 0;
        
    //对应的【addService】请求
    case SVC_MGR_ADD_SERVICE:           
        s = bio_get_string16(msg, &len);
        ptr = bio_get_ref(msg);
        if (do_add_service(bs, s, len, ptr, txn->sender_euid))
            return -1;
        break;
    //得到当前系统已经注册的所有 service 的名字
    case SVC_MGR_LIST_SERVICES: {
        unsigned n = bio_get_uint32(msg);

        si = svclist;
        while ((n-- > 0) && si)
            si = si->next;
        if (si) {
            bio_put_string16(reply, si->name);
            return 0;
        }
        return -1;
    }
    default:
        LOGE("unknown code %d\n", txn->code);
        return -1;
    }

    bio_put_uint32(reply, 0);
    return 0;
}



//【6.3.2】服务的注册
//p155 上面提到的switch/case语句，将实现 IServiceManager 中定义的各个业务函数
//比如 do_add_service 完成了对 addService 请求的处理！
//http://androidxref.com/2.2.3/xref/frameworks/base/cmds/servicemanager/service_manager.c
//p156、p157
int do_add_service(struct binder_state *bs,
                   uint16_t *s, 
                   unsigned len,
                   void *ptr, 
                   unsigned uid)
{
    struct svcinfo *si;
//    LOGI("add_service('%s',%p) uid=%d\n", str8(s), ptr, uid);

    if (!ptr || (len == 0) || (len > 127))
        return -1;
    
    // svc_can_register 函数比较注册进程的 uid 和 名字
    if (!svc_can_register(uid, s)) {
        LOGE("add_service('%s',%p) uid=%d - PERMISSION DENIED\n",
             str8(s), ptr, uid);
        return -1;
    }
    
    
    //p157
    si = find_svc(s, len);
    if (si) {
        if (si->ptr) {
            LOGE("add_service('%s',%p) uid=%d - ALREADY REGISTERED\n",
                 str8(s), ptr, uid);
            return -1;
        }
        si->ptr = ptr;
    } else {
        si = malloc(sizeof(*si) + (len + 1) * sizeof(uint16_t));
        if (!si) {
            return -1;
        }
        //ptr是关键数据，可惜为 void *类型。 只有分析驱动的实现才能知道它的真实含义
        si->ptr = ptr;
        si->len = len;
        memcpy(si->name, s, (len + 1) * sizeof(uint16_t));
        si->name[len] = '\0';
        si->death.func = svcinfo_death;   //service 退出的通知函数
        si->death.ptr = si;
        si->next = svclist;  //svclist 是一个 list，保存了当前注册到 ServiceManager 中的信息
        svclist = si;
    }

    binder_acquire(bs, ptr);
    //每当有服务进程退出时，ServiceManager 都会得到来自 binder 设备的通知
    binder_link_to_death(bs, ptr, &si->death);  
    return 0;
}


//http://androidxref.com/2.2.3/xref/frameworks/base/cmds/servicemanager/service_manager.c
//p157
int svc_can_register(unsigned uid, uint16_t *name)
{
    unsigned n;
    //如果用户权限是 root 用户 或 system 用户， 则权限够高，允许注册
    if ((uid == 0) || (uid == AID_SYSTEM))
        return 1;

    for (n = 0; n < sizeof(allowed) / sizeof(allowed[0]); n++)
        // allowed 结构数组控制那些权限达不到 root 和 system 的进程
        //所以如果 Server 进程权限不够 root 和 system，那么请记住要在 allowed 中添加对应的项
        if ((uid == allowed[n].uid) && str16eq(name, allowed[n].name))
            return 1;

    return 0;
}



//【6.3.3】 ServiceManager 存在的意义
