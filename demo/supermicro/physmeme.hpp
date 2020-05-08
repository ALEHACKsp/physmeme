#pragma once
#include <windows.h>
#include <mutex>
#include <cstdint>
#include <map>

namespace physmeme
{
	/*
		please code this function depending on your method of physical read/write.
	*/
	inline std::uintptr_t map_phys(
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

	/*
		please code this function depending on your method of physical read/write.
	*/
	inline bool unmap_phys(
		std::uintptr_t addr,
		std::size_t size
	)
	{
		static const auto unmap_phys_ptr =
			reinterpret_cast<__int64(*)(__int64, unsigned)>(
				GetProcAddress(LoadLibrary("pmdll64.dll"), "UnmapPhyMem"));
		return unmap_phys_ptr ? unmap_phys_ptr(addr, size) : false;
	}

	/*
		please code this function depending on your method of physical read/write.
	*/
	inline HANDLE load_drv()
	{
		static const auto load_driver_ptr =
			reinterpret_cast<__int64(*)()>(
				GetProcAddress(LoadLibrary("pmdll64.dll"), "LoadPhyMemDriver"));

		if (load_driver_ptr)
			load_driver_ptr();

		//--- i dont ever use this handle, its just an example of what you should do.
		return CreateFileA("\\\\.\\PhyMem", 0xC0000000, 3u, 0i64, 3u, 0x80u, 0i64);
	}

	/*
		please code this function depending on your method of physical read/write.
	*/
	inline bool unload_drv()
	{
		static const auto unload_driver_ptr =
			reinterpret_cast<__int64(*)()>(
				GetProcAddress(LoadLibrary("pmdll64.dll"), "UnloadPhyMemDriver"));
		return unload_driver_ptr ? unload_driver_ptr() : false;
	}

	inline HANDLE drv_handle = load_drv();
}