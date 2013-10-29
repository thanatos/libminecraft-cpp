#include <stdexcept>
#include <string>

#include "nbt.h"


#ifdef __GNUC__
  #define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
  #define UNUSED(x) UNUSED_ ## x
#endif


namespace nbt {
namespace io {
namespace detail {

	template<size_t size, typename RT, typename IT>
	RT twos_complement_decode(IT v) {
		IT sign_bit = ((IT) 1) << (size * 8 - 1);
		if (v & sign_bit) {
			IT abs_value = (~v) + 1;
			return -((RT) abs_value - 1) - 1;
		} else {
			return (RT) v;
		}
	}

	template<typename T, size_t size>
	T read_big_endian_unsigned_int(InputStream &s) {
		unsigned char data[size];
		s.read(data, size);

		T n = 0;
		for(size_t i = 0; i < size; ++i) {
			n = (n << 8) | static_cast<T>(data[i]);
		}

		return n;
	}

	template<typename TagType, typename RawType, size_t raw_type_size>
	TagType read_simple_payload(InputStream &s) {
		RawType encoded_value = (
			nbt::io::detail::read_big_endian_unsigned_int<RawType, raw_type_size>(s));
		typename TagType::value_type value = (
			nbt::io::detail::twos_complement_decode<raw_type_size, typename TagType::value_type, RawType>(encoded_value));
		return TagType(value);
	}

	ByteArrayTag read_byte_array_tag(InputStream &s) {
		uint32_t length = read_big_endian_unsigned_int<uint32_t, 4>(s);
		ByteArrayTag tag;
		tag.value.resize(length);
		s.read(&tag.value[0], length);
		return tag;
	}

	IntArrayTag read_int_array_tag(InputStream &s) {
		uint32_t length = read_big_endian_unsigned_int<uint32_t, 4>(s);
		IntArrayTag tag;
		tag.values.reserve(length);
		for(uint32_t idx = 0; idx < length; ++idx) {
			uint32_t encoded_int = read_big_endian_unsigned_int<uint32_t, 4>(s);
			int32_t decoded_int = twos_complement_decode<4, uint32_t, int32_t>(encoded_int);
			tag.values.push_back(decoded_int);
		}
		return tag;
	}

	template<typename FloatTagType>
	FloatTagType read_float_tag(InputStream &s) {
		FloatTagType tag;
		// NOTE: We don't have a good IEEE float decoder. This will only work
		// if your platform has IEEE floats, but that's probably the case.
		s.read(reinterpret_cast<unsigned char *>(&tag.value), sizeof(tag.value));
		return tag;
	}

	nbt::Utf8String read_string(InputStream &s) {
		uint16_t string_length = read_big_endian_unsigned_int<uint16_t, 2>(s);
		Utf8String string;
		string.data.resize(string_length);
		s.read(&string.data[0], string_length);
		return string;
	}

	StringTag read_string_tag(InputStream &s) {
		StringTag tag;
		tag.value = read_string(s);
		return tag;
	}

	std::unique_ptr<Tag> read_simple_tag(InputStream &s, TagTypeId tag_type) {
		switch(tag_type) {
			case TAG_TYPE_BYTE:
				return std::unique_ptr<Tag>(new ByteTag(read_simple_payload<ByteTag, uint8_t, 1>(s)));
			case TAG_TYPE_SHORT:
				return std::unique_ptr<Tag>(new ShortTag(read_simple_payload<ShortTag, uint16_t, 2>(s)));
			case TAG_TYPE_INT:
				return std::unique_ptr<Tag>(new IntTag(read_simple_payload<IntTag, uint32_t, 4>(s)));
			case TAG_TYPE_LONG:
				return std::unique_ptr<Tag>(new LongTag(read_simple_payload<LongTag, uint64_t, 8>(s)));
			case TAG_TYPE_FLOAT:
				return std::unique_ptr<Tag>(new FloatTag(read_float_tag<FloatTag>(s)));
			case TAG_TYPE_DOUBLE:
				return std::unique_ptr<Tag>(new DoubleTag(read_float_tag<DoubleTag>(s)));
			case TAG_TYPE_BYTE_ARRAY:
				return std::unique_ptr<Tag>(new ByteArrayTag(read_byte_array_tag(s)));
			case TAG_TYPE_STRING:
				return std::unique_ptr<Tag>(new StringTag(read_string_tag(s)));
			case TAG_TYPE_LIST:
				throw std::logic_error("read_simple_tag should not be called for TAG_TYPE_LIST.");
			case TAG_TYPE_COMPOUND:
				throw std::logic_error("read_simple_tag should not be called for TAG_TYPE_COMPOUND.");
			case TAG_TYPE_INT_ARRAY:
				return std::unique_ptr<Tag>(new IntArrayTag(read_int_array_tag(s)));
			default:
				throw IoError(std::string("Unknown tag type in NBT: ") + std::to_string(tag_type));
		}
	}

