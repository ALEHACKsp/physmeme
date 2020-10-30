#include "kernel_ctx/kernel_ctx.h"
#include "drv_image/drv_image.h"

namespace physmeme
{
	NTSTATUS __cdecl map_driver(std::vector<std::uint8_t>& raw_driver)
	{
		physmeme::drv_image image(raw_driver);
		if (!physmeme::load_drv())
			return STATUS_ABANDONED;

		physmeme::kernel_ctx ctx;
		const auto drv_timestamp = util::get_file_header(raw_driver.data())->TimeDateStamp;
		if (!ctx.clear_piddb_cache(physmeme::drv_key, drv_timestamp))
			return STATUS_ABANDONED;

		const auto _get_export_name = [&](const char* base, const char* name)
		{
			return reinterpret_cast<std::uintptr_t>(util::get_kernel_export(base, name));
		};

		image.fix_imports(_get_export_name);
		image.map();

		const auto pool_base = 
			ctx.allocate_pool(
				image.size(),
				NonPagedPool
			);

		image.relocate(pool_base);
		ctx.write_kernel(pool_base, image.data(), image.size());
		auto entry_point = reinterpret_cast<std::uintptr_t>(pool_base) + image.entry_point();

		auto result = ctx.syscall<DRIVER_INITIALIZE>
		(
			reinterpret_cast<void*>(entry_point),
			reinterpret_cast<std::uintptr_t>(pool_base),
			image.size()
		);

		ctx.zero_kernel_memory(pool_base, image.header_size());
		if (!physmeme::unload_drv())
			return STATUS_ABANDONED;
		return result;
	}

	NTSTATUS __cdecl map_driver(std::uint8_t * image, std::size_t size)
	{
		std::vector<std::uint8_t> data(image, image + size);
		return map_driver(data);
	}
}