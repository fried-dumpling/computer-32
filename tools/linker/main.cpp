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

int main(int argc, char* argv[]) {
	iotools::setDirectory();

	if (argc < 3) {
		cout << "usage: linker <output file> <input file1> [input file2] ..." << endl;
		return -1;
	}

	const char* outputFile = argv[1];

	std::vector<std::pair<std::string, std::vector<unsigned char>>> rawObjects;
	for (int i = 2; i < argc; i++) {
		std::vector<unsigned char> buff;
		if (!iotools::readBinaryFile(argv[i], buff)) {
			cout << "failed to read input file '" << argv[i] << "'" << endl;
			return -1;
		}
		rawObjects.push_back({ argv[i], buff });
	}

	std::vector<unsigned char> binary;
	std::string errorMessage;
	if (!link(binary, rawObjects, errorMessage)) {
		if (!errorMessage.empty())
			cout << errorMessage << endl;
		cout << "link failed" << endl;
		return -1;
	}

	if (!iotools::writeBinaryFile(outputFile, binary)) {
		cout << "failed to create output file '" << outputFile << "'" << endl;
		return -1;
	}

	cout << "linked " << rawObjects.size() << " file(s) -> " << binary.size() << " bytes -> " << outputFile << endl;

	return 0;
}
