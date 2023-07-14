/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

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
