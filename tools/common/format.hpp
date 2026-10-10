#ifndef DPICP_FORMAT
#define DPICP_FORMAT

#include <string>
#include <vector>

namespace format_data {
	using u32 = unsigned __int32;
	using u8 = unsigned __int8;
	
	enum class SectionType {
		OUTER,
		TEXT,
		DATA,
		BSS,
		ROD
	};

	enum class OperationType {
		EOX,
		IDENTIFIER,
		CONSTANT,
		ADD, SUB, MULT, DIV, MOD, POW,
		SHIFTL, SHIFTR, BND, BOR, BNT, BXR,
		LESS, GREATER, LESSEQUAL, GREATEQUAL, EQUAL, NOTEQUAL,
		LAND, LOR, LNOT,
		NEG,
		SELECT
	};

	inline void push32(std::vector<u8>& list, u32 inst) {
		union {
			u32 u32data;
			u8 u8data[4];
		} conv;

		conv.u32data = inst;
		list.push_back(conv.u8data[3]);
		list.push_back(conv.u8data[2]);
		list.push_back(conv.u8data[1]);
		list.push_back(conv.u8data[0]);
	}

	inline u32 read32(std::vector<u8>::const_iterator& it) {
		union {
			u32 u32data;
			u8 u8data[4];
		} conv;

		conv.u8data[3] = *(it++);
		conv.u8data[2] = *(it++);
		conv.u8data[1] = *(it++);
		conv.u8data[0] = *(it++);
		return conv.u32data;
	}

	typedef struct _Section {
		std::string name;
		u32 type;
		u32 start;
		u32 end;
		// address space the section occupies. Equals end - start for sections with content;
		// a .bss section has no bytes in `binary` (start == end) but a nonzero size.
		u32 size;

		void getRaw(std::vector<u8>& out) {
			out.insert(out.end(), name.begin(), name.end());
			out.push_back(0);
			push32(out, type);
			push32(out, start);
			push32(out, end);
			push32(out, size);
		}

		void loadRaw(std::vector<u8>::const_iterator& it) {
			for (; *it != 0; ++it)
				name.push_back(*it);
			++it;
			type = read32(it);
			start = read32(it);
			end = read32(it);
			size = read32(it);
		}
	} Section;

	typedef struct _Mapper {
		std::string sectionName;
		u32 byteIndex;
		u32 offset;
		u32 size;
		u32 exprIndex;

		void getRaw(std::vector<u8>& out) {
			out.insert(out.end(), sectionName.begin(), sectionName.end());
			out.push_back(0);
			push32(out, byteIndex);
			push32(out, offset);
			push32(out, size);
			push32(out, exprIndex);
		}

		void loadRaw(std::vector<u8>::const_iterator& it) {
			for (; *it != 0; ++it)
				sectionName.push_back(*it);
			++it;
			byteIndex = read32(it);
			offset = read32(it);
			size = read32(it);
			exprIndex = read32(it);
		}
	} Mapper;

	typedef struct _Operation {
		u32 type;
		u32 value;

		void getRaw(std::vector<u8>& out) {
			push32(out, type);
			push32(out, value);
		}

		void loadRaw(std::vector<u8>::const_iterator& it) {
			type = read32(it);
			value = read32(it);
		}
	} Operation;

	typedef struct _Identifier {
		std::string name;
		std::string section;
		u32 sectionIndex; // index into Format::sections, or (u32)-1 if this identifier has no section (e.g. a resolved constant)
		u32 value; // offset relative to the start of `sections[sectionIndex]` (or the constant value itself when sectionIndex == -1)

		void getRaw(std::vector<u8>& out) {
			out.insert(out.end(), name.begin(), name.end());
			out.push_back(0);
			out.insert(out.end(), section.begin(), section.end());
			out.push_back(0);
			push32(out, sectionIndex);
			push32(out, value);
		}

		void loadRaw(std::vector<u8>::const_iterator& it) {
			for (; *it != 0; ++it)
				name.push_back(*it);
			++it;
			for (; *it != 0; ++it)
				section.push_back(*it);
			++it;
			sectionIndex = read32(it);
			value = read32(it);
		}
	} Identifier;

