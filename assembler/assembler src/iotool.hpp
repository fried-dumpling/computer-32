#ifndef DPICP_IOTOOL_HPP
#define DPICP_IOTOOL_HPP

#include <Windows.h>

#include <iostream>
#include <fstream>

#include <string>
#include <vector>
#include <stack>
#include <queue>
#include <map>
#include <set>

namespace iotools {
	std::string directory;
	std::string executable;

	void setDirectory() {
		char szPath[MAX_PATH];
		if (!GetModuleFileNameA(NULL, szPath, MAX_PATH))
			return;

		std::string raw_directory(szPath);

		size_t index1 = raw_directory.rfind("\\");
		if (index1 == std::string::npos)
			index1 = raw_directory.length() - 1;
		size_t index2 = raw_directory.rfind(".");
		if (index2 == std::string::npos)
			index2 = raw_directory.length() - 1;


		directory = raw_directory.substr(0, index1+1);
		executable = raw_directory.substr(index1+1, index2-index1-1);
	}

	using u8 = unsigned __int8;
	using u32 = unsigned __int32;

	bool readTextFile(const std::string& filename, std::string& outText) {
		std::fstream fin;
		fin.open(filename, std::ios::in);
		if (!fin.is_open() || !fin.good())
			return false;

		char c;
		while (fin.get(c)) {
			outText.push_back(c);
		}
		fin.close();
		return true;
	}

	bool readBinaryFile(const std::string& filename, std::vector<u8>& data) {
		std::fstream fin;
		fin.open(filename, std::ios::in | std::ios::binary);
		if (!fin.is_open() || !fin.good())
			return false;

		if (!fin)
			return false;

		fin.seekg(0, fin.end);
		int length = (int)fin.tellg();
		fin.seekg(0, fin.beg);

		data.clear();
		char* tmp = new char[length];
		fin.read(tmp, length);
		for (size_t i = 0; i < length; i++)
			data.push_back(tmp[i]);

		fin.close();
		return true;
	}

	bool writeBinaryFile(const std::string& filename, const std::vector<u8>& data) {
		std::fstream fout;
		fout.open(filename, std::ios::out | std::ios::binary);
		if (!fout.is_open() || !fout.good())
			return false;

		for (auto it = data.cbegin(); it != data.cend(); ++it) {
			char tmp = *it;
			fout.write(&tmp, sizeof(char));
		}

		fout.close();
		return true;
	}
}

#endif