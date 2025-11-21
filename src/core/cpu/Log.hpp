#ifndef LOG_HPP
#define LOG_HPP

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

// Chainable logging class matching Lib65816's Log API
class Log {
private:
    std::ostringstream buffer;
    std::ostream* output;
    
public:
    Log() : output(&std::cout) {}
    
    Log(std::ostream& os) : output(&os) {}
    
    // Reset for reuse
    void reset(std::ostream& os) {
        buffer.str("");
        buffer.clear();
        output = &os;
    }
    
    // Add string
    Log& str(const std::string& s) {
        buffer << s;
        return *this;
    }
    
    Log& str(const char* s) {
        buffer << s;
        return *this;
    }
    
    // Add space
    Log& sp() {
        buffer << " ";
        return *this;
    }
    
    // Add hex value with optional width
    Log& hex(uint32_t value, int width = 0) {
        buffer << "0x" << std::hex << std::uppercase << std::setfill('0');
        if (width > 0) {
            buffer << std::setw(width);
        }
        buffer << value << std::dec;
        return *this;
    }
    
    // Add decimal number
    Log& num(int value) {
        buffer << value;
        return *this;
    }
    
    // Actually output the log
    void show() {
        if (output) {
            *output << buffer.str() << std::endl;
        }
        buffer.str("");
        buffer.clear();
    }
    
    // Static methods for each log level - return references to static instances
    static Log& err(const std::string& tag) {
        static thread_local Log instance;
        instance.reset(std::cerr);
        instance.str("[ERROR][").str(tag).str("] ");
        return instance;
    }
    
    static Log& wrn(const std::string& tag) {
        static thread_local Log instance;
        instance.reset(std::cout);
        instance.str("[WARN][").str(tag).str("] ");
        return instance;
    }
    
    static Log& inf(const std::string& tag) {
        static thread_local Log instance;
        instance.reset(std::cout);
        instance.str("[INFO][").str(tag).str("] ");
        return instance;
    }
    
    static Log& dbg(const std::string& tag) {
        static thread_local Log instance;
        instance.reset(std::cout);
        instance.str("[DEBUG][").str(tag).str("] ");
        return instance;
    }
    
    static Log& trc(const std::string& tag) {
        static thread_local Log instance;
        instance.reset(std::cout);
        instance.str("[TRACE][").str(tag).str("] ");
        return instance;
    }
    
    // Simple non-chainable helpers
    static void info(const std::string& msg) {
        std::cout << "[INFO] " << msg << std::endl;
    }
    
    static void debug(const std::string& msg) {
        std::cout << "[DEBUG] " << msg << std::endl;
    }
    
    static void warning(const std::string& msg) {
        std::cout << "[WARNING] " << msg << std::endl;
    }
    
    static void error(const std::string& msg) {
        std::cerr << "[ERROR] " << msg << std::endl;
    }
};

#endif // LOG_HPP
