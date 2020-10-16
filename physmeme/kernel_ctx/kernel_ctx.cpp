#include "kernel_ctx.h"

namespace physmeme
{
	kernel_ctx::kernel_ctx()
	{
		if (psyscall_func.load() || nt_page_offset || ntoskrnl_buffer)
			return;

		ntoskrnl_buffer = reinterpret_cast<std::uint8_t*>(
			LoadLibraryEx("ntoskrnl.exe", NULL,
				DONT_RESOLVE_DLL_REFERENCES));

		nt_rva = reinterpret_cast<std::uint32_t>(
			util::get_kernel_export(
				"ntoskrnl.exe",
				syscall_hook.first,
				true
			));

		nt_page_offset = nt_rva % page_size;
		std::vector<std::thread> search_threads;

		for (auto ranges : util::pmem_ranges)
			search_threads.emplace_back(std::thread(
				&kernel_ctx::map_syscall,
				this,
				ranges.first,
				ranges.second
			));

		for (std::thread& search_thread : search_threads)
			search_thread.join();
	}

	void kernel_ctx::map_syscall(std::uintptr_t begin, std::uintptr_t size) const
	{
		for (auto page = 0u; page < size; page += page_size)
		{
			if (is_page_found.load())
				break;

			auto virt_addr = physmeme::map_phys(begin + page, page_size);
			// sometimes the virtual address returned from map_phys is 1 page ahead
			// of the actual mapping... This is a problem with the driver...
			__try
			{
				if (!memcmp((void*)(virt_addr + nt_page_offset), ntoskrnl_buffer + nt_rva, 32))
				{
					psyscall_func.store(reinterpret_cast<void*>(virt_addr + nt_page_offset));
					if (get_proc_base(GetCurrentProcessId()) == GetModuleHandle(NULL))
					{
						is_page_found.store(true);
						return;
					}
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {}
			physmeme::unmap_phys(virt_addr, page_size);
		}
	}

	void* kernel_ctx::allocate_pool(std::size_t size, POOL_TYPE pool_type)
	{
		static const auto ex_alloc_pool = 
			util::get_kernel_export(
				"ntoskrnl.exe", 
				"ExAllocatePool"
			);

		return syscall<ExAllocatePool>
		(
			ex_alloc_pool, 
			pool_type,
			size
		);
	}

	void* kernel_ctx::allocate_pool(std::size_t size, ULONG pool_tag, POOL_TYPE pool_type)
	{
		static const auto ex_alloc_pool_with_tag = 
			util::get_kernel_export(
				"ntoskrnl.exe", 
				"ExAllocatePoolWithTag"
			);

		return syscall<ExAllocatePoolWithTag>(
			ex_alloc_pool_with_tag,
			pool_type,
			size,
			pool_tag
		);
	}

	void kernel_ctx::read_kernel(void* addr, void* buffer, std::size_t size)
	{
		static const auto mm_copy_memory = 
			util::get_kernel_export
			(
				"ntoskrnl.exe", 
				"RtlCopyMemory"
			);

		syscall<decltype(&memcpy)>
		(
			mm_copy_memory,
			buffer,
			addr,
			size
		);
	}

	void kernel_ctx::write_kernel(void* addr, void* buffer, std::size_t size)
	{
		static const auto mm_copy_memory = 
			util::get_kernel_export
			(
				"ntoskrnl.exe",
				"RtlCopyMemory"
			);

		syscall<decltype(&memcpy)>
		(
			mm_copy_memory,
			addr,
			buffer,
			size
		);
	}

	void kernel_ctx::zero_kernel_memory(void* addr, std::size_t size)
	{
		static const auto rtl_zero_memory = 
			util::get_kernel_export
			(
				"ntoskrnl.exe",
				"RtlZeroMemory"
			);

		syscall<decltype(&RtlSecureZeroMemory)>
		(
			rtl_zero_memory, 
			addr,
			size
		);
	}

	PEPROCESS kernel_ctx::get_peprocess(unsigned pid) const
	{
		PEPROCESS proc;
		static auto get_peprocess_from_pid =
			util::get_kernel_export
			(
				"ntoskrnl.exe",
				"PsLookupProcessByProcessId"
			);

		syscall<PsLookupProcessByProcessId>
		(
			get_peprocess_from_pid,
			(HANDLE)pid,
			&proc
		);
		return proc;
	}

	void* kernel_ctx::get_proc_base(unsigned pid) const
	{
		const auto peproc = get_peprocess(pid);
		if (!peproc) return {};

		static auto get_section_base = 
			util::get_kernel_export
			(
				"ntoskrnl.exe",
				"PsGetProcessSectionBaseAddress"
			);

		return syscall<PsGetProcessSectionBaseAddress>
		(
			get_section_base,
			peproc
		);
	}
}