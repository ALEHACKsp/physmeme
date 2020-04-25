#pragma once
#include <windows.h>
#include <mutex>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>

#pragma pack ( push, 1 )
typedef struct _GIOMAP
{
	unsigned long	interface_type;
	unsigned long	bus;
	uintptr_t		physical_address;
	unsigned long	io_space;
	unsigned long	size;
} GIOMAP;
#pragma pack ( pop )

static const char* driver_name = "gdrv";
static const char* driver_path = (std::filesystem::current_path().string() + "\\driver\\gdrv.sys").c_str();

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
	static HANDLE load_drv()
	{
		const SC_HANDLE manager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);

		if (!manager)
			return false;

		SC_HANDLE service_handle = CreateService(manager,
			driver_name,
			driver_name,
			SERVICE_START | SERVICE_STOP | DELETE, SERVICE_KERNEL_DRIVER,
			SERVICE_DEMAND_START,
			SERVICE_ERROR_IGNORE,
			driver_path, nullptr, nullptr, nullptr, nullptr, nullptr);

		if (!service_handle)
			service_handle = OpenService(manager, driver_name, SERVICE_START);

		if (!service_handle)
		{
			CloseServiceHandle(manager);
			return false;
		}

		bool result = StartService(service_handle, 0, nullptr);

		if (!result)
			printf("[-] failed to start service, last_error=%d\n", GetLastError());

		CloseServiceHandle(service_handle);
		CloseServiceHandle(manager);

		auto handle = CreateFile("\\\\.\\GIO", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		std::cout << "[+] service started, handle: " << handle << std::endl;
		return handle;
	}

	inline HANDLE drv_handle = load_drv();

	/*
		please code this function depending on your method of physical read/write.
	*/
	static bool unload_drv()
	{
		const SC_HANDLE manager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);

		if (!manager)
			return false;

		const SC_HANDLE service_handle = OpenService(manager, driver_name, SERVICE_STOP | DELETE);
		if (!service_handle)
		{
			CloseServiceHandle(manager);
			return false;
		}

		SERVICE_STATUS status = { 0 };
		bool result = ControlService(service_handle, SERVICE_CONTROL_STOP, &status);

		DeleteService(service_handle);
		CloseServiceHandle(service_handle);
		CloseServiceHandle(manager);
		return result;
	}

	/*
		please code this function depending on your method of physical read/write.
	*/
	static std::uintptr_t map_phys(
		std::uintptr_t addr,
		std::size_t size
	)
	{
		GIOMAP in_buffer = { 0, 0, addr, 0, size };
		uintptr_t out_buffer[2] = { 0 };
		unsigned long returned = 0;
		DeviceIoControl(drv_handle, 0xC3502004, reinterpret_cast<LPVOID>(&in_buffer), sizeof(in_buffer),
			reinterpret_cast<LPVOID>(out_buffer), sizeof(out_buffer), &returned, NULL);
		return out_buffer[0];
	}

	/*
		please code this function depending on your method of physical read/write.
	*/
	static bool unmap_phys(
		std::uintptr_t addr,
		std::size_t size
	)
	{
		uintptr_t in_buffer = addr;
		uintptr_t out_buffer[2] = { 0 };

		unsigned long returned = 0;
		DeviceIoControl(drv_handle, 0xC3502008, reinterpret_cast<LPVOID>(&in_buffer), sizeof(in_buffer),
			reinterpret_cast<LPVOID>(out_buffer), sizeof(out_buffer), &returned, NULL);

		return out_buffer[0];
	}
}