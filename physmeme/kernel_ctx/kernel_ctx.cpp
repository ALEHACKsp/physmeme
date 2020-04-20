#include "kernel_ctx.h"

namespace physmeme
{
	/*
		Author: xerox
		Date: 4/19/2020
	*/
	kernel_ctx::kernel_ctx()
		: psyscall_func(NULL), ntoskrnl_buffer(NULL)
	{
		nt_rva = reinterpret_cast<std::uint32_t>(
			util::get_module_export(
				"ntoskrnl.exe",
				syscall_hook.first.data(),
				true
			));

		nt_page_offset = nt_rva % 0x1000;
		ntoskrnl_buffer = reinterpret_cast<std::uint8_t*>(LoadLibraryA("C:\\Windows\\System32\\ntoskrnl.exe"));

#if PHYSMEME_DEBUGGING
		std::cout << "[+] page offset of " << syscall_hook.first << " is: " << std::hex << nt_page_offset << std::endl;
#endif

		std::vector<std::thread> search_threads;
		//--- for each physical memory range, make a thread to search it
		for (auto ranges : pmem_ranges)
			search_threads.emplace_back(std::thread(
				&kernel_ctx::map_syscall,
				this,
				ranges.first,
				ranges.second
			));

		for (std::thread& search_thread : search_threads)
			search_thread.join();

#if PHYSMEME_DEBUGGING
		std::cout << "[+] psyscall_func: " << std::hex << std::showbase << psyscall_func.load() << std::endl;
#endif
	}

	/*
		author: xerox
		date: 4/18/2020

		finds physical page of a syscall and map it into this process.
	*/
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
					if (!psyscall_func) // keep scanning until its found
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

	/*
		Author: xerox
		Date: 4/19/2020

		allocate a pool in the kernel (no tag)
	*/
	void* kernel_ctx::allocate_pool(std::size_t size, POOL_TYPE pool_type)
	{
		static const auto ex_alloc_pool = 
			util::get_module_export(
				"ntoskrnl.exe", 
				"ExAllocatePool"
			);
		if (ex_alloc_pool)
			return syscall<ExAllocatePool>(ex_alloc_pool, pool_type, size);
		return NULL;
	}

	/*
		Author: xerox
		Date: 4/19/2020

		allocate a pool in the kernel with a tag
	*/
	void* kernel_ctx::allocate_pool(std::size_t size, ULONG pool_tag, POOL_TYPE pool_type)
	{
		static const auto ex_alloc_pool_with_tag = 
			util::get_module_export(
				"ntoskrnl.exe", 
				"ExAllocatePoolWithTag"
			);
		if (ex_alloc_pool_with_tag)
			return syscall<ExAllocatePoolWithTag>(ex_alloc_pool_with_tag, pool_type, size, pool_tag);
	}

	/*
		Author: xerox
		Date: 4/19/2020

		read kernel memory
	*/
	void kernel_ctx::read_kernel(std::uintptr_t addr, void* buffer, std::size_t size)
	{
		size_t amount_copied;
		static const auto mm_copy_memory = 
			util::get_module_export(
				"ntoskrnl.exe", 
				"MmCopyMemory"
			);
		if (mm_copy_memory)
			syscall<MmCopyMemory>(
				mm_copy_memory, 
				reinterpret_cast<void*>(buffer),
				MM_COPY_ADDRESS{ (void*)addr },
				size,
				MM_COPY_MEMORY_VIRTUAL, 
				&amount_copied
			);
	}

	/*
		Author: xerox
		Date: 4/19/2020

		write kernel memory, this doesnt write to read only memory!
	*/
	void kernel_ctx::write_kernel(std::uintptr_t addr, void* buffer, std::size_t size)
	{
		size_t amount_copied;
		static const auto mm_copy_memory = 
			util::get_module_export(
				"ntoskrnl.exe",
				"MmCopyMemory"
			);
		if (mm_copy_memory)
			syscall<MmCopyMemory>(
				mm_copy_memory, 
				reinterpret_cast<void*>(addr),
				MM_COPY_ADDRESS{ buffer },
				size, 
				MM_COPY_MEMORY_VIRTUAL,
				&amount_copied
			);
	}

	/*
		Author: xerox
		Date: 4/19/2020

		zero driver header
	*/
	void kernel_ctx::zero_kernel_memory(std::uintptr_t addr, std::size_t size)
	{
		static const auto rtl_zero_memory = 
			util::get_module_export(
				"ntoskrnl.exe",
				"RtlZeroMemory"
			);
		syscall<decltype(&RtlSecureZeroMemory)>(
			rtl_zero_memory, 
			reinterpret_cast<void*>(addr),
			size
		);
	}
}