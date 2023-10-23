# echo “hello_world” > /dev/faulty
**Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000**[^1]
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000042105000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
**Internal error: Oops: 96000045 [#1] SMP**[^2]
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 159 Comm: sh Tainted: G           O      5.15.18 #1 [^3]
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty] [^4]
lr : vfs_write+0xa8/0x2b0
sp : ffffffc008d23d80
x29: ffffffc008d23d80 x28: ffffff80020e0cc0 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 0000000000000012 x21: 000000557c9e2670
x20: 000000557c9e2670 x19: ffffff8002078200 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008d23df0
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 **faulty_write**+***0x14***/0x20 ***[faulty]*** [^5]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xe8/0xf0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace cb71849e2320ec34 ]---


#My analysis
Line breakdown 1 [^1]: This is a segmentation fault from trying to derefence a NULL pointer simply put this kills all of the current tasks.

Line breakdown 2 [^2]: This identifies that it is indeed a kernel oops!

Line breakdown 3 [^3]: This identifies the type of process it was this one being a proprietary module that was loaded as indicated by > Tainted: G > and it shows the running process ID and finally the kernel version.

Memory breakdown [^4]: This part of the output tell us all of the register information at the time of the oops / stack data. So you can see it loaded and was running the faulty_write function that would return to vfs_write if it ever returned.

Trace breakdown  [^5]: This part of the output identified the function that crashed in this case ***faulty_write*** the input size and the module name so you can go to that module and debug the exact cause of the issue! The lines that follow also show the whole call stack up until that final function call!








###References:
https://www.cs.fsu.edu/~cop4610t/lectures/project2/debugging/kernel_debugging.pdf
https://docs.kernel.org/admin-guide/tainted-kernels.html
