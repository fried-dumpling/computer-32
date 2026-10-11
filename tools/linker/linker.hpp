#ifndef DPICP_LINKER
#define DPICP_LINKER

#include <iostream>
#include <fstream>

#include <string>
#include <vector>
#include <array>
#include <stack>
#include <queue>
#include <map>
#include <set>
#include <unordered_map>

#include "../common/format.hpp"
#include "../common/iotool.hpp"

#include "stackmachine.hpp"

namespace linker {
    // when set, mergeByType fills .bss with real zero bytes at the end of the final binary
    // (otherwise .bss only takes address space and the binary ends after .data) -- dlnk -padbss
    bool bssPadding = false;

    namespace format {
		using u32 = unsigned __int32;
		using u8 = unsigned __int8;

		using Format = format_data::Format;
		using Section = format_data::Section;
		using Operation = format_data::Operation;
		using Mapper = format_data::Mapper;
		using Identifier = format_data::Identifier;
    }

    namespace stackMachine {
        using u32 = unsigned __int32;
        using s32 = __int32;

        // Reuse format_data's operation vocabulary directly instead of duplicating it -
        // it already has CONSTANT (leaf) plus every operator the assembler can emit.
        using OpType = format_data::OperationType;

        using StackMachine = stack_machine_generator::StackMachine<OpType>;
        using Element = StackMachine::Element;
        using Function = StackMachine::StackMachineFunction;

        // Number of operands each non-leaf operator pops off the run stack.
        inline u32 arity(OpType op) {
            switch (op) {
            case OpType::BNT:
            case OpType::LNOT:
            case OpType::NEG:
                return 1;
            case OpType::SELECT:
                return 3;
            default:
                return 2;
            }
        }

        // function()'s own error codes: 0 = success.
        enum class FunctionError {
            SUCCESS = 0,
            DIV_BY_ZERO,
            MOD_BY_ZERO,
            UNKNOWN_OPERATOR // oper isn't one function() knows how to evaluate (e.g. IDENTIFIER/CONSTANT/EOX reaching here is a caller bug)
        };

        int function(u32& ret, OpType oper, const std::vector<u32>& args) {
            switch (oper) {
            case OpType::ADD: ret = (u32)((s32)args[0] + (s32)args[1]); return (int)FunctionError::SUCCESS;
            case OpType::SUB: ret = (u32)((s32)args[0] - (s32)args[1]); return (int)FunctionError::SUCCESS;
            case OpType::MULT: ret = (u32)((s32)args[0] * (s32)args[1]); return (int)FunctionError::SUCCESS;
            case OpType::DIV:
                if (args[1] == 0) return (int)FunctionError::DIV_BY_ZERO;
                ret = (u32)((s32)args[0] / (s32)args[1]);
                return (int)FunctionError::SUCCESS;
            case OpType::MOD:
                if (args[1] == 0) return (int)FunctionError::MOD_BY_ZERO;
                ret = (u32)((s32)args[0] % (s32)args[1]);
                return (int)FunctionError::SUCCESS;
            case OpType::POW: {
                s32 base = (s32)args[0];
                u32 power = args[1];
                s32 r = 1;
                for (u32 i = 0; i < power; i++)
                    r *= base;
                ret = (u32)r;
                return (int)FunctionError::SUCCESS;
            }
            case OpType::SHIFTL: ret = (u32)((s32)args[0] << (s32)args[1]); return (int)FunctionError::SUCCESS;
            case OpType::SHIFTR: ret = (u32)((s32)args[0] >> (s32)args[1]); return (int)FunctionError::SUCCESS;
            case OpType::BND: ret = args[0] & args[1]; return (int)FunctionError::SUCCESS;
            case OpType::BOR: ret = args[0] | args[1]; return (int)FunctionError::SUCCESS;
            case OpType::BXR: ret = args[0] ^ args[1]; return (int)FunctionError::SUCCESS;
            case OpType::BNT: ret = ~args[0]; return (int)FunctionError::SUCCESS;
            case OpType::LESS: ret = ((s32)args[0] < (s32)args[1]) ? 1 : 0; return (int)FunctionError::SUCCESS;
            case OpType::GREATER: ret = ((s32)args[0] > (s32)args[1]) ? 1 : 0; return (int)FunctionError::SUCCESS;
            case OpType::LESSEQUAL: ret = ((s32)args[0] <= (s32)args[1]) ? 1 : 0; return (int)FunctionError::SUCCESS;
            case OpType::GREATEQUAL: ret = ((s32)args[0] >= (s32)args[1]) ? 1 : 0; return (int)FunctionError::SUCCESS;
            case OpType::EQUAL: ret = (args[0] == args[1]) ? 1 : 0; return (int)FunctionError::SUCCESS;
            case OpType::NOTEQUAL: ret = (args[0] != args[1]) ? 1 : 0; return (int)FunctionError::SUCCESS;
            case OpType::LAND: ret = (args[0] != 0 && args[1] != 0) ? 1 : 0; return (int)FunctionError::SUCCESS;
            case OpType::LOR: ret = (args[0] != 0 || args[1] != 0) ? 1 : 0; return (int)FunctionError::SUCCESS;
            case OpType::LNOT: ret = (args[0] == 0) ? 1 : 0; return (int)FunctionError::SUCCESS;
            case OpType::NEG: ret = (u32)(-(s32)args[0]); return (int)FunctionError::SUCCESS;
            case OpType::SELECT: ret = (args[0] != 0) ? args[1] : args[2]; return (int)FunctionError::SUCCESS;
            default: return (int)FunctionError::UNKNOWN_OPERATOR;
            }
        }

