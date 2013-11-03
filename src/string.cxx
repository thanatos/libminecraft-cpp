#include <climits>


#include "nbt.h"


namespace nbt {
namespace detail {

	/*
	 * This is the FNV-1a hash.
	 * It is only specialized for systems with CHAR_BIT, and 4 or 8 byte
	 * size_t's.
	 */
	template<size_t size_t_size, size_t charbits>
	struct FnvHash1AConstants;

	template<>
	struct FnvHash1AConstants<8, 8> {
		static const size_t fnv_prime = 1099511628211ULL;
		static const size_t fnv_offset_bias = 14695981039346656037ULL;
	};
	template<>
	struct FnvHash1AConstants<4, 8> {
		static const size_t fnv_prime = 16777619UL;
		static const size_t fnv_offset_bias = 2166136261UL;
	};


	template<typename IT>
	size_t fnv_hash(IT begin, IT end) {
		size_t hash_value = FnvHash1AConstants<sizeof(size_t), CHAR_BIT>::fnv_offset_bias;
		for(; begin != end; ++begin) {
			hash_value = hash_value * FnvHash1AConstants<sizeof(size_t), CHAR_BIT>::fnv_prime;
			hash_value = hash_value ^ *begin;
		}
		return hash_value;
	}
}

size_t Utf8StringHash::operator () (const nbt::Utf8String &s) const {
	return nbt::detail::fnv_hash(s.data.begin(), s.data.end());
}

}
