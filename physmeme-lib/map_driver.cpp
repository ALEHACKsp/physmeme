#include <windows.h>
#include <iostream>
#include <fstream>

#include "kernel_ctx/kernel_ctx.h"
#include "drv_image/drv_image.h"

namespace physmeme
{
	/*
		Author: xerox
		Date: 4/19/2020
	*/
	bool __cdecl map_driver(std::vector<std::uint8_t>& raw_driver)
	{
		physmeme::drv_image image(raw_driver);
		physmeme::kernel_ctx ctx;

		//
		// we dont need the driver loaded anymore
		//
		physmeme::unload_drv();

		//
		// allocate memory in the kernel for the driver
		//
		const auto pool_base = ctx.allocate_pool(image.size(), NonPagedPool);
		printf("[+] allocated 0x%llx at 0x%p\n", image.size(), pool_base);

		if (!pool_base)
		{
			printf("[!] allocation failed!\n");
			return -1;
		}

		//
		// lambdas used for fixing driver image
		//
		const auto _get_module = [&](std::string_view name)
		{
			return util::get_module_base(name.data());
		};

		const auto _get_export_name = [&](const char* base, const char* name)
		{
			return reinterpret_cast<std::uintptr_t>(util::get_module_export(base, name));
		};

		//
		// fix the driver image
		//
		image.fix_imports(_get_module, _get_export_name);
		printf("[+] fixed imports\n");

		image.map();
		printf("[+] sections mapped in memory\n");

		image.relocate(pool_base);
		printf("[+] relocations fixed\n");

		//
		// copy driver into the kernel
		//
		ctx.write_kernel(pool_base, image.data(), image.size());

		//
		// driver entry
		//
		auto entry_point = reinterpret_cast<std::uintptr_t>(pool_base) + image.entry_point();

		//
		// call driver entry
		//
		auto result = ctx.syscall<DRIVER_INITIALIZE>(
			reinterpret_cast<void*>(entry_point),
			reinterpret_cast<std::uintptr_t>(pool_base),
			image.size()
			);
		printf("[+] driver entry returned: 0x%p\n", result);

		//
		// zero driver headers
		//
		ctx.zero_kernel_memory(pool_base, image.header_size());
		return !result; // 0x0 means STATUS_SUCCESS
	}

	bool __cdecl map_driver(std::uint8_t * image, std::size_t size)
	{
		auto data = std::vector<std::uint8_t>(image, image + size);
		return map_driver(data);
	}
}