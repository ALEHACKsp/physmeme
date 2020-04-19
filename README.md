# Credits

Before I begin, those who helped me create this project shall be credited.

- Can1357, for helping me find the correct page in physical memory.
- buck, for teaching me everything about paging tables.
- Ch40zz, for helping me fix many issues in things I could never have fixed.
- wlan, I used your drv_image class :)

# Physmeme

Given map/unmap (read/write) of physical memory, one can now systematically map unsigned code into ones kernel.
Many drivers expose this primitive and now can all be exploited by simply coding a few functions.

### What drivers support physical read/write?

Any driver exposing MmMapIoSpace/MmUnmapIoSpace or ZwMapViewOfSection/ZwUnmapViewOfSection can be exploited. This means bios flashing utils, fan speed utils
(like MSI Afterburner), or general windows system utilities that expose physical read/write. 

If you are in any sort of doubt about the abundance of these drivers simply go to 
<a href="https://www.unknowncheats.me/forum/anti-cheat-bypass/334557-vulnerable-driver-megathread.html">this</a> page and ctrl-f "MmMapIoSpace". (24 results)

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

<img src="https://cdn.discordapp.com/attachments/687446832175251502/701355063939039292/unknown.png"/>

This scanning takes under a second since each physical range is scanned with a seperate thread. To increase speeds i also map 2mb at a time and scan each page (512 pages).

# How to use

There are four functions that need to be altered to make this mapper work for you. I will cover each one by one. These functions are defined inside of a `physmeme.hpp` and need
to stay inside of this file. This allows people to make different `physmeme.hpp` files for each driver they want to abuse. Modular code.

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
Unload driver can and should return a bool but its not needed. There is also no need to pass any paremeters since the driver handle is global.

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

This function will `MUST` take two parameters the first is the physical address to be mapped, the second is the size to be mapped. The return
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

# Other

you can change the paremeters you pass to driver entry simply by changing this:

```cpp
using DRIVER_INITIALIZE = NTSTATUS(__stdcall*)(std::uintptr_t, std::size_t);
```

right now your entry point should look like this:

```cpp
NTSTATUS DriverEntry(PVOID lpBaseAddress, DWORD32 dwSize)
```

You can change this as you see fit.

