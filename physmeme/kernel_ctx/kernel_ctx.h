#pragma once
#include <windows.h>
#include <string_view>
#include <vector>
#include <thread>
#include <atomic>

#include "../util/util.hpp"
#include "../physmeme/physmeme.hpp"
#include "../util/hook.hpp"

#define PHYSMEME_DEBUGGING 1

#if PHYSMEME_DEBUGGING
#include <iostream>
#endif

/*
	Author: xerox
	Date: 4/19/2020

	this namespace contains everything needed to interface with the kernel
*/
namespace physmeme
{
	class kernel_ctx
	{
	public:
		kernel_ctx();
		void* allocate_pool(std::size_t size, POOL_TYPE pool_type = NonPagedPool);
		void* allocate_pool(std::size_t size, ULONG pool_tag = 'MEME', POOL_TYPE pool_type = NonPagedPool);

		void read_kernel(std::uintptr_t addr, void* buffer, std::size_t size);
		void write_kernel(std::uintptr_t addr, void* buffer, std::size_t size);

		void zero_kernel_memory(std::uintptr_t addr, std::size_t size);

		template <class T>
		T read_kernel(std::uintptr_t addr)
		{
			if (!addr)
				return  {};
			T buffer;
			read_kernel(addr, &buffer, sizeof(T));
			return buffer;
		}

		template <class T>
		void write_kernel(std::uintptr_t addr, const T& data)
		{
			if (!addr)
				return {};
			write_kernel(addr, &data, sizeof(T));
		}

		//
		// use this to call any function in the kernel
		//
		template <class T, class ... Ts>
		PVOID syscall(void* addr, Ts ... args)
		{
			auto proc = GetProcAddress(GetModuleHandleA("ntdll.dll"), syscall_hook.first.data());
			if (!proc || !psyscall_func || !addr)
				return reinterpret_cast<PVOID>(STATUS_INVALID_PARAMETER);

			hook::make_hook(psyscall_func, addr);
			PVOID result = reinterpret_cast<PVOID>(reinterpret_cast<T>(proc)(args ...));
			hook::remove(psyscall_func);
			return result;
		}

	private:

		//
		// find and map the physical page of a syscall into this process
		//
		void map_syscall(std::uintptr_t begin, std::uintptr_t end) const;

		//
		// mapping of a syscalls physical memory (for installing hooks)
		//
		mutable std::atomic<void*> psyscall_func;

		//
		// you can edit this how you choose, im hooking NtTraceControl.
		//
		const std::pair<std::string_view, std::string_view> syscall_hook = { "NtSystemShutdown", "ntdll.dll" };

		//
		// offset of function into a physical page
		// used for comparing bytes when searching
		//
		std::uint16_t nt_page_offset;

		//
		// rva of nt function we are going to hook
		//
		std::uint32_t nt_rva;

		//
		// base address of ntoskrnl (inside of this process)
		//
		const std::uint8_t* ntoskrnl_buffer;
	};
}