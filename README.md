# Credits

Before I begin, those who helped me create this project shall be credited.

- Can1357, for helping me find the correct page in physical memory.
- buck, for teaching me everything about paging tables.
- Ch40zz, for helping me fix many issues for things I could never have fixed.
- IChooseYou, for his work with physical memory.
- Heep042, for his work with physical memory and paging tables.
- wlan, I used your drv_image class :) 

# Physmeme

Given map/unmap (read/write) of physical memory, one can now systematically map unsigned code into ones kernel.
Many drivers expose this primitive and now can all be exploited by simply coding a few functions.

### What drivers support physical read/write?

Any driver exposing MmMapIoSpace/MmUnmapIoSpace or ZwMapViewOfSection/ZwUnmapViewOfSection can be exploited. This means bios flashing utils, fan speed utils
(like MSI Afterburner), or general windows system utilities that expose physical read/write. 

Ff you are in any sort of doubt about the abundance of these drivers simply go to 
<a href="https://www.unknowncheats.me/forum/anti-cheat-bypass/334557-vulnerable-driver-megathread.html">this</a> page and ctrl-f "MmMapIoSpace".

### How does this exploit work?

First lets start with a given, controlled writes can be leveraged to gain execution. I think people call this "write what where", but nevertheless if you
know where you are writing you can leverage it to gain execution in places that might not have been accessable proir. Now that we have that agreed upon, lets get into the details of how this works.

To start, lets first understand that one page of memory reguardless of physical or virtual is typically 0x1000 bytes or 4096 bytes. Now, given a relative virtual address of a syscall
(an address relative to the base of the module) we can modulus the address with the size of a page (0x1000) and get the index into the page. 

```
auto nt_syscall_offset = rva % 0x1000;
```

This index, combined with the iteraction of each physical page and a comparison of bytes will result in us finding the physical page of a syscall (and its mapped into our process).
This then allows us the ability to install hooks, call the syscall, and then uninstall the hook. The "hook" being `ExAllocatePool`, `ExAllocatePoolWithTag`, and `MmCopyMemory`.
