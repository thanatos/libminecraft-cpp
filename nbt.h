#ifndef NBT_H
#define NBT_H

#include <cstdint>
#include <istream>
#include <memory>
#include <unordered_map>
#include <vector>


namespace nbt {
	/**
	 * Although the format is called "Named Binary Tag", tags only have names if:
	 * 1. They're the root tag.
	 * 2. They're in a compound tag.
	 *
	 * Thus, we represent the data that way.
	 */

	class Utf8String {
		public:
			std::vector<unsigned char> data;

			Utf8String() : data() { }
			Utf8String(unsigned char *p_data, size_t p_length) :
				data(p_data, p_data + p_length) {}

			bool operator == (const Utf8String &other) const {
				return data == other.data;
			}
			bool operator != (const Utf8String &other) const {
				return !(*this == other);
			}
	};

	class Utf8StringHash {
		size_t operator() (const nbt::Utf8String &s) const;
	};

	class Tag {
	public:
		virtual ~Tag() {};
	};

	template<typename T>
	class BasicTag : public Tag {
		typedef T value_type;
		T value;
	};
	typedef BasicTag<int8_t> ByteTag;
	typedef BasicTag<int16_t> ShortTag;
	typedef BasicTag<int32_t> IntTag;
	typedef BasicTag<int64_t> LongTag;
	typedef BasicTag<float> FloatTag;
	typedef BasicTag<double> DoubleTag;

	class ByteArrayTag : public Tag {
		std::vector<unsigned char> value;
	};

	class StringTag : public Tag {
		Utf8String value;
	};

	class ListTag : public Tag {
		std::vector<std::unique_ptr<Tag>> values;
	};

	class CompoundTag : public Tag {
		std::unordered_map<Utf8String, std::unique_ptr<Tag>, Utf8StringHash> values;
	};

	class IntArrayTag : public Tag {
		std::vector<int32_t> values;
	};

	class RootTag {
		public:
			Utf8String name;
			std::unique_ptr<Tag> tag;
	};


	namespace io {

		const unsigned char TAG_TYPE_END = 0;
		const unsigned char TAG_TYPE_BYTE = 1;
		const unsigned char TAG_TYPE_SHORT = 2;
		const unsigned char TAG_TYPE_INT = 3;
		const unsigned char TAG_TYPE_LONG = 4;
		const unsigned char TAG_TYPE_FLOAT = 5;
		const unsigned char TAG_TYPE_DOUBLE = 6;
		const unsigned char TAG_TYPE_BYTE_ARRAY = 7;
		const unsigned char TAG_TYPE_STRING = 8;
		const unsigned char TAG_TYPE_LIST = 9;
		const unsigned char TAG_TYPE_COMPOUND = 10;
		const unsigned char TAG_TYPE_INT_ARRAY = 11;

		class IoError : public std::exception {
		public:
			virtual const char * what() const throw() {
				return "I/O error while reading NBT stream.";
			}
		};

		class PrematureEof : public std::exception {
		public:
			virtual const char * what() const throw() {
				return "Premature EOF while parsing NBT.";
			}
		};

		class InputStream {
		public:
			virtual void read(unsigned char *data, size_t size) = 0;
			virtual ~InputStream() {};
		};


		class IStreamInputStream {
		public:
			IStreamInputStream(std::istream &stream) : m_istream(stream) {}
			virtual void read(unsigned char *data, size_t size) {
				if(!m_istream.read(reinterpret_cast<char *>(data), size)) {
					if(m_istream.fail()) {
						throw IoError();
					} else {
						throw PrematureEof();
					}
				}
			}
		private:
			std::istream &m_istream;
		};

	}
}

namespace std {
}

#endif