        StackMachine stackMachine;

        // Runs an already-built RPN element list (see evaluate::functions::castOperation for how
        // a raw format::Operation list - terminated by EOX - becomes one of these) to a single value.
        // Returns 0 on success; any other value is an error code from function() or from the
        // StackMachine's own run() (StackMachine::RunError), propagated unchanged.
        inline int runOperation(const std::vector<Element>& elements, u32& result) {
            return stackMachine.run(elements, function, result);
        }
    }

    namespace evaluate {
        namespace functions {
			using u32 = format_data::u32;
			using u8 = format_data::u8;
			using SectionType = format_data::SectionType;
			using OperationType = format_data::OperationType;
			using Section = format_data::Section;
			using Operation = format_data::Operation;

            // A section's bytes as a list of (possibly non-contiguous) ranges into
            // EvalData::initialBin, in append order -- avoids copying the same bytes into every
            // stage of the pipeline; only the final flatten (mergeByType, into textBin/dataBin/
            // bssBin) actually materializes a contiguous buffer.
            typedef struct Indexer {
            public:
                std::vector<std::pair<u32, u32>> indexes; // {start, end} into initialBin, end exclusive

                void add(u32 start, u32 end) {
                    indexes.push_back({ start, end });
                }

                // total logical length across every range (i.e. as if they were concatenated)
                u32 size() const {
                    u32 total = 0;
                    for (auto it = indexes.cbegin(); it != indexes.cend(); ++it)
                        total += it->second - it->first;
                    return total;
                }

                // translates a logical offset `id` (0-based, as if every range were concatenated)
                // into the absolute offset into the buffer the ranges point into
                u32 get(u32 id) const {
                    u32 seen = 0;
                    for (auto it = indexes.cbegin(); it != indexes.cend(); ++it) {
                        u32 len = it->second - it->first;
                        if (id - seen < len)
                            return it->first + (id - seen);
                        seen += len;
                    }
                    return 0; // out of range
                }
            } Indexer;

            typedef struct _MapData {
                u32 byteIndex;
                u32 offset;
                u32 size;
                u32 exprIndex;
            } MapData;

            typedef struct _SectionData {
                std::string name;
                SectionType type;
                Indexer ninIndex; //indexes for initialBin for lookup
                std::unordered_map<u32, MapData> map;  // keyed by logical offset into `ninIndex` above (as if its ranges were concatenated)
                u32 size;            // address space occupied (Format::Section::size summed); exceeds ninIndex.size() only for .bss, which has no bytes

                u32 sectionOffset;   // this section's final absolute base once merged into text/data/bss (filled by mergeByType)
                std::vector<u8> calcBin; // relocated bytes, filled by a later "apply relocations" pass -- not written here
            } SectionData;

