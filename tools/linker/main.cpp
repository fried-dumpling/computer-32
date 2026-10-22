#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <stack>
#include <queue>
#include <map>
#include <set>
#include <functional>
#include <chrono>

#include "linker.hpp"
#include "../common/iotool.hpp"


using namespace std;
using namespace linker;

using CLI = iotools::Terminal::TerminalIO<int>;

// command-line flags (value = bit in the parsed flag set)
enum LinkFlag {
	FLAG_OUTPUT,
	FLAG_PADBSS,
	FLAG_ORG,
	FLAG_BIG,
	FLAG_LITTLE,
	FLAG_RAW,
	FLAG_START,
	FLAG_VERBOSE,
	FLAG_HELP,
	FLAG_HELP_LONG
};

int main(int argc, char* argv[]) {
	iotools::setDirectory();

	CLI terminal;
	terminal.setup({
		{ "-o",      FLAG_OUTPUT,    true,  {}, "output binary (default: a.out)" },
		{ "-padbss", FLAG_PADBSS,    false, {}, "fill .bss with zero bytes at the end of the binary" },
		{ "-org",    FLAG_ORG,       true,  {}, "address of the first output byte (default 0)" },
		{ "-big",    FLAG_BIG,       false, {}, "link big-endian objects" },
		{ "-little", FLAG_LITTLE,    false, {}, "link little-endian objects (default)" },
		{ "-raw",    FLAG_RAW,       false, {}, "write the program only, without the {start, length} header" },
		{ "-start",  FLAG_START,     true,  {}, "label whose address goes into the header (default _start)" },
		{ "-v",      FLAG_VERBOSE,   false, {}, "print a summary of the link" },
		{ "-h",      FLAG_HELP,      false, {}, "show this help" },
		{ "-help",   FLAG_HELP_LONG, false, {}, "show this help" },
	}, -1, { ' ', '\t' }, iotools::executable + " <input file1> [input file2] ... [-o <output file>] [flags]");

	iotools::u64 flags;
	std::vector<std::string> inputFiles;
	if (!terminal.parse(flags, inputFiles, argc, argv))
		return terminal.printParseError();
	if (CLI::has(flags, FLAG_HELP) || CLI::has(flags, FLAG_HELP_LONG))
		return terminal.printHelp();
	if (inputFiles.empty())
		return iotools::Terminal::usageError(iotools::Terminal::noInput(), terminal.usage());

	std::string outputFile = terminal.value(FLAG_OUTPUT, "a.out");
	linker::bssPadding = CLI::has(flags, FLAG_PADBSS);
	if (CLI::has(flags, FLAG_ORG)) {
		iotools::u32 org;
		if (!iotools::Terminal::parseNumber(terminal.value(FLAG_ORG), org))
			return iotools::Terminal::usageError(iotools::Terminal::invalidValue("-org", terminal.value(FLAG_ORG)), terminal.usage());
		linker::origin = (int)org;
	}
	if (CLI::has(flags, FLAG_BIG) && CLI::has(flags, FLAG_LITTLE))
		return iotools::Terminal::usageError("-big and -little cannot be used together", terminal.usage());
	linker::bigEndian = CLI::has(flags, FLAG_BIG);
	linker::formatted = !CLI::has(flags, FLAG_RAW);
	if (CLI::has(flags, FLAG_START)) {
		if (!linker::formatted)
			return iotools::Terminal::usageError("-start cannot be used with -raw", terminal.usage());
		linker::startLabel = terminal.value(FLAG_START);
	}

	std::vector<std::pair<std::string, std::vector<unsigned char>>> rawObjects;
	for (auto it = inputFiles.begin(); it != inputFiles.end(); ++it) {
		std::vector<unsigned char> buff;
		if (!iotools::readBinaryFile(*it, buff)) {
			iotools::Terminal::printError(iotools::Terminal::readFailed(*it));
			return iotools::Terminal::EXIT_FAIL;
		}
		rawObjects.push_back({ *it, buff });
	}

	std::vector<unsigned char> binary;
	std::string errorMessage;
	if (!link(binary, rawObjects, errorMessage)) {
		iotools::Terminal::printError(errorMessage.empty() ? "link failed" : errorMessage);
		return iotools::Terminal::EXIT_FAIL;
	}

	if (!iotools::writeBinaryFile(outputFile, binary)) {
		iotools::Terminal::printError(iotools::Terminal::writeFailed(outputFile));
		return iotools::Terminal::EXIT_FAIL;
	}

	if (CLI::has(flags, FLAG_VERBOSE))
		iotools::Terminal::printDiagnostic("linked " + std::to_string(rawObjects.size()) + " file(s) -> " + std::to_string(binary.size()) + " bytes -> " + outputFile);

	return iotools::Terminal::EXIT_OK;
}
