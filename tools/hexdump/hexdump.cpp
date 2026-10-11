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

        void dumpDecode(std::ostream& out, std::vector<u8>& in, int width, int bundle) {
            bool lower = false;

            size_t count = 0;
            std::string curHex;
            std::string curText;
            for (auto it = in.begin(); it != in.end(); ++it) {
                curHex += util::convHex(*it, lower);
                curHex += " ";

                switch (*it) {
                case '\n':
                    curText += "\\n";
                    break;
                case '\t':
                    curText += "\\t";
                    break;
                default:
                    if(std::isprint(*it))
                        curText += *it;
                    else
                        curText += ".";
                    curText += " ";
                    break;
                }
                count++;

                if (count == width) {
                    count = 0;
                    out << curHex << "| " << curText << std::endl;
                    curHex.clear();
                    curText.clear();
                }
                else {
                    if (!(count % bundle)) {
                        curHex += " ";
                        curText += " ";
                    }
                }
            }
            if (count) {
                out << curHex;
                for (int i = 0; i < width - count; i++) {
                    out << "   ";
                    if (!((i + 1) % bundle) && i + 1 < width - count) {
                        out << " ";
                    }
                }
                out << "| " << curText << std::endl;
            }
        }

        void dumpNonDecode(std::ostream& out, std::vector<u8>& in, int width, int bundle) {
            bool lower = false;

            size_t count = 0;
            std::string curHex;
            for (auto it = in.begin(); it != in.end(); ++it) {
                curHex += util::convHex(*it, lower);
                curHex += " ";
                count++;

                if (count == width) {
                    count = 0;
                    out << curHex << std::endl;
                    curHex.clear();
                }
                else {
                    if (!(count % bundle)) {
                        curHex += " ";
                    }
                }
            }
            if (count) {
                out << curHex;
                for (int i = 0; i < width - count; i++) {
                    out << "   ";
                    if (!((i + 1) % bundle) && i + 1 < width - count) {
                        out << " ";
                    }
                }
                out << std::endl;
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
        { "-h",      FLAG_HELP,      false, {}, "show this help" },
        { "-help",   FLAG_HELP_LONG, false, {}, "show this help" },
    }, 1, { ' ', '\t' }, iotools::executable + " <file> [-o <output file>] [flags]");

    int flags;
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
        hexdump::dump::dumpNonDecode(*out, data, width, bundle);
    else
        hexdump::dump::dumpDecode(*out, data, width, bundle);

    return iotools::Terminal::EXIT_OK;
}