            // A section is identified by (type, name), not name alone -- an unnamed "" .data and
            // an unnamed "" .text are different sections that just happen to share a name. Rather
            // than a separate key-building function, every lookup mangles the key inline as the
            // literal string "<type>#<name>" ('#' can't appear in a source identifier, so this
            // can never collide with a real section name).

            typedef struct _PendingLabel {
                u32 sectionIdx; // index into the owning FileData::sections, or (u32)-1 if this identifier has no section (a resolved constant)
                u32 offset;      // offset within that section (or the final constant value, if sectionIdx == -1) -- not yet the final absolute address
            } PendingLabel;

            typedef struct _FileData {
                std::string name;
                std::unordered_map<std::string, u32> nameMap; // keyed by "<type>#<name>" -> index into `sections` below
                std::vector<SectionData> sections;  // in first-encountered order within this file -- use nameMap for lookup by (type, name)

                std::unordered_map<std::string, PendingLabel> pendingLabels; // identifier name -> not-yet-resolved location; mergeByType finishes these into definedLabel

                std::map<std::string, u32> definedLabel;       // resolved to final absolute addresses (filled by mergeByType)
                std::vector<std::string> usedIdentifier; // copy of Format::usedIdentifier -- an expression's IDENTIFIER operation value indexes into this
                std::vector<std::vector<Operation>> expressions;
            } FileData;

			typedef struct _EvalData {
                std::vector<u8> initialBin; //just initial combined binary for later lookup

                std::unordered_map<std::string, u32> nameMap; // keyed by "<type>#<name>" -> index into `sections` below
                std::vector<SectionData> sections; // one entry per (type, name), merged across every file, in first-encountered order (mergeByName)

                std::vector<FileData> files;

                // placement ledger, in original section order: where each merged (type, name)
                // section ended up (absolute start/end) in `totalBin`. ROD is not implemented yet,
                // so ROD/OUTER sections are skipped entirely (not merged, not placed).
                std::vector<Section> texts;
                std::vector<Section> datas;
                std::vector<Section> bsses;

                u32 textBase, dataBase, bssBase;
                std::vector<u8> totalBin;

                std::string errorMessage; // why the last pass failed (undefined/duplicate symbol), empty otherwise
			} EvalData;

            // Iterator naming, fixed by nesting depth, in every function below (and their
            // comments): it = depth 1, si = depth 2, ti = depth 3, qi = depth 4, pi = depth 5,
            // hi = depth 6 ... regardless of which container is being walked or what it's called
            // elsewhere -- the name says "how deep", not "what". This applies only to iterators
            // that actually drive a for-loop; a one-off iterator such as a `find()` result is
            // named for what it is instead (see e.g. `slotIt`/`mergedIt` below).

