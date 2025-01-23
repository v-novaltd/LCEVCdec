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

// Video
var video;

// Video controls
var pause;

// lcevc variable.
var lcevc_data;
var lcevc_enable;
var lcevc_ptr;
var pss_data_ptr;
var stream_width_data_ptr;
var stream_height_data_ptr;
var lcevc_buffer;
var full_data_ptr;
var loq_base_enabled_ptr;
var loq_high_enabled_ptr;

var lcevcUUID = new Uint8Array([
	0xa7,
	0xc4,
	0x6d,
	0xed,
	0x49,
	0xd8,
	0x38,
	0xeb,
	0x9a,
	0xad,
	0x6d,
	0xa6,
	0x84,
	0x97,
	0xa7,
	0x54,
]);

var lcevcITU = new Uint8Array([0xb4, 0x00, 0x50, 0x00]);

function compareArrays(arrayOne, arrayTwo)
{
	if (arrayOne.length !== arrayTwo.length)
	{
		return false;
	}
	var length = arrayOne.length;
	for (var i = 0; i < length; i++)
	{
		if (arrayOne[i] !== arrayTwo[i])
		{
			return false;
		}
	}
	return true;
}

function findPssData(data, size)
{
	for (var i = 0; i < size; i++)
	{
		var seiLength  = 0;
		var found_type = 0;
		/* look for SEI NAL-Unit */
		if (data[i] === 0x00 && data[i + 1] === 0x00 && data[i + 2] === 0x01 && data[i + 3] === 0x06
			&& data[i + 4] === 0x05)
		{
			found_type = 1;
		}
		else if (data[i] === 0x00 && data[i + 1] === 0x00 && data[i + 2] === 0x01
				 && data[i + 3] === 0x06 && data[i + 4] === 0x04)
		{
			found_type = 2;
		}
		else
		{
			continue;
		}
		i += 5;

		/* Read the SEI Payload Size */
		do
		{
			seiLength += data[i];
		} while (data[i++] === 0xff);

		if ((found_type === 1 && seiLength > lcevcUUID.length)
			|| (found_type === 2 && seiLength > lcevcITU.length))
		{
			/* Look for lcevc Data */
			var pssData;
			var slice;
			if (found_type === 1)
			{
				slice = data.subarray(i, i + lcevcUUID.length);
				if (compareArrays(slice, lcevcUUID))
				{
					i += lcevcUUID.length;
					pssData = new Uint8Array(seiLength - lcevcUUID.length);
				}
				else
				{
					i += seiLength;
					continue;
				}
			}
			else if (found_type === 2)
			{
				slice = data.subarray(i, i + lcevcITU.length);
				if (compareArrays(slice, lcevcITU))
				{
					i += lcevcITU.length;
					pssData = new Uint8Array(seiLength - lcevcITU.length);
				}
				else
				{
					i += seiLength;
					continue;
				}
			}

			var currentPos = i;
			for (var j = 0; j < pssData.length; j++)
			{
				/* Remove the emulation prevention bytes */
				if (data[currentPos - 2] === 0x00 && data[currentPos - 1] === 0x00 && data[currentPos] === 0x03)
				{
					currentPos++;
				}
				pssData[j] = data[currentPos];
				currentPos++;
			}

			lcevc_data.push(pssData);
		}
		i += seiLength;
	}

	if (lcevc_data.size !== 0)
	{
		console.log('lcevc data loaded.');
	}
	else
	{
		console.error('Could not find lcevc data');
	}
}

function readBin(bin_data)
{
	lcevc_data = [0];

	var result_array = new Uint8Array(bin_data);

	var pos = 0;
	while (pos < result_array.length)
	{
		var size = (result_array[pos + 3] << 24) + (result_array[pos + 2] << 16)
				   + (result_array[pos + 1] << 8) + result_array[pos];

		pos += 4;
		lcevc_data.push(result_array.subarray(pos, pos + size));
		pos += size;
	}
}

function freeLcevcBuffers()
{
	freeDataPointer(pss_data_ptr);
	freeDataPointer(lcevc_buffer);
	freeDataPointer(full_data_ptr);
	freeDataPointer(stream_width_data_ptr);
	freeDataPointer(stream_height_data_ptr);
	freeDataPointer(loq_base_enabled_ptr);
	freeDataPointer(loq_high_enabled_ptr);
}

function initLcevcBuffers()
{
	pss_data_ptr           = createDataPointer(new Uint8Array(1024 * 1024));
	lcevc_buffer           = createDataPointer(new Uint8Array(640 * 360 * 2));
	full_data_ptr          = createDataPointer(new Uint8Array(640 * 360 * 2));
	stream_width_data_ptr  = createDataPointer32(new Uint32Array(1));
	stream_height_data_ptr = createDataPointer32(new Uint32Array(1));
	loq_base_enabled_ptr   = createDataPointer(new Uint8Array(1));
	loq_high_enabled_ptr   = createDataPointer(new Uint8Array(1));
}

