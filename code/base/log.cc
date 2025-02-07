
#include "log.h"

#include <spdlog/spdlog.h>
#include <spdlog/async.h> 
#include <spdlog/sinks/basic_file_sink.h>

Logger::Logger() {
    // spdlog::init_thread_pool(2048, 1); // 队列大小和工作线程数
    // spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
    m_logger = spdlog::basic_logger_mt<spdlog::async_factory>("async_logger", "logs/async_log.txt");
    //m_logger->set_level(spdlog::level::trace);
}

Logger::~Logger() {
    spdlog::shutdown();
}

Logger* Logger::getInstance() {
    static Logger instance;
    return &instance;
}

// void Logger::async_info(const char* format, ...) {
//     m_logger->info(format, );
//     m_logger->info()
// }

// void Logger::async_error(const char *format, ...) {
//     m_logger->error(format);
// }

// void Logger::async_warn(const char *format, ...) {
//     m_logger->warn(format);
// }

// void Logger::async_debug(const char *format, ...) {
//     m_logger->debug(format);
// }