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

int fileIO(std::string& text, std::string path) {
	if (!iotools::readTextFile(path, text)) {
		cout << "failed to read input file \'" << path << "\'" << endl;
		return -1;
	}

	return 0;
}

int main(int argc, char* argv[]) {
	iotools::setDirectory();

	assembler::createAssembler();
	assembler::fileIoFunc = fileIO;

	if (argc < 3) {
		std::cout << "usage: assembler <input file> <output file> [-file, -token, -preproc, -eval, -bin, -double] ..." << std::endl;
		return -1;
	}

	// resolved relative to the invoking shell's own current directory (not the executable's
	// directory) so included-file paths and duplicate-#include detection are stable no matter
	// where the caller's cwd is or whether they passed a relative or absolute input path.
	std::string inputFile = iotools::absolutePath(argv[1]);
	const char* outputFile = argv[2];

	// label mangling uses the output binary's own base name (no directory, no extension) so
	// same-named labels from different object files don't collide once the linker merges them
	std::string outputBase = outputFile;
	size_t slashPos = outputBase.find_last_of("/\\");
	if (slashPos != std::string::npos)
		outputBase = outputBase.substr(slashPos + 1);
	size_t dotPos = outputBase.find_last_of('.');
	if (dotPos != std::string::npos)
		outputBase = outputBase.substr(0, dotPos);

	bool dumpFile = false, dumpToken = false, dumpPreproc = false, dumpAST = false, dumpEvaluate = false, dumpBinary = false, doubleOutput = false;
	for (int i = 3; i < argc; i++) {
		std::string flag = argv[i];
		if (flag == "-file")
			dumpFile = true;
		else if (flag == "-token")
			dumpToken = true;
		else if (flag == "-preproc")
			dumpPreproc = true;
		else if (flag == "-eval")
			dumpEvaluate = true;
		else if (flag == "-bin")
			dumpBinary = true;
		else if (flag == "-double")
			doubleOutput = true;
		else {
			cout << "unknown flag \'" << flag << "\'" << endl;
			return -1;
		}
	}

	std::string buff;
	if (!iotools::readTextFile(inputFile, buff)) {
		cout << "failed to read input file \'" << inputFile << "\'" << endl;
		return -1;
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
	/*if (dumpAST)
		dump.flags |= assembler::AssemblerDump::getAST;*/
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

	if (dumpPreproc || !preprocOk) {
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
		if (!preprocOk)
			cout << dump.errorMessage << endl;
		cout << "------------------------------------------------------------" << endl;
	}

	if (dumpEvaluate || dumpBinary || !parseOk || !evalOk) {
		cout << "parse->" << endl;
		cout << (parseOk ? "success" : "failed") << endl;
		if (!parseOk)
			cout << dump.errorMessage << endl;
		cout << "------------------------------------------------------------" << endl;
		// evaluate() never runs when the parse failed (assemble() bails out right after
		// parsing) -- print it only when parsing actually succeeded, otherwise evalOk's
		// default-true value would misleadingly read as "evaluate succeeded".
		if (parseOk) {
			cout << "evaluate->" << endl;
			cout << (evalOk ? "success" : "failed") << endl;
			if (!evalOk)
				cout << dump.errorMessage << endl;
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
		std::remove(outputFile);
		return -1;
	}

	if (!iotools::writeBinaryFile(outputFile, binary)) {
		cout << "failed to create output file \'" << inputFile << "\'" << endl;
		return -1;
	}

	return 0;
}