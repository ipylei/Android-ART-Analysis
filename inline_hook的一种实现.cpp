//功能函数(纯汇编实现)
myreplace(){
    //寄存器保存
    --------------
    offset: 0x44 跳转到hook方法，即register_hook的参数2
    
    /*思考：如何修改返回值？
    一种不太优雅的方法：(因为x30寄存器是变化的！)
        获取到x30寄存器？取出其中的值，然后按同样的逻辑再来一遍替换指令或者hook？   
        此时x0寄存器就是返回值，可以对其进行检查、修改，然后继续跳转到原逻辑继续执行([x30]+0x10)
    */
    --------------
    //寄存器恢复
    //offset:0x9c 等价执行被替换的4条指令(参考课时18)
    asm("mov x0,x0");
    asm("mov x0,x0");
    asm("mov x0,x0");
    asm("mov x0,x0");

    // 执行完hook逻辑后，跳转到原方法剩余逻辑中去
    asm("ldr x16,8");
    asm("br x16");
    // offset:0x104
    asm("mov x0,x0");   //被替换的4条指令后面的一条指令地址(即原第5条指令)
    asm("mov x0,x0");
}





register_hook(target_func, func_replace){
    //1.指令替换，跳转到 myreplace 
    *(int *)((char *)target_func) = 0x58000050;     // ldr x16,8
    *(int *)((char *)target_func + 4) = 0xd61f0200; // br x16;
    *(long *)((char *)target_func + 8) = myreplace;

    //2.指令替换=>中转，从 myreplace 跳转到 func_replace
    int n = 0x44;
    *(myreplace + n = 0x58000070;     // ldr x16, #0xc   => 指向address地址
    *(myreplace + n + 4) = 0xd63f0200;// blr x16;        => 执行完hook逻辑后，还得继续向下执行原函数逻辑，所以执行下一句
    *(myreplace+ n + 8) = 0x14000003; // b 12;  地址占2字节/8位, 所以为b 12, 即跳过下一行，即继续向后执行
    *(myreplace+ n + 12) = reinterpret_cast<long>(func_replace);
    
    //3.指令恢复(1)， 补上缺失(被替换了)的4条指令【即对被替换的4条指令进行等价执行】
    *(int *) ((char *) myreplace + 0x9c) = old_code1;
    *(int *) ((char *) myreplace + 0xa0) = old_code2;
    *(int *) ((char *) myreplace + 0xa4) = old_code3;
    *(int *) ((char *) myreplace + 0xa8) = old_code4;
    
    //4.指令恢复(2)，从remyplace跳转到被hook函数的剩余逻辑
    retaddress = reinterpret_cast<long>((char *)target_func + 0x10);  //(0x0  0x4  0x8  0xc)  0x10
    *(long *)((char *)myreplace + 0x104) = retaddress;
}