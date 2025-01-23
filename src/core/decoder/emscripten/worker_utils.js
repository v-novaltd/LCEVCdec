/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

'use strict';

function createDataPointer(data)
{
	if (!Module)
	{
		console.error('Emscripten module is undefined');
		return undefined;
	}

	var nDataBytes = data.length * data.BYTES_PER_ELEMENT;
	var dataPtr    = Module._malloc(nDataBytes);
	var dataHeap   = new Uint8Array(Module.HEAPU8.buffer, dataPtr, nDataBytes);
	dataHeap.set(new Uint8Array(data));
	return dataHeap;
}

function createDataPointer32(data)
{
	if (!Module)
	{
		console.error('Emscripten module is undefined');
		return undefined;
	}

	var nDataBytes = data.length;
	var dataPtr    = Module._malloc(nDataBytes);
	var dataHeap   = new Uint32Array(Module.HEAPU32.buffer, dataPtr, nDataBytes);
	dataHeap.set(new Uint32Array(data));
	return dataHeap;
}

function freeDataPointer(data)
{
	if (!Module)
	{
		console.error('Emscripten module is undefined');
		return false;
	}

	if (data != null)
	{
		Module._free(data.byteOffset);
	}

	return true;
}

function setDataPointer(ptr, data)
{
	if (!Module)
	{
		console.error('Emscripten module is undefined');
		return undefined;
	}

	if (ptr.byteLength < data.length)
	{
		if (freeDataPointer(ptr))
		{
			ptr = createDataPointer(data);
		}
		else
		{
			console.error('Could not free pointer before setting it.');
			return undefined;
		}
	}
	ptr.set(new Uint8Array(data));
	return ptr;
}

function resizeDataPointer(ptr, len)
{
	if (!Module)
	{
		console.error('Emscripten module is undefined');
		return undefined;
	}

	if (ptr.byteLength < len)
	{
		if (freeDataPointer(ptr))
		{
			ptr = createDataPointer(new Uint8Array(len));
		}
		else
		{
			console.error('Could not free pointer before setting it.');
			return undefined;
		}
	}
	return ptr;
}
