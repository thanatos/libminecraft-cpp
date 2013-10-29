#include <stdexcept>
#include <string>

#include "nbt.h"


namespace nbt {
namespace utility {


/* TagNameLookup is a class to look up, at compile time, the "pretty" name of
 * the type of the tag. This is something like, "TAG_Compound".
 */
template<typename T>
struct TagNameLookup;

#define TAG_NAME_LOOKUP(cls, cls_name) \
	template<> \
	struct TagNameLookup<cls> { \
		static const char * const name; \
	}; \
	const char * const TagNameLookup<cls>::name = cls_name;

TAG_NAME_LOOKUP(ByteTag, "TAG_Byte")
TAG_NAME_LOOKUP(ShortTag, "TAG_Short")
TAG_NAME_LOOKUP(IntTag, "TAG_Int")
TAG_NAME_LOOKUP(LongTag, "TAG_Long")
TAG_NAME_LOOKUP(FloatTag, "TAG_Float")
TAG_NAME_LOOKUP(DoubleTag, "TAG_Double")
TAG_NAME_LOOKUP(ByteArrayTag, "TAG_Byte_Array")
TAG_NAME_LOOKUP(StringTag, "TAG_String")
TAG_NAME_LOOKUP(ListTagBase, "TAG_List")
TAG_NAME_LOOKUP(CompoundTag, "TAG_Compound")
TAG_NAME_LOOKUP(IntArrayTag, "TAG_Int_Array")

#undef TAG_NAME_LOOKUP


/* An optional class. A wrapped class that contains a single instance of T, but
 * only sometimes.
 * This is similar to a nullable pointer.
 */

template<typename T>
class Optional {
public:
	Optional() : m_has_value(false), m_value() {}
	Optional(const T &v) : m_has_value(true), m_value(v) {}

	bool has_value() const { return m_has_value; }
	T &value() { check_value(); return m_value; }
	const T &value() const { check_value(); return m_value; }
private:
	void check_value() const { 
		if(!m_has_value) {
			throw std::logic_error("Optional didn't contain a value.");
		}
	}

	bool m_has_value;
	T m_value;
};


/* A class to pretty-print NBT data. This class itself is private, and holds
 * during-print information, such as indentation level and the stream.
 */

class PrettyPrinter {
public:
	PrettyPrinter(std::ostream &os, const std::string &indent) : m_stream(os), m_indent_count(), m_indent(indent) {}

	void pretty_print(const RootTag &root_tag) {
		print_tag(*root_tag.tag, Optional<Utf8String>(root_tag.name));
	}

private:

	void print_indent() {
		for(unsigned int i = 0; i < m_indent_count; ++i) {
			m_stream << m_indent;
		}
	}

	std::string to_string(const Utf8String &s) {
		if(s.data.empty()) {
			return "";
		} else {
			// NOTE: This is a hack.
			return std::string(reinterpret_cast<const char *>(&s.data[0]), s.data.size());
		}
	}

	template<typename T>
	void print_preamble(const Optional<Utf8String> &name) {
		print_indent();
		m_stream << TagNameLookup<T>::name;
		if(name.has_value()) {
			m_stream << "(\"" << to_string(name.value()) << "\")";
		}
		m_stream << ':';
	}

	template<typename T>
	void print_simple_tag(const T &tag, const Optional<Utf8String> &name) {
		print_preamble<T>(name);
		m_stream << ' ' << tag.value << '\n';
	}

	void print_string_tag(const StringTag &tag, const Optional<Utf8String> &name) {
		print_preamble<StringTag>(name);
		m_stream << ' ' << to_string(tag.value) << '\n';
	}

	void print_compound_tag(const CompoundTag &tag, const Optional<Utf8String> &name) {
		print_preamble<CompoundTag>(name);
		m_stream << ' ' << tag.values.size() << " entries\n";
		print_indent();
		m_stream << "{\n";
		m_indent_count += 1;
		for(const auto &entry : tag.values) {
			print_tag(*entry.second, Optional<Utf8String>(entry.first));
		}
		m_indent_count -= 1;
		print_indent();
		m_stream << "}\n";
	}

	void print_tag(const Tag &tag, const Optional<Utf8String> &name) {
		#define HANDLE_SIMPLE_TAG(tag_type, variable_name) \
			const tag_type *simple_tag_##variable_name = dynamic_cast<const tag_type *>(&tag); \
			if(simple_tag_##variable_name) { \
				print_simple_tag(*simple_tag_##variable_name, name); \
				return; \
			}
		HANDLE_SIMPLE_TAG(ByteTag, byte_tag)
		HANDLE_SIMPLE_TAG(ShortTag, short_tag)
		HANDLE_SIMPLE_TAG(IntTag, int_tag)
		HANDLE_SIMPLE_TAG(LongTag, long_tag)
		HANDLE_SIMPLE_TAG(FloatTag, float_tag)
		HANDLE_SIMPLE_TAG(DoubleTag, double_tag)

		#undef HANDLE_SIMPLE_TAG

		const StringTag *str_tag = dynamic_cast<const StringTag *>(&tag);
		if(str_tag) {
			print_string_tag(*str_tag, name);
			return;
		}

		const CompoundTag *ct = dynamic_cast<const CompoundTag *>(&tag);
		if(ct) {
			print_compound_tag(*ct, name);
			return;
		}

		throw std::runtime_error("Unknown tag class passed to print_tag!");
	}

	std::ostream &m_stream;
	unsigned int m_indent_count;
	std::string m_indent;
};


/* The public interface.
 */
void pretty_print(std::ostream &os, const RootTag &root_tag) {
	PrettyPrinter pp(os, "    ");
	pp.pretty_print(root_tag);
}

}
}
