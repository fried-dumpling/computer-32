#include <iostream>
#include <fstream>
#include <vector>

#include "../common/iotool.hpp"

namespace hexdump {
    using u8 = unsigned __int8;
    using u32 = unsigned __int32;
    using u64 = unsigned __int64;

    namespace util {
        std::string convHex(u8 in, bool low) {
            std::string out;
            char alpha = (low) ? 'a' : 'A';

            for (int i = 0; i < 2; i++) {
                u8 cur = in >> 4;

                if (0 <= cur && cur <= 9)
                    out.push_back((char)cur + '0');
                else if (10 <= cur && cur <= 15)
                    out.push_back((char)cur + alpha - 10);
                else
                    return "IV";

                in <<= 4;
            }

            return out;
        }
    }

    namespace dump  {
        // hex column for in[start, start+count): `width` slots split into groups of `bundle`.
        // little-endian shows each group's bytes in reverse so a group reads as one value; slots
        // past the end of the file are blank (on the high-order side of a little-endian group).
        std::string hexColumn(const std::vector<u8>& in, size_t start, size_t count, int width, int bundle, bool little) {
            bool lower = false;
            std::string out;
            for (int group = 0; group < width; group += bundle) {
                if (group)
                    out += " ";
                int groupLen = (width - group < bundle) ? width - group : bundle;
                for (int j = 0; j < groupLen; j++) {
                    size_t index = (size_t)group + (little ? groupLen - 1 - j : j);
                    out += (index < count) ? util::convHex(in[start + index], lower) + " " : "   ";
                }
            }
            return out;
        }

        // text column in file order
        std::string textColumn(const std::vector<u8>& in, size_t start, size_t count, int bundle) {
            std::string out;
            for (size_t i = 0; i < count; i++) {
                if (i && !(i % bundle))
                    out += " ";
                u8 c = in[start + i];
                switch (c) {
                case '\n':
                    out += "\\n";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    out += std::isprint(c) ? (char)c : '.';
                    out += " ";
                    break;
                }
            }
            return out;
        }

        void dumpDecode(std::ostream& out, std::vector<u8>& in, int width, int bundle, bool little) {
            for (size_t start = 0; start < in.size(); start += width) {
                size_t count = (in.size() - start < (size_t)width) ? in.size() - start : width;
                out << hexColumn(in, start, count, width, bundle, little) << "| " << textColumn(in, start, count, bundle) << std::endl;
            }
        }

        void dumpNonDecode(std::ostream& out, std::vector<u8>& in, int width, int bundle, bool little) {
            for (size_t start = 0; start < in.size(); start += width) {
                size_t count = (in.size() - start < (size_t)width) ? in.size() - start : width;
                out << hexColumn(in, start, count, width, bundle, little) << std::endl;
            }
        }
    }
}

using namespace std;

using CLI = iotools::Terminal::TerminalIO<int>;

// command-line flags (value = bit in the parsed flag set)
enum HexFlag {
    FLAG_NODEC,
    FLAG_WIDTH,
    FLAG_BUNDLE,
    FLAG_OUTPUT,
    FLAG_BIG,
    FLAG_LITTLE,
    FLAG_HELP,
    FLAG_HELP_LONG
};

// reads a -width/-bundle value: a number from 1 to 4096
static bool readCount(const CLI& terminal, int flag, int& out) {
    std::string text = terminal.value(flag);
    iotools::u32 value;
    if (!iotools::Terminal::parseNumber(text, value) || value < 1 || value > 4096)
        return false;
    out = (int)value;
    return true;
}

int main(int argc, char* argv[]) {
    iotools::setDirectory();

    CLI terminal;
    terminal.setup({
        { "-nodec",  FLAG_NODEC,     false, {}, "hex only, no text column" },
        { "-width",  FLAG_WIDTH,     true,  {}, "bytes per line (default 4)" },
        { "-bundle", FLAG_BUNDLE,    true,  {}, "bytes per group within a line (default: width)" },
        { "-o",      FLAG_OUTPUT,    true,  {}, "write the dump to a file instead of stdout" },
        { "-big",    FLAG_BIG,       false, {}, "show each group in file order (big-endian words)" },
        { "-little", FLAG_LITTLE,    false, {}, "show each group's bytes reversed (little-endian words, default)" },
        { "-h",      FLAG_HELP,      false, {}, "show this help" },
        { "-help",   FLAG_HELP_LONG, false, {}, "show this help" },
    }, 1, { ' ', '\t' }, iotools::executable + " <file> [-o <output file>] [flags]");

    iotools::u64 flags;
    std::vector<std::string> inputs;
    if (!terminal.parse(flags, inputs, argc, argv))
        return terminal.printParseError();
    if (CLI::has(flags, FLAG_HELP) || CLI::has(flags, FLAG_HELP_LONG))
        return terminal.printHelp();
    if (inputs.empty())
        return iotools::Terminal::usageError(iotools::Terminal::noInput(), terminal.usage());

    int width = 4;
    if (CLI::has(flags, FLAG_WIDTH) && !readCount(terminal, FLAG_WIDTH, width))
        return iotools::Terminal::usageError(iotools::Terminal::invalidValue("-width", terminal.value(FLAG_WIDTH)), terminal.usage());
    int bundle = width;
    if (CLI::has(flags, FLAG_BUNDLE) && !readCount(terminal, FLAG_BUNDLE, bundle))
        return iotools::Terminal::usageError(iotools::Terminal::invalidValue("-bundle", terminal.value(FLAG_BUNDLE)), terminal.usage());
    bool nodec = CLI::has(flags, FLAG_NODEC);
    if (CLI::has(flags, FLAG_BIG) && CLI::has(flags, FLAG_LITTLE))
        return iotools::Terminal::usageError("-big and -little cannot be used together", terminal.usage());
    bool little = !CLI::has(flags, FLAG_BIG);

    vector<hexdump::u8> data;
    if (!iotools::readBinaryFile(inputs[0], data)) {
        iotools::Terminal::printError(iotools::Terminal::readFailed(inputs[0]));
        return iotools::Terminal::EXIT_FAIL;
    }

    std::ofstream file;
    std::ostream* out = &std::cout;
    if (CLI::has(flags, FLAG_OUTPUT)) {
        std::string outputFile = terminal.value(FLAG_OUTPUT);
        file.open(outputFile, std::ios::out);
        if (!file.is_open()) {
            iotools::Terminal::printError(iotools::Terminal::writeFailed(outputFile));
            return iotools::Terminal::EXIT_FAIL;
        }
        out = &file;
    }

    if (nodec)
        hexdump::dump::dumpNonDecode(*out, data, width, bundle, little);
    else
        hexdump::dump::dumpDecode(*out, data, width, bundle, little);

    return iotools::Terminal::EXIT_OK;
}