            // Pass 1/3: casts each input object file's Format into this file's own FileData --
            // consolidating same-(type,name) sections (multiple ".text"/".data"/... blocks
            // sharing a (type,name) are logically one section split across the source),
            // remapping its Format::mapper entries onto that consolidation, and stashing its
            // defined identifiers as not-yet-resolved PendingLabels (mergeByType finishes them).
            inline bool convertData(EvalData& data, const std::vector<std::pair<std::string, format::Format>>& objects) {
                data = {};

                // it: each input object file, in the order given
                for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
                    const format::Format& obj = it->second;

                    FileData file = {};
                    file.name = it->first;

                    // offset within this file's own consolidated section where each of obj's
                    // original Format::sections entries starts -- parallel to obj.sections
                    std::vector<u32> localBase;

                    // si: each section this file's assembler emitted, in source order
                    for (auto si = obj.sections.cbegin(); si != obj.sections.cend(); ++si) {
                        std::string mangled = std::to_string(si->type) + "#" + si->name;

                        auto slotIt = file.nameMap.find(mangled);
                        u32 idx;
                        if (slotIt == file.nameMap.end()) {
                            idx = (u32)file.sections.size();
                            file.nameMap[mangled] = idx;
                            SectionData sd = {};
                            sd.name = si->name;
                            sd.type = (SectionType)si->type;
                            file.sections.push_back(sd);
                        }
                        else
                            idx = slotIt->second;

                        SectionData& sd = file.sections[idx];
                        localBase.push_back(sd.size);
                        sd.size += si->size;

                        u32 rangeStart = (u32)data.initialBin.size();
                        data.initialBin.insert(data.initialBin.end(), obj.binary.begin() + si->start, obj.binary.begin() + si->end);
                        sd.ninIndex.add(rangeStart, (u32)data.initialBin.size());
                    }

                    // si: each relocation this file's assembler recorded, matched back to the
                    // section (ti) whose byte range it falls in
                    for (auto si = obj.mapper.cbegin(); si != obj.mapper.cend(); ++si) {
                        bool found = false;
                        for (auto ti = obj.sections.cbegin(); ti != obj.sections.cend() && !found; ++ti) {
                            if (si->sectionName != ti->name || si->byteIndex < ti->start || si->byteIndex >= ti->end)
                                continue;

                            size_t sectionIdx = (size_t)(ti - obj.sections.cbegin());
                            std::string mangled = std::to_string(ti->type) + "#" + ti->name;

                            MapData md = {};
                            md.byteIndex = localBase[sectionIdx] + (si->byteIndex - ti->start);
                            md.offset = si->offset;
                            md.size = si->size;
                            md.exprIndex = si->exprIndex;
                            file.sections[file.nameMap[mangled]].map[md.byteIndex] = md;
                            found = true;
                        }
                        if (!found)
                            return false;
                    }

                    file.expressions = obj.expressions;
                    file.usedIdentifier = obj.usedIdentifier;

                    // si: each identifier this file's assembler defined
                    for (auto si = obj.definedIdentifier.cbegin(); si != obj.definedIdentifier.cend(); ++si) {
                        PendingLabel pending = {};
                        if (si->sectionIndex == (u32)-1) {
                            pending.sectionIdx = (u32)-1;
                            pending.offset = si->value;
                        }
                        else {
                            if (si->sectionIndex >= obj.sections.size())
                                return false;
                            const Section& labelSection = obj.sections[si->sectionIndex];
                            std::string mangled = std::to_string(labelSection.type) + "#" + labelSection.name;

                            auto slotIt = file.nameMap.find(mangled);
                            if (slotIt == file.nameMap.end())
                                return false;
                            pending.sectionIdx = slotIt->second;
                            pending.offset = localBase[si->sectionIndex] + si->value;
                        }
                        file.pendingLabels[si->name] = pending;
                    }

                    data.files.push_back(file);
                }

                return true;
            }

            // Pass 2: merges same-(type,name) sections across every file (EvalData::sections),
            // in file order. Temporarily stashes, in each file's own SectionData::sectionOffset,
            // the offset within that cross-file blob where this file's contribution starts --
            // mergeByType turns it into a true absolute address once each blob's final base is
            // known.
            inline bool mergeByName(EvalData& data) {
                // it: each file, in the order convertData produced them
                for (auto it = data.files.begin(); it != data.files.end(); ++it) {
                    // si: each of that file's (already name-consolidated) sections
                    for (auto si = it->sections.begin(); si != it->sections.end(); ++si) {
                        std::string mangled = std::to_string((u32)si->type) + "#" + si->name;

                        auto slotIt = data.nameMap.find(mangled);
                        u32 idx;
                        if (slotIt == data.nameMap.end()) {
                            idx = (u32)data.sections.size();
                            data.nameMap[mangled] = idx;
                            SectionData merged = {};
                            merged.name = si->name;
                            merged.type = si->type;
                            data.sections.push_back(merged);
                        }
                        else
                            idx = slotIt->second;

                        SectionData& merged = data.sections[idx];
                        si->sectionOffset = merged.size; // offset within the cross-file blob, for now
                        merged.size += si->size;
                        // ti: each (possibly non-contiguous) range making up this file's section --
                        // just copy the small {start,end} pair, never the bytes themselves
                        for (auto ti = si->ninIndex.indexes.cbegin(); ti != si->ninIndex.indexes.cend(); ++ti)
                            merged.ninIndex.add(ti->first, ti->second);
                    }
                }

                return true;
            }

