#include "lib.hpp"

// A safe and modern C++ variadic function for throwing exceptions.
// It now includes the file and line number of the call site.
    void error(const std::string& msg, const char* file, int line, ...) {
        // va_list is used to iterate through the variable arguments.
        va_list args;
        va_start(args, line);

        // We use a two-pass approach to safely determine the required buffer size.
        // The first call to vsnprintf with a null buffer and a zero size
        // will return the number of characters that *would have been written*
        // if the buffer was large enough. This is a common and safe pattern.
        va_list args_copy;
        va_copy(args_copy, args); // Make a copy for the second pass
        int required_size = std::vsnprintf(nullptr, 0, msg.c_str(), args_copy);
        va_end(args_copy); // Clean up the copy

        // Check for an error during sizing.
        if (required_size < 0) {
            throw std::runtime_error("Error: Failed to determine required buffer size.");
        }

        // Allocate a buffer of the exact size needed, plus one for the null terminator.
        std::vector<char> buffer(required_size + 1);

        // Second pass: Write the formatted string into the new buffer.
        std::vsnprintf(buffer.data(), buffer.size(), msg.c_str(), args);

        // Clean up the main va_list.
        va_end(args);

        // Use a stringstream to build the final error message with file and line.
        std::stringstream ss;
        ss << file << ":" << line << ": " << buffer.data();

        // Throw the exception with the correctly formatted message.
        throw std::runtime_error(ss.str());
        return;
    }
