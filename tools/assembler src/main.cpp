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
#include "iotool.hpp"

using namespace std;

using namespace assembler;
using namespace assembler::lexer;
using namespace assembler::parser;
using namespace assembler::evaluator;

int main(int argc, char* argv[]) {
	iotools::setDirectory();

	assembler::createAssembler();

	if (argc < 3) {
		std::cout << "usage: assembler <input file> <output file> <-file, -token, -preproc, -eval, -bin, -double>..." << std::endl;
		return -1;
	}

	const char* inputFile = argv[1];
	const char* outputFile = argv[2];

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
	if (dumpAST)
		dump.flags |= assembler::AssemblerDump::getAST;
	if (dumpEvaluate)
		dump.flags |= assembler::AssemblerDump::getEval;
	if (dumpBinary)
		dump.flags |= assembler::AssemblerDump::getBin;

	std::vector<u8> binary;

	assembler::assemble(buff, binary, dump);

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
		cout << (dump.preprocSuccess ? "success" : "failed") << endl;
		cout << "------------------------------------------------------------" << endl;
	}

	if (dumpEvaluate || dumpBinary) {
		cout << "parse->" << endl;
		cout << (dump.parseSuccess ? "success" : "failed") << endl;
		if (!dump.parseSuccess)
			cout << dump.errorMessage << " <- here" << endl;
		cout << "------------------------------------------------------------" << endl;
		cout << "evaluate->" << endl;
		cout << (dump.evalSuccess ? "success" : "failed") << endl;
		cout << "------------------------------------------------------------" << endl;
	}

	if (dumpBinary) {
		cout << "binary->" << endl;
		cout << "------------------------------------------------------------" << endl;
		cout << "text section" << endl;
		for (auto it = dump.textSection.begin(); it != dump.textSection.end(); ) {
			for (int i = 0; it != dump.textSection.end() && i++ < 4; ++it)
				printf("%02X ", *it);
			cout << endl;
		}
		cout << "data section" << endl;
		for (auto it = dump.dataSection.begin(); it != dump.dataSection.end(); ) {
			for (int i = 0; it != dump.dataSection.end() && i++ < 4; ++it)
				printf("%02X ", *it);
			cout << endl;
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
	
	if (!iotools::writeBinaryFile(outputFile, binary)) {
		cout << "failed to create output file \'" << inputFile << "\'" << endl;
		return -1;
	}

	return 0;
}	