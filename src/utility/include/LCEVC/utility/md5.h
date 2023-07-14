// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Compuite MD5 checksum
//
// See https://en.wikipedia.org/wiki/MD5
//
#ifndef VN_LCEVC_UTILITY_MD5_H
#define VN_LCEVC_UTILITY_MD5_H

#include <cstdint>
#include <string>

namespace lcevc_dec::utility {

class MD5 {
public:
	MD5() { reset(); }

	// Reset to initial state
	void reset();

	// Append data to message
	void update(const uint8_t *data, uint32_t size);

	// Finish message, and fetch the message's digest as bytes
	void digest(uint8_t output[16]);

	// Finish message, and fetch the message's digest as a string of hex digits
	std::string hexDigest();

private:
	// Size of each chunk in bytes
	static constexpr uint32_t kChunkSize = 64;

	// Process one 512 bit chunk of message
	void chunk(const uint8_t *data);

	// Finish processing message
	void finish();

	// Current sum
	uint32_t m_a0{0};
	uint32_t m_b0{0};
	uint32_t m_c0{0};
	uint32_t m_d0{0};

	// Length in bits
	uint64_t m_length{0};

	// Pending message data
	uint8_t m_chunk[kChunkSize]{};
	uint32_t m_chunkSize{0};

	bool m_finished{false};
};

}
#endif
