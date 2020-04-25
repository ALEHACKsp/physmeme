#include "map_driver.hpp"

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
#if PHYSMEME_DEBUGGING true
		std::cout << "[+] allocated " << std::hex << std::showbase << image.size() << " at: " << std::hex << std::showbase << pool_base << std::endl;
#endif

		//
		// fix the driver image
		//
		image.fix_imports(_get_module, _get_export_name);
#if PHYSMEME_DEBUGGING true
		std::cout << "[+] fixed imports" << std::endl;
#endif

		image.map();
#if PHYSMEME_DEBUGGING true
		std::cout << "[+] sections mapped in memory" << std::endl;
#endif
		image.relocate(pool_base);
#if PHYSMEME_DEBUGGING true
		std::cout << "[+] relocations fixed" << std::endl;
#endif

		//
		// copy driver into the kernel
		// this might blue screen if the image takes too long to copy
		//
		ctx.write_kernel(pool_base, image.data(), image.size());

		//
		// driver entry params
		//
		auto entry_point = pool_base + image.entry_point();
		auto size = image.size();

		//
		// call driver entry
		//
		auto result = ctx.syscall<DRIVER_INITIALIZE>(reinterpret_cast<void*>(entry_point), pool_base, image.size());
#if PHYSMEME_DEBUGGING true
		std::cout << "[+] driver entry returned: " << std::hex << result << std::endl;
#endif

		//
		// zero header of driver
		//
		ctx.zero_kernel_memory(pool_base, image.header_size());
#if PHYSMEME_DEBUGGING true
		std::cout << "[+] zero'ed driver's pe header" << std::endl;
#endif

		//
		// close and unload vuln drivers
		//
#if PHYSMEME_DEBUGGING true
		std::cout << "[=] press enter to close" << std::endl;
#endif
		physmeme::unload_drv();
		std::cin.get();
	}

	bool __cdecl map_driver(std::uint8_t * image, std::size_t size)
	{
		auto data = std::vector<std::uint8_t>(image, image + size);
		map_driver(data);
	}
}