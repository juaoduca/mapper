#include <stdexcept>
#include <string>
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <sstream> // To build the final string


using str = std::string;
using dml_pair = std::pair<std::string, int>;
using er = std::runtime_error;

void error(const std::string& msg, const char* file, int line, ...);
// A helper macro to automatically pass __FILE__ and __LINE__
#define THROW(msg, ...) error(msg, __FILE__, __LINE__, ##__VA_ARGS__)
