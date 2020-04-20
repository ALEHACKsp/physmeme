#pragma once
#include <windows.h>
#include <mutex>
#include <cstdint>
#include <map>

namespace physmeme
{
	//--- ranges of physical memory
	static std::map<std::uintptr_t, std::size_t> pmem_ranges;

	//--- validates the address
	static bool is_valid(std::uintptr_t addr)
	{
		for (auto range : pmem_ranges)
			if (addr >= range.first && addr <= range.first + range.second)
				return true;
		return false;
	}

	// Author: Remy Lebeau
	// taken from here: https://stackoverflow.com/questions/48485364/read-reg-resource-list-memory-values-incorrect-value
	static const auto init_ranges = ([&]() -> bool
	{
			HKEY h_key;
			DWORD type, size;
			LPBYTE data;
			RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\RESOURCEMAP\\System Resources\\Physical Memory", 0, KEY_READ, &h_key);
			RegQueryValueEx(h_key, ".Translated", NULL, &type, NULL, &size); //get size
			data = new BYTE[size];
			RegQueryValueEx(h_key, ".Translated", NULL, &type, data, &size);
			DWORD count = *(DWORD*)(data + 16);
			auto pmi = data + 24;
			for (int dwIndex = 0; dwIndex < count; dwIndex++)
			{
				pmem_ranges.emplace(*(uint64_t*)(pmi + 0), *(uint64_t*)(pmi + 8));
				pmi += 20;
			}
			delete[] data;
			RegCloseKey(h_key);
			return true;
	})();

	/*
		please code this function depending on your method of physical read/write.
	*/
	static std::uintptr_t map_phys(
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
	static bool unmap_phys(
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
	static HANDLE load_drv()
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
	static bool unload_drv()
	{
		static const auto unload_driver_ptr =
			reinterpret_cast<__int64(*)()>(
				GetProcAddress(LoadLibrary("pmdll64.dll"), "UnloadPhyMemDriver"));
		return unload_driver_ptr ? unload_driver_ptr() : false;
	}

	inline HANDLE drv_handle = load_drv();
}