	typedef struct _Header {
		u32 sectionsStart, sectionsEnd;
		u32 binaryStart, binaryEnd;
		u32 mapperStart, mapperEnd;
		u32 expressionsStart, expressionsEnd;
		u32 definedIdentifierStart, definedIdentifierEnd;
		u32 usedIdentifierStart, usedIdentifierEnd;

		static u32 size() {
			return 13 * (u32)sizeof(u32);
		}

		void getRaw(std::vector<u8>& out) {
			push32(out, size());
			push32(out, sectionsStart);
			push32(out, sectionsEnd);
			push32(out, binaryStart);
			push32(out, binaryEnd);
			push32(out, mapperStart);
			push32(out, mapperEnd);
			push32(out, expressionsStart);
			push32(out, expressionsEnd);
			push32(out, definedIdentifierStart);
			push32(out, definedIdentifierEnd);
			push32(out, usedIdentifierStart);
			push32(out, usedIdentifierEnd);
		}

		void loadRaw(std::vector<u8>::const_iterator& it) {
			read32(it); // stored header size, recomputed via size() on re-serialize
			sectionsStart = read32(it);
			sectionsEnd = read32(it);
			binaryStart = read32(it);
			binaryEnd = read32(it);
			mapperStart = read32(it);
			mapperEnd = read32(it);
			expressionsStart = read32(it);
			expressionsEnd = read32(it);
			definedIdentifierStart = read32(it);
			definedIdentifierEnd = read32(it);
			usedIdentifierStart = read32(it);
			usedIdentifierEnd = read32(it);
		}
	} Header;

