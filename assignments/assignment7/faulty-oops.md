# faulty-oops

<table>
<tr>
<th>Output</th>
<th>Analysis</th>
</tr>
<tr>
<td style="white-space: nowrap; width:600px">

```
# echo “hello_world” > /dev/faulty
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x0000000096000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000041bc9000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 0000000096000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O) [last unloaded: scull(O)]
CPU: 0 PID: 158 Comm: sh Tainted: G           O       6.1.44 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x10/0x20 [faulty]
lr : vfs_write+0xc4/0x380
sp : ffffffc008debd20
x29: ffffffc008debd20 x28: ffffff8001e13500 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000000000012 x22: 0000000000000012 x21: ffffffc008debdf0
x20: 0000005585b821c0 x19: ffffff8001e74300 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc000777000 x3 : ffffffc008debdf0
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
faulty_write+0x10/0x20 [faulty]
ksys_write+0x70/0x110
__arm64_sys_write+0x1c/0x30
invoke_syscall+0x54/0x130
el0_svc_common.constprop.0+0x44/0xf0
do_el0_svc+0x2c/0xc0
el0_svc+0x2c/0x90
el0t_64_sync_handler+0xf4/0x120
el0t_64_sync+0x18c/0x190
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 0000000000000000 ]---
```

<td>


The command is executed successfully, indicating that /dev/faulty exists

We notice that the Kernel oops because a NULL pointer is dereferenced in the faulty driver, as expected. 

The command executed causes the driver to write to address 0000000000000000 which is NULL.

We can see in the call trace that the program crashed 16 (0x10) bytes into faulty_write function. The faulty_write function is 32 bytes in length (0x20).

The instruction that failed is (b900003f) which after decoding is the instruction: str wzr, [x1] - a command to store to the address saved in register x1. 

The output also lists the PID (158) and the process (sh) that was running during the kernel oops. It also shows information from critical registers confirming the oops. These could be used to identify/debug more difficult kernel oops/drivers in other cases.

We can also see in the output all three of the modules we have installed - hello, faulty, and scull - each of which are loaded successfully and have good licences. 

The output also displays additional information about the system including that it supports Symmetric Multi-Processing and dumps the Processor State (pstate) register.  


</td>
</tr>
</table>

