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

#include "assembler.hpp"
#include "../common/iotool.hpp"

using namespace std;

using namespace assembler;
using namespace assembler::lexer;
using namespace assembler::parser;
using namespace assembler::evaluator;

using CLI = iotools::Terminal::TerminalIO<int>;

// command-line flags (value = bit in the parsed flag set)
enum AsmFlag {
	FLAG_OUTPUT,
	FLAG_FILE,
	FLAG_TOKEN,
	FLAG_PREPROC,
	FLAG_EVAL,
	FLAG_BIN,
	FLAG_BIG,
	FLAG_LITTLE,
	FLAG_HELP,
	FLAG_HELP_LONG
};

int fileIO(std::string& text, std::string path) {
	if (!iotools::readTextFile(path, text)) {
		iotools::Terminal::printError(iotools::Terminal::readFailed(path));
		return -1;
	}

	return 0;
}

int main(int argc, char* argv[]) {
	iotools::setDirectory();

	CLI terminal;
	terminal.setup({
		{ "-o",       FLAG_OUTPUT,    true,  {}, "output object file (default: <input>.o)" },
		{ "-file",    FLAG_FILE,      false, {}, "print the input file" },
		{ "-token",   FLAG_TOKEN,     false, {}, "print the lexer tokens" },
		{ "-preproc", FLAG_PREPROC,   false, {}, "print the tokens after preprocessing" },
		{ "-eval",    FLAG_EVAL,      false, {}, "print the parse/evaluate result" },
		{ "-bin",     FLAG_BIN,       false, {}, "print the section bytes and the object file" },
		{ "-big",     FLAG_BIG,       false, {}, "emit big-endian code/data" },
		{ "-little",  FLAG_LITTLE,    false, {}, "emit little-endian code/data (default)" },
		{ "-h",       FLAG_HELP,      false, {}, "show this help" },
		{ "-help",    FLAG_HELP_LONG, false, {}, "show this help" },
	}, 1, { ' ', '\t' }, iotools::executable + " <input file> [-o <output file>] [flags]");

	iotools::u64 flags;
	std::vector<std::string> inputs;
	if (!terminal.parse(flags, inputs, argc, argv))
		return terminal.printParseError();
	if (CLI::has(flags, FLAG_HELP) || CLI::has(flags, FLAG_HELP_LONG))
		return terminal.printHelp();
	if (inputs.empty())
		return iotools::Terminal::usageError(iotools::Terminal::noInput(), terminal.usage());

	bool dumpFile = CLI::has(flags, FLAG_FILE);
	bool dumpToken = CLI::has(flags, FLAG_TOKEN);
	bool dumpPreproc = CLI::has(flags, FLAG_PREPROC);
	bool dumpEvaluate = CLI::has(flags, FLAG_EVAL);
	bool dumpBinary = CLI::has(flags, FLAG_BIN);
	if (CLI::has(flags, FLAG_BIG) && CLI::has(flags, FLAG_LITTLE))
		return iotools::Terminal::usageError("-big and -little cannot be used together", terminal.usage());
	assembler::bigEndian = CLI::has(flags, FLAG_BIG);
	std::string inputArg = inputs[0];
	std::string outputFile = terminal.value(FLAG_OUTPUT);

	assembler::createAssembler();
	assembler::fileIoFunc = fileIO;

	// resolved relative to the invoking shell's own current directory (not the executable's
	// directory) so included-file paths and duplicate-#include detection are stable no matter
	// where the caller's cwd is or whether they passed a relative or absolute input path.
	std::string inputFile = iotools::absolutePath(inputArg);

	// default output: the input path with its extension replaced by ".o"
	if (outputFile.empty()) {
		outputFile = inputArg;
		size_t extPos = outputFile.find_last_of('.');
		size_t dirPos = outputFile.find_last_of("/\\");
		if (extPos != std::string::npos && (dirPos == std::string::npos || extPos > dirPos))
			outputFile.erase(extPos);
		outputFile += ".o";
	}

	// label mangling uses the output binary's own base name (no directory, no extension) so
	// same-named labels from different object files don't collide once the linker merges them
	std::string outputBase = outputFile;
	size_t slashPos = outputBase.find_last_of("/\\");
	if (slashPos != std::string::npos)
		outputBase = outputBase.substr(slashPos + 1);
	size_t dotPos = outputBase.find_last_of('.');
	if (dotPos != std::string::npos)
		outputBase = outputBase.substr(0, dotPos);

	std::string buff;
	if (!iotools::readTextFile(inputFile, buff)) {
		iotools::Terminal::printError(iotools::Terminal::readFailed(inputArg));
		return iotools::Terminal::EXIT_FAIL;
	}

	if (dumpFile) {
		cout << "file->" << endl;
		cout << "------------------------------------------------------------" << endl;
		cout << buff << endl;
		cout << "------------------------------------------------------------" << endl;
	}

	assembler::AssemblerDump dump;
	if (dumpToken)
		dump.flags |= assembler::AssemblerDump::getToken;
	if (dumpPreproc)
		dump.flags |= assembler::AssemblerDump::getPreproc;
	if (dumpEvaluate)
		dump.flags |= assembler::AssemblerDump::getEval;
	if (dumpBinary)
		dump.flags |= assembler::AssemblerDump::getBin;

	std::vector<u8> binary;

	assembler::assemble(buff, binary, dump, inputFile, outputBase);

	bool preprocOk = !(dump.errorCode & assembler::AssemblerDump::FAIL_PREPROCESS);
	bool parseOk = !(dump.errorCode & assembler::AssemblerDump::FAIL_PARSE);
	bool evalOk = !(dump.errorCode & assembler::AssemblerDump::FAIL_EVALUATE);

	if (dumpToken) {
		cout << "lex->" << endl;
		cout << "------------------------------------------------------------" << endl;
		for (auto it = dump.tokens.begin(); it != dump.tokens.end(); it++) {
			string tmp = it->text;
			for (auto si = tmp.begin(); si != tmp.end(); ) {
				switch (*si) {
				case '\n':
					si = tmp.erase(si);
					si = tmp.insert(si, '\\');
					si++;
					si = tmp.insert(si, 'n');
					break;
				case '\t':
					si = tmp.erase(si);
					si = tmp.insert(si, '\\');
					si++;
					si = tmp.insert(si, 't');
					break;
				case ' ':
					si = tmp.erase(si);
					si = tmp.insert(si, '\'');
					si++;
					si = tmp.insert(si, ' ');
					si++;
					si = tmp.insert(si, '\'');
					break;
				default:
					si++;
					break;
				}
			}
			tmp.push_back('\t');
			cout << tmp << "; " << ((it->type == TokenType::__unknown) ? "error" : lexer::tokenStr[it->type]) << endl;
		}
		cout << "------------------------------------------------------------" << endl;
	}

	if (dumpPreproc) {
		cout << "preproc->" << endl;
		cout << "------------------------------------------------------------" << endl;
		for (auto it = dump.preprocTokens.begin(); it != dump.preprocTokens.end(); it++) {
			string tmp = it->text;
			for (auto si = tmp.begin(); si != tmp.end(); ) {
				switch (*si) {
				case '\n':
					si = tmp.erase(si);
					si = tmp.insert(si, '\\');
					si++;
					si = tmp.insert(si, 'n');
					break;
				case '\t':
					si = tmp.erase(si);
					si = tmp.insert(si, '\\');
					si++;
					si = tmp.insert(si, 't');
					break;
				case ' ':
					si = tmp.erase(si);
					si = tmp.insert(si, '\'');
					si++;
					si = tmp.insert(si, ' ');
					si++;
					si = tmp.insert(si, '\'');
					break;
				default:
					si++;
					break;
				}
			}
			tmp.push_back('\t');
			cout << tmp << "; " << ((it->type == TokenType::__unknown) ? "error" : lexer::tokenStr[it->type]) << endl;
		}
		cout << "------------------------------------------------------------" << endl;
		cout << "preproc->" << endl;
		cout << (preprocOk ? "success" : "failed") << endl;
		cout << "------------------------------------------------------------" << endl;
	}

	// a stage that never ran (an earlier one failed) is not printed, otherwise its
	// default "ok" bit would read as success; the error itself goes to stderr below
	if ((dumpEvaluate || dumpBinary) && preprocOk) {
		cout << "parse->" << endl;
		cout << (parseOk ? "success" : "failed") << endl;
		cout << "------------------------------------------------------------" << endl;
		if (parseOk) {
			cout << "evaluate->" << endl;
			cout << (evalOk ? "success" : "failed") << endl;
			cout << "------------------------------------------------------------" << endl;
		}
	}

	if (dumpBinary) {
		cout << "binary->" << endl;
		cout << "------------------------------------------------------------" << endl;
		for (auto it = dump.sectionDump.begin(); it != dump.sectionDump.end(); ++it) {
			cout << "section: " << it->name << endl;
			for (auto si = it->data.begin(); si != it->data.end();) {
				cout << "    ";
				for (int i = 0; si != it->data.end() && i++ < 4; ++si)
					printf("%02X ", *si);
				cout << endl;
			}
		}
		cout << "------------------------------------------------------------" << endl;
		cout << "final binary" << endl;
		for (auto it = binary.begin(); it != binary.end(); ) {
			for (int i = 0; it != binary.end() && i++ < 4; ++it)
				printf("%02X ", *it);
			cout << endl;
		}
		cout << "------------------------------------------------------------" << endl;
	}
	
	// on failure write nothing, and remove a stale output from an earlier run, so a build
	// script that ignores the exit code can't hand an empty/old object to the linker
	if (!preprocOk || !parseOk || !evalOk) {
		iotools::Terminal::printDiagnostic(dump.errorMessage);
		std::remove(outputFile.c_str());
		return iotools::Terminal::EXIT_FAIL;
	}

	if (!iotools::writeBinaryFile(outputFile, binary)) {
		iotools::Terminal::printError(iotools::Terminal::writeFailed(outputFile));
		return iotools::Terminal::EXIT_FAIL;
	}

	return iotools::Terminal::EXIT_OK;
}