	class TagReadState;
	typedef std::vector<std::unique_ptr<TagReadState>> IoReadState;
	class TagReadState {
	public:
		virtual ~TagReadState() {};
		virtual void continue_read(InputStream &s, IoReadState &io_state) = 0;
		virtual void add_tag(std::unique_ptr<Tag> &&tag) = 0;
	protected:
		void finish_tag(std::unique_ptr<Tag> &&tag, IoReadState &io_state) {
			io_state[io_state.size() - 2]->add_tag(std::move(tag));
			io_state.pop_back();
			return;
		}
	};

	std::unique_ptr<TagReadState> new_read_state_for(InputStream &s, TagTypeId tag_type);

	class ReadRootTagState : public TagReadState {
	public:
		ReadRootTagState(RootTag &tag) : m_root_tag(tag) {}
		void continue_read(InputStream &UNUSED(s), IoReadState &io_state) {
			io_state.pop_back();
		}
		void add_tag(std::unique_ptr<Tag> &&tag) {
			m_root_tag.tag = std::move(tag);
		}
	private:
		RootTag &m_root_tag;
	};

	class ReadCompoundTagState : public TagReadState {
	public:
		ReadCompoundTagState() : m_tag(new CompoundTag()) {}

		void continue_read(InputStream &s, IoReadState &io_state) {
			TagTypeId tag_type_id = detail::read_big_endian_unsigned_int<unsigned char, 1>(s);
			if(tag_type_id == TAG_TYPE_END) {
				finish_tag(std::move(m_tag), io_state);
				return;
			}
			m_next_tag_name = read_string(s);
			if(tag_type_id == TAG_TYPE_COMPOUND || tag_type_id == TAG_TYPE_LIST) {
				io_state.push_back(new_read_state_for(s, tag_type_id));
			} else {
				std::unique_ptr<Tag> tag = read_simple_tag(s, tag_type_id);
				m_tag->values.insert(std::pair<Utf8String, std::unique_ptr<Tag>>(m_next_tag_name, std::move(tag)));
			}
		}

		void add_tag(std::unique_ptr<Tag> &&tag) {
			m_tag->values.insert(
				std::pair<Utf8String, std::unique_ptr<Tag>>(
					m_next_tag_name, std::move(tag)));
		}
	private:
		std::unique_ptr<CompoundTag> m_tag;
		Utf8String m_next_tag_name;
	};

	template<typename T>
	class ReadListTagState : public TagReadState {
	public:
		ReadListTagState(TagTypeId type_id, size_t reads) :
			m_type_id(type_id),
			m_list_tag(new ListTag<T>()),
			m_remaining_reads(reads)
		{}

		void continue_read(InputStream &s, IoReadState &io_state) {
			if(m_type_id == TAG_TYPE_COMPOUND || m_type_id == TAG_TYPE_LIST) {
				if(m_remaining_reads == 0) {
					finish_tag(std::move(m_list_tag), io_state);
				} else {
					io_state.push_back(new_read_state_for(s, m_type_id));
					--m_remaining_reads;
				}
			} else {
				// Just read everything in one pass.
				for(size_t idx = 0; idx < m_remaining_reads; ++idx) {
					add_tag(read_simple_tag(s, m_type_id));
				}
				finish_tag(std::move(m_list_tag), io_state);
			}
		}

