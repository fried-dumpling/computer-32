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
#include <algorithm>

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

	// Resolves `path` to an absolute path relative to the CURRENT WORKING DIRECTORY (i.e. the
	// directory the process was launched from) -- distinct from `directory` above, which is the
	// executable's own directory. Falls back to returning `path` unchanged if the OS call fails.
	std::string absolutePath(const std::string& path) {
		char buffer[MAX_PATH];
		DWORD len = GetFullPathNameA(path.c_str(), MAX_PATH, buffer, NULL);
		if (len == 0 || len > MAX_PATH)
			return path;
		return std::string(buffer, len);
	}

	using u8 = unsigned __int8;
	using u32 = unsigned __int32;
	using u64 = unsigned __int64;

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
		fin.read(tmp, length * sizeof(char));
		fin.close();
		
		for (size_t i = 0; i < length; i++)
			data.push_back(tmp[i]);

		delete[] tmp;
		return true;
	}

	bool writeBinaryFile(const std::string& filename, const std::vector<u8>& data) {
		std::fstream fout;
		fout.open(filename, std::ios::out | std::ios::binary);
		if (!fout.is_open() || !fout.good())
			return false;

		char* tmp = new char[data.size()];
		for (size_t i = 0; i < data.size(); i++)
			tmp[i] = data[i];

		fout.write(tmp, data.size() * sizeof(char));
		fout.close();

		delete[] tmp;
		return true;
	}

	// Source position of a code error, composed GCC-style so editors can link it:
	// (file, line, row) -> "file:line:row", (file, line) -> "file:line", (line) -> "line <n>".
	// A full message is "<position>: error: <detail>" (see Terminal::codeError).
	inline std::string errPosGenerator(const std::string& file, const std::string& line, const std::string& row) {
		return file + ":" + line + ":" + row;
	}
	inline std::string errPosGenerator(const std::string& file, const std::string& line) {
		return file + ":" + line;
	}
	inline std::string errPosGenerator(const std::string& line) {
		return "line " + line;
	}
	inline std::string errPosGenerator(const std::string& file, int line, int row) {
		return errPosGenerator(file, std::to_string(line), std::to_string(row));
	}
	inline std::string errPosGenerator(const std::string& file, int line) {
		return errPosGenerator(file, std::to_string(line));
	}
	inline std::string errPosGenerator(int line) {
		return errPosGenerator(std::to_string(line));
	}

	// Shared terminal conventions for dasm / dlnk / hexdump (see CLI.md):
	// stdout carries only requested output, stderr carries diagnostics,
	// exit codes are EXIT_OK / EXIT_FAIL / EXIT_USAGE.
	// Errors every tool can hit (command line, file IO) are built here; errors specific to
	// one program are built in that program.
	namespace Terminal {
		enum ExitCode {
			EXIT_OK = 0,      // success (including -h)
			EXIT_FAIL = 1,    // processing failed: unreadable input, assemble/link error
			EXIT_USAGE = 2    // bad command line: unknown flag, missing value, no input
		};

		// "<position>: error: <message>" -- errors located in the user's code
		inline std::string codeError(const std::string& position, const std::string& message) {
			return position + ": error: " + message;
		}

		// "<tool>: error: <message>" -- errors of the tool itself (no source position).
		// The tool name is the running executable's base name (needs setDirectory()).
		inline std::string toolError(const std::string& message) {
			return executable + ": error: " + message;
		}

		// shared error messages
		inline std::string readFailed(const std::string& path) {
			return "cannot read file '" + path + "'";
		}
		inline std::string writeFailed(const std::string& path) {
			return "cannot write file '" + path + "'";
		}
		inline std::string noInput(void) {
			return "no input file";
		}
		inline std::string invalidValue(const std::string& flag, const std::string& value) {
			return "invalid value for '" + flag + "': '" + value + "'";
		}

		// diagnostics go to stderr
		inline void printError(const std::string& message) {
			std::cerr << toolError(message) << std::endl;
		}
		inline void printDiagnostic(const std::string& text) {
			std::cerr << text << std::endl;
		}

		// prints a command-line error plus the usage text, returns EXIT_USAGE
		inline int usageError(const std::string& message, const std::string& usage) {
			printError(message);
			std::cerr << usage;
			return EXIT_USAGE;
		}

		// decimal or 0x-prefixed hexadecimal, no sign; false on empty/garbage/overflow (> 0xFFFFFFFF)
		inline bool parseNumber(const std::string& text, u32& out) {
			size_t i = 0;
			int base = 10;
			if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
				base = 16;
				i = 2;
			}
			if (i >= text.size())
				return false;

			unsigned long long value = 0;
			for (; i < text.size(); i++) {
				char c = text[i];
				int digit;
				if ('0' <= c && c <= '9') digit = c - '0';
				else if (base == 16 && 'a' <= c && c <= 'f') digit = c - 'a' + 10;
				else if (base == 16 && 'A' <= c && c <= 'F') digit = c - 'A' + 10;
				else return false;
				value = value * base + digit;
				if (value > 0xFFFFFFFFull)
					return false;
			}
			out = (u32)value;
			return true;
		}

		// Command-line parser shared by the tools.
		//  - FlagType is an enum whose values (0..63) are the flag's bit in the `flags` result.
		//  - flags and inputs may appear in any order; "--" makes every later argument an input.
		//  - a flag with `hasvalue` takes the next argument as its value, or an inline one
		//    after '=' ("-o out.o" or "-o=out.o"); read it back with value().
		//  - `follow`: flags implied by this one (setting it also sets them).
		//  - `count`: maximum number of inputs, negative = unlimited (the minimum is up to the tool).
		//  - `seperator`: characters splitting a single command-line string in parse(text);
		//    double quotes group a path containing them.
		template <class FlagType>
		class TerminalIO {
		public:
			typedef struct _FlagData {
				std::string text;
				FlagType type;
				bool hasvalue;

				std::vector<FlagType> follow;
				std::string description;   // shown by usage()
			} FlagData;

		private:
			std::vector<FlagData> flagList;
			std::map<std::string, int> flagMap;    // flag text -> index in flagList
			std::map<int, std::string> values;     // flag bit -> its value
			std::vector<char> seperator;
			std::string errorMessage;
			std::string synopsis;
			int count;

			bool fail(const std::string& message) {
				this->errorMessage = message;
				return false;
			}

		public:
			TerminalIO(void) : count(-1) {}

			bool setup(const std::vector<FlagData>& flags, const int count, std::vector<char> seperator, const std::string& synopsis = "") {
				if (flags.size() > 64)
					return false;

				this->flagList = flags;
				this->flagMap.clear();
				for (auto it = flags.cbegin(); it != flags.cend(); ++it) {
					int bit = (int)it->type;
					if (bit < 0 || bit >= 64 || this->flagMap.count(it->text))
						return false;
					this->flagMap.insert({ it->text, (int)(it - flags.cbegin()) });
				}
				this->count = count;
				this->seperator = seperator;
				this->synopsis = synopsis;

				return true;
			}

			// parses an already-split argument list (e.g. argv[1..])
			bool parse(u64& flags, std::vector<std::string>& inputs, const std::vector<std::string>& args) {
				flags = 0;
				inputs.clear();
				this->values.clear();
				this->errorMessage.clear();

				bool onlyInputs = false;
				for (auto it = args.cbegin(); it != args.cend(); ++it) {
					const std::string& arg = *it;
					if (onlyInputs || arg.size() < 2 || arg[0] != '-') {
						if (this->count >= 0 && (int)inputs.size() >= this->count)
							return fail("unexpected argument '" + arg + "'");
						inputs.push_back(arg);
						continue;
					}
					if (arg == "--") {
						onlyInputs = true;
						continue;
					}

					size_t eq = arg.find('=');
					std::string name = arg.substr(0, eq);
					auto found = this->flagMap.find(name);
					if (found == this->flagMap.end())
						return fail("unknown flag '" + name + "'");

					const FlagData& flag = this->flagList[found->second];
					if (flag.hasvalue) {
						std::string value;
						if (eq != std::string::npos)
							value = arg.substr(eq + 1);
						else if (it + 1 != args.cend())
							value = *++it;
						if (value.empty())
							return fail("missing value for '" + name + "'");
						this->values[(int)flag.type] = value;
					}
					else if (eq != std::string::npos)
						return fail("flag '" + name + "' does not take a value");

					flags |= 1ull << (int)flag.type;
					for (auto si = flag.follow.cbegin(); si != flag.follow.cend(); ++si)
						flags |= 1ull << (int)*si;
				}

				return true;
			}

			bool parse(u64& flags, std::vector<std::string>& inputs, int argc, char* argv[]) {
				return parse(flags, inputs, std::vector<std::string>(argv + 1, argv + argc));
			}

			// parses one command-line string, split on `seperator` characters
			bool parse(u64& flags, std::vector<std::string>& inputs, const std::string& text) {
				std::vector<std::string> args;
				std::string cur;
				bool quoted = false, pending = false;
				for (auto it = text.cbegin(); it != text.cend(); ++it) {
					if (*it == '"') {
						quoted = !quoted;
						pending = true;
					}
					else if (!quoted && std::find(this->seperator.cbegin(), this->seperator.cend(), *it) != this->seperator.cend()) {
						if (pending)
							args.push_back(cur);
						cur.clear();
						pending = false;
					}
					else {
						cur.push_back(*it);
						pending = true;
					}
				}
				if (quoted)
					return fail("unterminated quote");
				if (pending)
					args.push_back(cur);

				return parse(flags, inputs, args);
			}

			static bool has(u64 flags, FlagType type) {
				return (flags >> (int)type) & 1;
			}

			// value given to a `hasvalue` flag, or `fallback` if it wasn't given
			std::string value(FlagType type, const std::string& fallback = "") const {
				auto found = this->values.find((int)type);
				return (found == this->values.end()) ? fallback : found->second;
			}

			const std::string& error(void) const {
				return this->errorMessage;
			}

			// "usage: <synopsis>" followed by one line per flag
			std::string usage(void) const {
				std::string out = "usage: " + this->synopsis + "\n";
				for (auto it = this->flagList.cbegin(); it != this->flagList.cend(); ++it) {
					std::string left = "  " + it->text + (it->hasvalue ? " <value>" : "");
					if (left.size() < 20)
						left.resize(20, ' ');
					else
						left += " ";
					out += left + it->description + "\n";
				}
				return out;
			}

			// prints the usage text to stdout (for -h), returns EXIT_OK
			int printHelp(void) const {
				std::cout << usage();
				return EXIT_OK;
			}

			// prints the last parse() error plus usage to stderr, returns EXIT_USAGE
			int printParseError(void) const {
				return usageError(this->errorMessage, usage());
			}
		};
	}
}

#endif