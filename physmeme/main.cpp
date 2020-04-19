#include <windows.h>
#include <iostream>
#include <fstream>

#include "kernel_ctx/kernel_ctx.h"
#include "drv_image/drv_image.h"

/*
	Author: xerox
	Date: 4/19/2020
*/
int __cdecl main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cout << "[-] invalid use, please provide a path to a driver" << std::endl;
		return -1;
	}

	std::vector<std::uint8_t> drv_buffer;
	util::open_binary_file(argv[1], drv_buffer);

	physmeme::drv_image image(drv_buffer);
	physmeme::kernel_ctx ctx;

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
	// allocate memory in the kernel for the driver
	//
	std::uintptr_t pool_base = reinterpret_cast<std::uintptr_t>(ctx.allocate_pool(image.size(), NonPagedPool));
	std::cout << "[+] allocated " << std::hex << std::showbase << image.size() << " at: " << std::hex << std::showbase << pool_base << std::endl;

	//
	// fix the driver image
	//
	image.fix_imports(_get_module, _get_export_name);
	std::cout << "[+] fixed imports" << std::endl;
	image.map();
	std::cout << "[+] sections mapped in memory" << std::endl;
	image.relocate(pool_base);
	std::cout << "[+] relocations fixed" << std::endl;

	//
	// copy driver into the kernel
	// this might blue screen if the image takes too long to copy
	//
	ctx.write_kernel(pool_base, image.data(), image.size());

	//
	// call driver entry and pass in base address and size of the driver.
	//
	auto entry_point = pool_base + image.entry_point();
	auto size = image.size();

	auto result = ctx.syscall<DRIVER_INITIALIZE>(reinterpret_cast<void*>(entry_point), pool_base, image.size());
	std::cout << "[+] driver entry returned: " << std::hex << result << std::endl;
	physmeme::unload_drv();

	std::cout << "[=] press enter to close" << std::endl;
	std::cin.get();
}