            // Pass 3: places each cross-file-merged (type, name) section into its text/data/bss
            // group (TEXT, then DATA, then BSS -- the standard layout order; ROD is not
            // implemented yet and is skipped), in EvalData::sections order (i.e. the order
            // mergeByName first encountered each (type, name) across the given files). Then
            // resolves every file's pending identifiers to their final absolute address.
            inline bool mergeByType(EvalData& data) {
                std::vector<u8> textBin, dataBin, bssBin;
                // address space taken by each group so far; bytes (textBin/...) and size only
                // differ for .bss, which reserves space without contributing bytes
                u32 textSize = 0, dataSize = 0, bssSize = 0;

                // it: each cross-file-merged (type, name) section
                for (auto it = data.sections.begin(); it != data.sections.end(); ++it) {
                    std::vector<u8>* group;
                    std::vector<Section>* placements;
                    u32* groupSize;
                    switch (it->type) {
                    case SectionType::TEXT: group = &textBin; placements = &data.texts; groupSize = &textSize; break;
                    case SectionType::DATA: group = &dataBin; placements = &data.datas; groupSize = &dataSize; break;
                    case SectionType::BSS: group = &bssBin; placements = &data.bsses; groupSize = &bssSize; break;
                    default: continue; // OUTER, or ROD (not implemented yet) -- not merged
                    }

                    it->sectionOffset = *groupSize; // group-relative for now
                    *groupSize += it->size;
                    // si: each range making up this merged section's bytes, appended in order --
                    // this is the one place the pipeline actually copies bytes (out of initialBin)
                    for (auto si = it->ninIndex.indexes.cbegin(); si != it->ninIndex.indexes.cend(); ++si)
                        group->insert(group->end(), data.initialBin.begin() + si->first, data.initialBin.begin() + si->second);
                    // .bss has no bytes of its own; pad its whole size with zeros when requested
                    if (it->type == SectionType::BSS && bssPadding)
                        group->resize(*groupSize, 0);

                    Section placement = {};
                    placement.name = it->name;
                    placement.type = (u32)it->type;
                    placement.start = it->sectionOffset;
                    placement.end = it->sectionOffset + it->size;
                    placements->push_back(placement);
                }

                data.textBase = 0;
                data.dataBase = data.textBase + textSize;
                data.bssBase = data.dataBase + dataSize;

                // it: each placement-ledger entry in each group, patched group-relative -> absolute
                for (auto it = data.texts.begin(); it != data.texts.end(); ++it) { it->start += data.textBase; it->end += data.textBase; }
                for (auto it = data.datas.begin(); it != data.datas.end(); ++it) { it->start += data.dataBase; it->end += data.dataBase; }
                for (auto it = data.bsses.begin(); it != data.bsses.end(); ++it) { it->start += data.bssBase; it->end += data.bssBase; }

                // it: each cross-file-merged (type, name) section, patched group-relative -> absolute
                for (auto it = data.sections.begin(); it != data.sections.end(); ++it) {
                    switch (it->type) {
                    case SectionType::TEXT: it->sectionOffset += data.textBase; break;
                    case SectionType::DATA: it->sectionOffset += data.dataBase; break;
                    case SectionType::BSS: it->sectionOffset += data.bssBase; break;
                    default: break;
                    }
                }

                data.totalBin.clear();
                data.totalBin.insert(data.totalBin.end(), textBin.begin(), textBin.end());
                data.totalBin.insert(data.totalBin.end(), dataBin.begin(), dataBin.end());
                data.totalBin.insert(data.totalBin.end(), bssBin.begin(), bssBin.end());

                // it: each file
                for (auto it = data.files.begin(); it != data.files.end(); ++it) {
                    // si: each of that file's sections -- patch its "offset within the cross-file
                    // blob" (from mergeByName) up to a true absolute address, now that each
                    // cross-file blob's own final base (above) is known
                    for (auto si = it->sections.begin(); si != it->sections.end(); ++si) {
                        std::string mangled = std::to_string((u32)si->type) + "#" + si->name;
                        auto mergedIt = data.nameMap.find(mangled);
                        if (mergedIt == data.nameMap.end())
                            continue; // OUTER / ROD -- never placed, leave its offset as-is
                        si->sectionOffset += data.sections[mergedIt->second].sectionOffset;
                    }

                    // si: each identifier this file defined, still pending resolution from convertData
                    for (auto si = it->pendingLabels.cbegin(); si != it->pendingLabels.cend(); ++si) {
                        if (si->second.sectionIdx == (u32)-1)
                            it->definedLabel[si->first] = si->second.offset;
                        else
                            it->definedLabel[si->first] = it->sections[si->second.sectionIdx].sectionOffset + si->second.offset;
                    }
                }

                return true;
            }

