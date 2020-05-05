#pragma once
#include <Windows.h>
#include <cstdint>
#include <string_view>
#include <iterator>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <ntstatus.h>
#include <winternl.h>

#include "nt.hpp"

namespace util
{ 
	//--- ranges of physical memory
	static std::map<std::uintptr_t, std::size_t> pmem_ranges{};

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

	// this was taken from wlan's drvmapper:
	// https://github.com/not-wlan/drvmap/blob/98d93cc7b5ec17875f815a9cb94e6d137b4047ee/drvmap/util.cpp#L7
	static void open_binary_file(const std::string& file, std::vector<uint8_t>& data)
	{
		std::ifstream fstr(file, std::ios::binary);
		fstr.unsetf(std::ios::skipws);
		fstr.seekg(0, std::ios::end);

		const auto file_size = fstr.tellg();

		fstr.seekg(NULL, std::ios::beg);
		data.reserve(static_cast<uint32_t>(file_size));
		data.insert(data.begin(), std::istream_iterator<uint8_t>(fstr), std::istream_iterator<uint8_t>());
	}

	// get base address of kernel module
	//
	// taken from: https://github.com/z175/kdmapper/blob/master/kdmapper/utils.cpp#L30
	static std::uintptr_t get_module_base(const char* module_name)
	{
		void* buffer = nullptr;
		DWORD buffer_size = NULL;

		NTSTATUS status = NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(SystemModuleInformation), buffer, buffer_size, &buffer_size);

		while (status == STATUS_INFO_LENGTH_MISMATCH)
		{
			VirtualFree(buffer, NULL, MEM_RELEASE);
			buffer = VirtualAlloc(nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			status = NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(SystemModuleInformation), buffer, buffer_size, &buffer_size);
		}

		if (!NT_SUCCESS(status))
		{
			VirtualFree(buffer, NULL, MEM_RELEASE);
			return NULL;
		}

		const auto modules = static_cast<PRTL_PROCESS_MODULES>(buffer);
		for (auto idx = 0u; idx < modules->NumberOfModules; ++idx)
		{
			const std::string current_module_name = std::string(reinterpret_cast<char*>(modules->Modules[idx].FullPathName) + modules->Modules[idx].OffsetToFileName);
			if (!_stricmp(current_module_name.c_str(), module_name))
			{
				const uint64_t result = reinterpret_cast<uint64_t>(modules->Modules[idx].ImageBase);
				VirtualFree(buffer, NULL, MEM_RELEASE);
				return result;
			}
		}

		VirtualFree(buffer, NULL, MEM_RELEASE);
		return NULL;
	}

	// get base address of kernel module
	//
	// taken from: https://github.com/z175/kdmapper/blob/master/kdmapper/utils.cpp#L30
	static void* get_module_export(const char* module_name, const char* export_name, bool rva = false)
	{
		void* buffer = nullptr;
		DWORD buffer_size = 0;

		NTSTATUS status = NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(SystemModuleInformation), buffer, buffer_size, &buffer_size);

		while (status == STATUS_INFO_LENGTH_MISMATCH)
		{
			VirtualFree(buffer, 0, MEM_RELEASE);
			buffer = VirtualAlloc(nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			status = NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(SystemModuleInformation), buffer, buffer_size, &buffer_size);
		}

		if (!NT_SUCCESS(status))
		{
			VirtualFree(buffer, 0, MEM_RELEASE);
			return 0;
		}

		const auto modules = static_cast<PRTL_PROCESS_MODULES>(buffer);
		for (auto idx = 0u; idx < modules->NumberOfModules; ++idx)
		{
			// find module and then load library it
			const std::string current_module_name = std::string(reinterpret_cast<char*>(modules->Modules[idx].FullPathName) + modules->Modules[idx].OffsetToFileName);
			if (!_stricmp(current_module_name.c_str(), module_name))
			{
				// had to shoot the tires off of "\\SystemRoot\\"
				std::string full_path = reinterpret_cast<char*>(modules->Modules[idx].FullPathName);
				full_path.replace(
					full_path.find("\\SystemRoot\\"),
					sizeof("\\SystemRoot\\") - 1,
					std::string(getenv("SYSTEMROOT")).append("\\")
				);

				auto module_base = LoadLibraryA(full_path.c_str());
				PIMAGE_DOS_HEADER p_idh;
				PIMAGE_NT_HEADERS p_inh;
				PIMAGE_EXPORT_DIRECTORY p_ied;

				PDWORD addr, name;
				PWORD ordinal;

				p_idh = (PIMAGE_DOS_HEADER)module_base;
				if (p_idh->e_magic != IMAGE_DOS_SIGNATURE)
					return NULL;

				p_inh = (PIMAGE_NT_HEADERS)((LPBYTE)module_base + p_idh->e_lfanew);
				if (p_inh->Signature != IMAGE_NT_SIGNATURE)
					return NULL;

				if (p_inh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == 0)
					return NULL;

				p_ied = (PIMAGE_EXPORT_DIRECTORY)((LPBYTE)module_base +
					p_inh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

				addr = (PDWORD)((LPBYTE)module_base + p_ied->AddressOfFunctions);
				name = (PDWORD)((LPBYTE)module_base + p_ied->AddressOfNames);
				ordinal = (PWORD)((LPBYTE)module_base + p_ied->AddressOfNameOrdinals);

				// find exported function
				for (auto i = 0; i < p_ied->AddressOfFunctions; i++)
					if (!strcmp(export_name, (char*)module_base + name[i]))
					{
						if (!rva)
						{
							auto result = (void*)((std::uintptr_t)modules->Modules[idx].ImageBase + addr[ordinal[i]]);
							VirtualFree(buffer, NULL, MEM_RELEASE);
							return result;
						}
						else
						{
							auto result = (void*)addr[ordinal[i]];
							VirtualFree(buffer, NULL, MEM_RELEASE);
							return result;
						}
					}
			}
		}
		VirtualFree(buffer, NULL, MEM_RELEASE);
		return NULL;
	}
}