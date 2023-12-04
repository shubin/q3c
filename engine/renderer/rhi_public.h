/*
===========================================================================
Copyright (C) 2022-2023 Gian 'myT' Schellenbaum

This file is part of Challenge Quake 3 (CNQ3).

Challenge Quake 3 is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Challenge Quake 3 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Challenge Quake 3. If not, see <https://www.gnu.org/licenses/>.
===========================================================================
*/
// Rendering Hardware Interface - public interface


#pragma once


#include <stdint.h>


namespace RHI
{
	typedef uint32_t Handle;

#define RHI_HANDLE_TYPE(TypeName) struct TypeName { Handle v; }; \
	inline bool operator==(TypeName a, TypeName b) { return a.v == b.v; } \
	inline bool operator!=(TypeName a, TypeName b) { return a.v != b.v; }
	RHI_HANDLE_TYPE(HBuffer);
	RHI_HANDLE_TYPE(HRootSignature);
	RHI_HANDLE_TYPE(HDescriptorTable);
	RHI_HANDLE_TYPE(HPipeline);
	RHI_HANDLE_TYPE(HTexture);
	RHI_HANDLE_TYPE(HSampler);
	RHI_HANDLE_TYPE(HShader);
#undef RHI_HANDLE_TYPE

	struct MappedTexture
	{
		uint8_t* mappedData;
		uint32_t rowCount;
		uint32_t columnCount;
		uint32_t srcRowByteCount;
		uint32_t dstRowByteCount;
	};
}
