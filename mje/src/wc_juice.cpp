#include <fstream>
#include <iostream>
#include <string>

#include <map>

using namespace std;

int main(int argc, char **argv) {
    unsigned count = 0;

    if (argc < 3) {
        return 0;
    }
    string key = string(argv[2]);

    string line;
    ifstream file(argv[1]);
    if (file.is_open()) {
        while (getline(file, line)) {
            string cur_num = "";
            for (unsigned i = 0; i < line.length(); i++) {
                if (line.at(i) != ' ' && line.at(i) != '\n' && line.at(i) != '\t' &&
                    line.at(i) != '\r' && line.at(i) != '\v' && line.at(i) != '\f')
                {
                    cur_num += line.at(i);
                } else {
                    try {
                        count += std::stoi(cur_num);
                    } catch (...) {}
                    cur_num = "";
                }
            }
            if (cur_num.length() > 0) {
                try {
                    count += std::stoi(cur_num);
                } catch (...) {}
            }
        }
    }

    std::cout << key << " " << count << std::endl;
}
