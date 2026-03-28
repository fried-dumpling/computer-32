#include <iostream>
#include <fstream>
#include <vector>

namespace hexdump {
    using u8 = unsigned __int8;
    using u32 = unsigned __int32;
    using u64 = unsigned __int64;
    
    namespace file {
        bool readFile(std::vector<u8>& out, std::string name) {
            std::fstream file;
            file.open(name, std::ios::in | std::ios::binary);
            if (!file.is_open() || !file.good())
                return false;
            
            file.seekg(0, file.end);
            size_t length = file.tellg();
            file.seekg(0, file.beg);

            out.clear();
            char* buff = new char[length];
            file.read(buff, length);

            out.reserve(length);
            for (size_t i = 0; i < length; i++)
                out.push_back(buff[i]);

            delete[] buff;
            return true;
        }

        bool writeFile(std::vector<u8>& in, std::string name) {
            std::fstream file;
            file.open(name, std::ios::out | std::ios::binary);
            if (!file.is_open() || !file.good())
                return false;

            char* buff = new char[in.size()];
            for (size_t i = 0; i < in.size(); i++)
                buff[i] = in[i];
            
            file.write(buff, in.size());

            delete[] buff;
            return true;
        }
    }

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

        int convStr(std::string str) {
            int total = 0;
            for (auto it = str.begin(); it != str.end(); ++it) {
                total *= 10;
                if ('0' <= *it && *it <= '9')
                    total += (*it - '0');
                else
                    return -1;
            }
            return total;
        }
    }

    namespace dump  {

        void dump(std::vector<u8>& in, int width) {
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
                    count -= width;
                    std::cout << curHex << "| " << curText << std::endl;
                    curHex.clear();
                    curText.clear();
                }
            }
            if (count) {
                std::cout << curHex;
                for (int i = 0; i < width - count; i++)
                    std::cout << "   ";
                std::cout << "| " << curText << std::endl;
            }
        }
    }
}

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "too little argument" << endl;
        cout << "usage: hexdump <file> <-width num>" << endl;
        return -1;
    }

    vector<hexdump::u8> data;
    bool opened = hexdump::file::readFile(data, string(argv[1]));

    if (!opened) {
        cout << "failed to open file" << endl;
        return -1;
    }

    int width = 4;

    if (argc == 3) {
        cout << "too little argument" << endl;
        cout << "usage: hexdump <file> <-width num>" << endl;
        return -1;
    }
    else if (argc == 4) {
        if (string(argv[2]) != "-width") {
            cout << "wrong flag" << endl;
            return -1;
        }

        width = hexdump::util::convStr((argv[3]));
        if (width == -1) {
            cout << "wrong size" << endl;
            return -1;
        }
    }

    hexdump::dump::dump(data, width);
        
    return 0;
}