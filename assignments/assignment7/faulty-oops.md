# echo “hello_world” > /dev/faulty
[**Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000**](#line-breakdown-1) <sub>Line 1<sub> <br />



Mem abort info: <br />
  ESR = 0x96000045 <br />
  EC = 0x25: DABT (current EL), IL = 32 bits <br />
  SET = 0, FnV = 0 <br />
  EA = 0, S1PTW = 0 <br />
  FSC = 0x05: level 1 translation fault <br />
Data abort info: <br />
  ISV = 0, ISS = 0x00000045 <br />
  CM = 0, WnR = 1 <br />
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000042105000 <br />
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000 <br />


[**Internal error: Oops: 96000045 [#1] SMP**](#line-breakdown-2) <sub> Line 2 <sub> <br /> 


Modules linked in: hello(O) faulty(O) scull(O)<br />


[CPU: 0 PID: 159 Comm: sh Tainted: G           O      5.15.18 #1](#line-breakdown-3) <sub> Line 3 <sub><br />


Hardware name: linux,dummy-virt (DT) <br />
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--) <br />


[pc : faulty_write+0x14/0x20 [faulty] ](#memory-breakdown) <br />
lr : vfs_write+0xa8/0x2b0<br />
sp : ffffffc008d23d80<br />
x29: ffffffc008d23d80 x28: ffffff80020e0cc0 x27: 0000000000000000<br>
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000<br>
x23: 0000000040001000 x22: 0000000000000012 x21: 000000557c9e2670<br>
x20: 000000557c9e2670 x19: ffffff8002078200 x18: 0000000000000000<br>
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000<br>
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000<br>
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000<br>
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000<br>
x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008d23df0<br>
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000<br>


Call trace:<br>
 [**faulty_write**+***0x14***/0x20 ***[faulty]***](#trace-breakdown)<br>
 ksys_write+0x68/0x100 <br>
 __arm64_sys_write+0x20/0x30 <br>
 invoke_syscall+0x54/0x130 <br>
 el0_svc_common.constprop.0+0x44/0xf0 <br>
 do_el0_svc+0x40/0xa0 <br>
 el0_svc+0x20/0x60 <br>
 el0t_64_sync_handler+0xe8/0xf0 <br>
 el0t_64_sync+0x1a0/0x1a4 <br>
Code: d2800001 d2800000 d503233f d50323bf (b900003f)  <br>
---[ end trace cb71849e2320ec34 ]--- <br>


## My analysis

### Line breakdown 1: 
This is a segmentation fault from trying to derefence a NULL pointer simply put this kills all of the current tasks.

### Line breakdown 2: 
This identifies that it is indeed a kernel oops!

### Line breakdown 3: 
This identifies the type of process it was this one being a proprietary module that was loaded as indicated by > Tainted: G > and it shows the running process ID and finally the kernel version.

### Memory breakdown: 
This part of the output tell us all of the register information at the time of the oops / stack data. So you can see it loaded and was running the faulty_write function that would return to vfs_write if it ever returned.

### Trace breakdown: 
This part of the output identified the function that crashed in this case ***faulty_write*** the input size and the module name so you can go to that module and debug the exact cause of the issue! The lines that follow also show the whole call stack up until that final function call!




## References:
https://www.cs.fsu.edu/~cop4610t/lectures/project2/debugging/kernel_debugging.pdf
https://docs.kernel.org/admin-guide/tainted-kernels.html