	typedef struct _Format {
		std::vector<Section> sections;
		std::vector<u8> binary;
		std::vector<Mapper> mapper;
		std::vector<std::vector<Operation>> expressions;
		std::vector<Identifier> definedIdentifier;
		std::vector<std::string> usedIdentifier;

		void clear() {
			sections.clear();
			binary.clear();
			mapper.clear();
			expressions.clear();
			definedIdentifier.clear();
			usedIdentifier.clear();
		}

		void getRaw(std::vector<u8>& out) {
			std::vector<u8> sectionsRaw, mapperRaw, expressionsRaw, definedIdentifierRaw, usedIdentifierRaw;

			for (auto it = sections.begin(); it != sections.end(); ++it)
				it->getRaw(sectionsRaw);
			for (auto it = mapper.begin(); it != mapper.end(); ++it)
				it->getRaw(mapperRaw);
			for (auto it = expressions.begin(); it != expressions.end(); ++it) {
				for (auto si = it->begin(); si != it->end(); ++si)
					si->getRaw(expressionsRaw);
			}
			for (auto it = definedIdentifier.begin(); it != definedIdentifier.end(); ++it)
				it->getRaw(definedIdentifierRaw);
			for (auto it = usedIdentifier.begin(); it != usedIdentifier.end(); ++it) {
				usedIdentifierRaw.insert(usedIdentifierRaw.end(), it->begin(), it->end());
				usedIdentifierRaw.push_back(0);
			}

			Header header = {};
			u32 offset = Header::size();

			header.sectionsStart = offset; offset += (u32)sectionsRaw.size(); header.sectionsEnd = offset;
			header.binaryStart = offset; offset += (u32)binary.size(); header.binaryEnd = offset;
			header.mapperStart = offset; offset += (u32)mapperRaw.size(); header.mapperEnd = offset;
			header.expressionsStart = offset; offset += (u32)expressionsRaw.size(); header.expressionsEnd = offset;
			header.definedIdentifierStart = offset; offset += (u32)definedIdentifierRaw.size(); header.definedIdentifierEnd = offset;
			header.usedIdentifierStart = offset; offset += (u32)usedIdentifierRaw.size(); header.usedIdentifierEnd = offset;

			header.getRaw(out);
			out.insert(out.end(), sectionsRaw.begin(), sectionsRaw.end());
			out.insert(out.end(), binary.begin(), binary.end());
			out.insert(out.end(), mapperRaw.begin(), mapperRaw.end());
			out.insert(out.end(), expressionsRaw.begin(), expressionsRaw.end());
			out.insert(out.end(), definedIdentifierRaw.begin(), definedIdentifierRaw.end());
			out.insert(out.end(), usedIdentifierRaw.begin(), usedIdentifierRaw.end());
		}

		// Returns false (Format left cleared) if `in` is not a well-formed object: too short,
		// region offsets out of order / out of range, or a record that runs past its region.
		bool loadRaw(const std::vector<u8>& in) {
			clear();

			if (in.size() < Header::size())
				return false;
			auto it = in.cbegin();
			if (read32(it) != Header::size())
				return false;
			it = in.cbegin();
			Header header = {};
			header.loadRaw(it);

			// regions are stored back to back right after the header, covering the rest of the file
			u32 bounds[12] = { header.sectionsStart, header.sectionsEnd, header.binaryStart, header.binaryEnd,
				header.mapperStart, header.mapperEnd, header.expressionsStart, header.expressionsEnd,
				header.definedIdentifierStart, header.definedIdentifierEnd, header.usedIdentifierStart, header.usedIdentifierEnd };
			u32 prev = Header::size();
			for (int i = 0; i < 12; i += 2) {
				if (bounds[i] != prev || bounds[i + 1] < bounds[i])
					return false;
				prev = bounds[i + 1];
			}
			if (prev != in.size())
				return false;

			typedef std::vector<u8>::const_iterator CIt;
			// [it, end) holds a NUL-terminated string followed by at least `tail` bytes
			auto fits = [](CIt it, CIt end, int strings, size_t tail) {
				for (int i = 0; i < strings; i++) {
					while (it < end && *it != 0) ++it;
					if (it >= end) return false;
					++it;
				}
				return (size_t)(end - it) >= tail;
			};

			auto sectionsIt = in.cbegin() + header.sectionsStart;
			auto sectionsEndIt = in.cbegin() + header.sectionsEnd;
			while (sectionsIt < sectionsEndIt) {
				if (!fits(sectionsIt, sectionsEndIt, 1, 4 * sizeof(u32)))
					return false;
				Section s = {};
				s.loadRaw(sectionsIt);
				sections.push_back(s);
			}

			binary.assign(in.cbegin() + header.binaryStart, in.cbegin() + header.binaryEnd);
			for (auto si = sections.cbegin(); si != sections.cend(); ++si)
				if (si->start > si->end || si->end > binary.size())
					return false;

			auto mapperIt = in.cbegin() + header.mapperStart;
			auto mapperEndIt = in.cbegin() + header.mapperEnd;
			while (mapperIt < mapperEndIt) {
				if (!fits(mapperIt, mapperEndIt, 1, 4 * sizeof(u32)))
					return false;
				Mapper m = {};
				m.loadRaw(mapperIt);
				mapper.push_back(m);
			}

			auto exprIt = in.cbegin() + header.expressionsStart;
			auto exprEndIt = in.cbegin() + header.expressionsEnd;
			while (exprIt < exprEndIt) {
				std::vector<Operation> expr;
				Operation op = {};
				do {
					if (exprEndIt - exprIt < (long long)(2 * sizeof(u32)))
						return false; // ran out before the expression's EOX
					op = {};
					op.loadRaw(exprIt);
					expr.push_back(op);
				} while (op.type != (u32)OperationType::EOX);
				expressions.push_back(expr);
			}

			auto definedIt = in.cbegin() + header.definedIdentifierStart;
			auto definedEndIt = in.cbegin() + header.definedIdentifierEnd;
			while (definedIt < definedEndIt) {
				if (!fits(definedIt, definedEndIt, 2, 2 * sizeof(u32)))
					return false;
				Identifier id = {};
				id.loadRaw(definedIt);
				definedIdentifier.push_back(id);
			}

			auto usedIt = in.cbegin() + header.usedIdentifierStart;
			auto usedEndIt = in.cbegin() + header.usedIdentifierEnd;
			while (usedIt < usedEndIt) {
				if (!fits(usedIt, usedEndIt, 1, 0))
					return false;
				std::string name;
				for (; *usedIt != 0; ++usedIt)
					name.push_back(*usedIt);
				++usedIt;
				usedIdentifier.push_back(name);
			}
			return true;
		}
	} Format;
}

#endif