            // Converts one full RPN expression (as stored in Format::expressions, terminated by an EOX
            // operation) into a stackMachine::Element list. `resolvedIdentifiers[i]` supplies the address
            // for the i-th entry of Format::usedIdentifier (an IDENTIFIER operation's value is that index).
            inline bool castOperation(const std::vector<format::Operation>& operation, const std::vector<u32>& resolvedIdentifiers, std::vector<stackMachine::Element>& elements) {
                using OpType = stackMachine::OpType;

                elements.clear();

                for (auto it = operation.cbegin(); it != operation.cend(); ++it) {
                    OpType type = (OpType)it->type;
                    if (type == OpType::EOX)
                        break;

                    stackMachine::Element el = {};
                    if (type == OpType::IDENTIFIER) {
                        if (it->value >= resolvedIdentifiers.size())
                            return false;
                        el.type = OpType::CONSTANT;
                        el.value = resolvedIdentifiers[it->value];
                    }
                    else if (type == OpType::CONSTANT) {
                        el.type = OpType::CONSTANT;
                        el.value = it->value;
                    }
                    else {
                        el.type = type;
                        el.value = stackMachine::arity(type);
                    }
                    elements.push_back(el);
                }

                return true;
            }

            // Final pass: evaluates every relocation recorded during assembly (Format::mapper,
            // carried per-file as SectionData::map after convertData) via castOperation +
            // stackMachine, and patches the resolved value into data.totalBin. Runs after
            // mergeByType, since every section's sectionOffset (and every identifier's
            // definedLabel) must already be a final absolute address/value.
            inline bool mapExpr(EvalData& data) {
                // it: each file
                for (auto it = data.files.begin(); it != data.files.end(); ++it) {
                    // this file's usedIdentifier[i] resolved to a final global address --
                    // looked up across every file's definedLabel (cross-file symbol resolution)
                    std::vector<u32> resolvedIdentifiers(it->usedIdentifier.size(), 0);

                    // si: each name this file's expressions might reference
                    for (auto si = it->usedIdentifier.cbegin(); si != it->usedIdentifier.cend(); ++si) {
                        bool found = false;
                        // ti: each file, searched for a matching symbol definition
                        for (auto ti = data.files.cbegin(); ti != data.files.cend() && !found; ++ti) {
                            auto labelIt = ti->definedLabel.find(*si);
                            if (labelIt == ti->definedLabel.end())
                                continue;
                            resolvedIdentifiers[(size_t)(si - it->usedIdentifier.cbegin())] = labelIt->second;
                            found = true;
                        }
                        if (!found) {
                            data.errorMessage = "undefined symbol '" + *si + "' (used in '" + it->name + "')";
                            return false;
                        }
                    }

                    // si: each of that file's sections
                    for (auto si = it->sections.begin(); si != it->sections.end(); ++si) {
                        // ti: each relocation recorded for this section, keyed by its logical offset within si->ninIndex
                        for (auto ti = si->map.cbegin(); ti != si->map.cend(); ++ti) {
                            const MapData& reloc = ti->second;
                            if (reloc.exprIndex >= it->expressions.size())
                                return false;

                            std::vector<stackMachine::Element> elements;
                            if (!castOperation(it->expressions[reloc.exprIndex], resolvedIdentifiers, elements))
                                return false;

                            u32 value = 0;
                            if (stackMachine::runOperation(elements, value) != 0)
                                return false;

                            u32 bytePos = si->sectionOffset + ti->first;
                            if (bytePos >= data.totalBin.size())
                                return false;

                            // patch `reloc.size` bits of `value`, LSB at bit `reloc.offset` of
                            // the byte at bytePos, extending toward earlier bytes for a wider
                            // field -- mirrors the assembler's own registerReplace bit layout
                            u32 bitsLeft = reloc.size;
                            u32 bitOffset = reloc.offset;
                            u32 shifted = value;
                            while (bitsLeft > 0) {
                                u32 bitsInThisByte = 8 - bitOffset;
                                if (bitsInThisByte > bitsLeft)
                                    bitsInThisByte = bitsLeft;

                                u8 mask = (u8)(((1u << bitsInThisByte) - 1) << bitOffset);
                                u8 bits = (u8)((shifted << bitOffset) & mask);
                                data.totalBin[bytePos] = (u8)((data.totalBin[bytePos] & ~mask) | bits);

                                shifted >>= bitsInThisByte;
                                bitsLeft -= bitsInThisByte;
                                bitOffset = 0;

                                if (bitsLeft > 0) {
                                    if (bytePos == 0)
                                        return false; // field runs off the start of the binary
                                    bytePos--;
                                }
                            }
                        }
                    }
                }

                return true;
            }
            
