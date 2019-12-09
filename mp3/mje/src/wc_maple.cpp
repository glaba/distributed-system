#include <fstream>
#include <iostream>
#include <string>

#include <map>

using namespace std;

string sanitize(string word) {
    string output = "";
    for (unsigned i = 0; i < word.length(); i++) {
        char cur = word.at(i);
        if ((cur >= '0' && cur <= '9') ||
            (cur >= 'a' && cur <= 'z') ||
            (cur >= 'A' && cur <= 'Z'))
        {
            output += string(1, cur);
        }
    }
    return output;
}

int main(int argc, char **argv) {
    map<string, unsigned> counts;

    string line;
    ifstream file(argv[1]);
    if (file.is_open()) {
        while (getline(file, line)) {
			string cur_word = "";
            for (unsigned i = 0; i < line.length(); i++) {
                if (line.at(i) != ' ' && line.at(i) != '\n' && line.at(i) != '\t' &&
                    line.at(i) != '\r' && line.at(i) != '\v' && line.at(i) != '\f')
                {
                    cur_word += line.at(i);
                } else {
                    counts[sanitize(cur_word)]++;
                    cur_word = "";
                }
            }
            if (cur_word.length() > 0) {
                counts[sanitize(cur_word)]++;
            }
        }
    }

    for (auto pair : counts) {
        std::cout << pair.first << " " << std::to_string(pair.second) << std::endl;
    }
}
