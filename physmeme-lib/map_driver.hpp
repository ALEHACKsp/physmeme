#pragma once
#include <windows.h>
#include <iostream>
#include <fstream>

#include "kernel_ctx/kernel_ctx.h"
#include "drv_image/drv_image.h"

namespace physmeme
{
	bool __cdecl map_driver(std::vector<std::uint8_t>& raw_driver);
	bool __cdecl map_driver(std::uint8_t * image, std::size_t size);
}