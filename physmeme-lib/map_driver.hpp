#pragma once
#include <vector>

namespace physmeme
{
	bool __cdecl map_driver(std::vector<std::uint8_t>& raw_driver);
	bool __cdecl map_driver(std::uint8_t * image, std::size_t size);
}