		void add_tag(std::unique_ptr<Tag> &&tag) {
			T *real_ptr = dynamic_cast<T *>(tag.get());
			if(real_ptr == nullptr) {
				throw std::logic_error("add_tag called on ReadListTagState with a tag of a type different than the list.");
			} else {
				std::unique_ptr<T> wrapped_ptr = std::unique_ptr<T>(real_ptr);
				tag.release();
				add_typed_tag(std::move(wrapped_ptr));
			}
		}

	private:
		void add_typed_tag(std::unique_ptr<T> &&tag) {
			m_list_tag->values.push_back(std::move(tag));
		}

		TagTypeId m_type_id;
		std::unique_ptr<ListTag<T>> m_list_tag;
		size_t m_remaining_reads;
	};

	TagReadState *new_list_read_state(TagTypeId inner_tag_type, size_t length) {
		switch(inner_tag_type) {
			case TAG_TYPE_END:
				throw IoError("List tag had a tag type of \"TAG_End\".");
			case TAG_TYPE_BYTE:
				return new ReadListTagState<ByteTag>(inner_tag_type, length);
			case TAG_TYPE_SHORT:
				return new ReadListTagState<ShortTag>(inner_tag_type, length);
			case TAG_TYPE_INT:
				return new ReadListTagState<IntTag>(inner_tag_type, length);
			case TAG_TYPE_LONG:
				return new ReadListTagState<LongTag>(inner_tag_type, length);
			case TAG_TYPE_FLOAT:
				return new ReadListTagState<FloatTag>(inner_tag_type, length);
			case TAG_TYPE_DOUBLE:
				return new ReadListTagState<DoubleTag>(inner_tag_type, length);
			case TAG_TYPE_BYTE_ARRAY:
				return new ReadListTagState<ByteArrayTag>(inner_tag_type, length);
			case TAG_TYPE_STRING:
				return new ReadListTagState<StringTag>(inner_tag_type, length);
			case TAG_TYPE_LIST:
				// We don't have just a "List" type.
				return new ReadListTagState<Tag>(inner_tag_type, length);
			case TAG_TYPE_COMPOUND:
				return new ReadListTagState<CompoundTag>(inner_tag_type, length);
			case TAG_TYPE_INT_ARRAY:
				return new ReadListTagState<IntArrayTag>(inner_tag_type, length);
			default:
				throw IoError(std::string("Unknown tag type in NBT for list: ") + std::to_string(inner_tag_type));
		}
	}

	std::unique_ptr<TagReadState> new_read_state_for(InputStream &s, TagTypeId tag_type) {
		if(tag_type == TAG_TYPE_COMPOUND) {
			return std::unique_ptr<TagReadState>(new ReadCompoundTagState());
		} else if(tag_type == TAG_TYPE_LIST) {
			TagTypeId inner_tag_type = read_big_endian_unsigned_int<unsigned char, 1>(s);
			size_t length = read_big_endian_unsigned_int<uint32_t, 4>(s);
			return std::unique_ptr<TagReadState>(new_list_read_state(inner_tag_type, length));
		} else {
			throw std::logic_error(
				"new_read_state_for should not be called except for tags of"
				" type TAG_TYPE_COMPOUND and TAG_TYPE_LIST.");
		}
	}

	void process_read_state(InputStream &s, IoReadState &io_state) {
		while(!io_state.empty()) {
			io_state[io_state.size() - 1]->continue_read(s, io_state);
		}
	}
}

RootTag read_nbt(InputStream &s) {
	unsigned char tag_type_id = detail::read_big_endian_unsigned_int<unsigned char, 1>(s);
	RootTag root_tag;

	root_tag.name = detail::read_string(s);
	if(tag_type_id == TAG_TYPE_LIST || tag_type_id == TAG_TYPE_COMPOUND) {
		detail::IoReadState io_state;
		io_state.push_back(std::unique_ptr<detail::TagReadState>(new detail::ReadRootTagState(root_tag)));
		io_state.push_back(detail::new_read_state_for(s, tag_type_id));
		detail::process_read_state(s, io_state);
	}
	return root_tag;
}

}
}