            // global symbols are exported by plain name, so the same name defined in two files is
            // ambiguous; file-local names carry their file's prefix and never collide this way
            inline bool checkDuplicateSymbols(EvalData& data) {
                std::unordered_map<std::string, std::string> owner; // name -> defining file
                // it: each file, si: each name it defines
                for (auto it = data.files.cbegin(); it != data.files.cend(); ++it) {
                    for (auto si = it->definedLabel.cbegin(); si != it->definedLabel.cend(); ++si) {
                        auto found = owner.find(si->first);
                        if (found != owner.end()) {
                            data.errorMessage = "duplicate symbol '" + si->first + "' in '" + found->second + "' and '" + it->name + "'";
                            return false;
                        }
                        owner[si->first] = it->name;
                    }
                }
                return true;
            }

            inline bool evaluate(EvalData& data, const std::vector<std::pair<std::string, format::Format>>& objects) {
                if (!convertData(data, objects)) return false;
                if (!mergeByName(data)) return false;
                if (!mergeByType(data)) return false;
                if (!checkDuplicateSymbols(data)) return false;
                if (!mapExpr(data)) return false;

                return true;
            }
        }
    }

    using u8 = unsigned __int8;

    // Links the given raw object files (each one a serialized format::Format, as produced by
    // the assembler's format::getRaw) into a single final binary. No logging -- just evaluates
    // the pipeline (convertData -> mergeByName -> mergeByType -> mapExpr) and returns the result.
    // errorMessage is set when an input isn't a valid object file
    inline bool link(std::vector<u8>& outBinary, const std::vector<std::pair<std::string, std::vector<u8>>>& rawObjects, std::string& errorMessage) {
        std::vector<std::pair<std::string, format::Format>> objects;

        // it: each raw object file's bytes
        for (auto it = rawObjects.cbegin(); it != rawObjects.cend(); ++it) {
            format::Format obj = {};
            if (!obj.loadRaw(it->second)) {
                errorMessage = "invalid object file '" + it->first + "'";
                return false;
            }
            objects.push_back({ it->first, obj });
        }

        evaluate::functions::EvalData data;
        if (!evaluate::functions::evaluate(data, objects)) {
            errorMessage = data.errorMessage;
            return false;
        }

        outBinary = data.totalBin;
        return true;
    }
}

#endif