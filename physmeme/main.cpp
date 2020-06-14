#include "kernel_ctx/kernel_ctx.h"
#include "drv_image/drv_image.h"

int __cdecl main(int argc, char** argv)
{
	if (argc < 2)
	{
		perror("[-] invalid use, please provide a path to a driver\n");
		return -1;
	}

	std::vector<std::uint8_t> drv_buffer;
	util::open_binary_file(argv[1], drv_buffer);

	if (!drv_buffer.size())
	{
		perror("[-] invalid drv_buffer size\n");
		return -1;
	}

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
	// fix the driver image
	//
	image.fix_imports(_get_module, _get_export_name);
	printf("[+] fixed imports\n");

	image.map();
	printf("[+] sections mapped in memory\n");

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
	physmeme::unload_drv();
	printf("[=] press enter to close\n");
	std::cin.get();
}