function initLcevc()
{
	lcevc_ptr = Module._perseus_decoder_open_wrapper(1, 0);

	return 0;
}

function applyLcevc(lcevcPointer, buffer, width, height, frame)
{
	var total_start = performance.now();

	if (frame >= lcevc_data.length)
	{
		return [-1];
	}

	// Create a buffer compatible with lcevc with the decoded buffer picture.
	var pss_size         = 0;
	var lcevc_frame_data = lcevc_data[frame];
	pss_data_ptr         = setDataPointer(pss_data_ptr, lcevc_frame_data);
	pss_size             = lcevc_frame_data.length;

	var parse_start = performance.now();
	if (Module._perseus_decoder_parse_wrapper(
			lcevcPointer, pss_data_ptr.byteOffset, pss_size, 0, stream_width_data_ptr.byteOffset,
			stream_height_data_ptr.byteOffset, loq_base_enabled_ptr.byteOffset, loq_high_enabled_ptr.byteOffset))
	{
		console.error('Could not parse the lcevc data.');
		return [-2];
	}
	var parse_end = performance.now();

	var base_start  = performance.now();
	var baseDataPtr = buffer;
	if (Module._perseus_decoder_decode_base_wrapper(lcevcPointer, baseDataPtr.byteOffset, width))
	{
		console.error('Could not apply lcevc base decoder.');
		return [-2];
	}
	var base_end = performance.now();

	var full_width  = stream_width_data_ptr[0];
	var full_height = stream_height_data_ptr[0];
	var full_size   = full_width * full_height;
	full_size += (full_size / 4) * 2;

	full_data_ptr = resizeDataPointer(full_data_ptr, full_size);

	var upscale_start = performance.now();
	if (Module._perseus_decoder_upscale_wrapper(lcevcPointer, baseDataPtr.byteOffset, width, height,
												full_data_ptr.byteOffset, full_width, full_height))
	{
		console.error('Could not apply lcevc upscaler.');
		return [-2];
	}
	var upscale_end = performance.now();

	var high_start = performance.now();
	if (Module._perseus_decoder_decode_high_wrapper(lcevcPointer, full_data_ptr.byteOffset, full_width))
	{
		console.error('Could not apply lcevc high decoder');
		return [-2];
	}
	var high_end = performance.now();

	var total_time   = high_end - total_start;
	var parse_time   = parse_end - parse_start;
	var base_time    = base_end - base_start;
	var upscale_time = upscale_end - upscale_start;
	var high_time    = high_end - high_start;

	return [
		full_width,
		full_height,
		total_time,
		parse_time,
		base_time,
		upscale_time,
		high_time,
	];
}

function initLib(video, bin_data)
{
	lcevc_data = [0];

	if (!bin_data)
	{
		findPssData(video, video.length);
	}
	else
	{
		readBin(bin_data);
	}

	initLcevc();
	initLcevcBuffers();
}

function processFrame(join_buffer, y_width, height, frame)
{
	lcevc_buffer = resizeDataPointer(lcevc_buffer, join_buffer.length);
	lcevc_buffer = setDataPointer(lcevc_buffer, join_buffer);

	var ret_values = applyLcevc(lcevc_ptr, lcevc_buffer, y_width, height, frame);

	var ret_width = ret_values[0];
	if (ret_width === -1)
	{
		console.log('No more lcevc data, skipping frame.');
		return 1;
	}
	else if (ret_width === -2)
	{
		console.error('Error applying lcevc.');
		return -1;
	}

	var ret_height   = ret_values[1];
	var total_time   = ret_values[2];
	var parse_time   = ret_values[3];
	var base_time    = ret_values[4];
	var upscale_time = ret_values[5];
	var high_time    = ret_values[6];

	var copy_full_data = new Uint8Array(full_data_ptr);
	self.postMessage({
		full_data: copy_full_data,
		width: ret_width,
		height: ret_height,
		y_width: y_width,
		total_time: total_time,
		parse_time: parse_time,
		base_time: base_time,
		upscale_time: upscale_time,
		high_time: high_time,
	},
					 [copy_full_data.buffer]);
}

self.onmessage = function(e) {
	if (e.data.pause !== undefined)
	{
		pause = e.data.pause;
	}

	if (e.data.init)
	{
		video        = e.data.video;
		lcevc_enable = e.data.lcevc_enable;

		initLib(video, e.data.bin_data);
	}
	else if (e.data.ended)
	{
		freeLcevcBuffers();

		self.postMessage({ended: true});
	}
	else if (e.data.join_buffer)
	{
		if (e.data.change)
		{
			return;
		}
		else if (pause)
		{
			return;
		}

		var join_buffer = e.data.join_buffer;
		var y_width     = e.data.y_width;
		var height      = e.data.height;
		var frame       = e.data.frame;

		processFrame(join_buffer, y_width, height, frame);
	}
};
