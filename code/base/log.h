#pragma once

#include <memory>

#include <spdlog/spdlog.h>
#include <spdlog/async.h> 
#include <spdlog/sinks/basic_file_sink.h>
#include <utility>

#define LOG_INFO(format, ...) Logger::getInstance()->async_info(format, __VA_ARGS__)
#define LOG_ERROR(format, ...) Logger::getInstance()->async_error(format, __VA_ARGS__)
#define LOG_WARN(format, ...) Logger::getInstance()->async_warn(format, __VA_ARGS__)
#define LOG_DEBUG(format, ...) Logger::getInstance()->async_debug(format, __VA_ARGS__)

class Logger {
public:
    static Logger* getInstance();

    template <typename... Args>
    void async_info(const char* format, Args &&...args) {
        m_logger->info(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void async_error(const char* format, Args &&...args) {
        m_logger->error(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void async_warn(const char* format, Args &&...args) {
        m_logger->warn(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void async_debug(const char* format, Args &&...args) {
        m_logger->debug(format, std::forward<Args>(args)...);
    }
    
private:
    Logger();
    ~Logger();

    std::shared_ptr<spdlog::logger> m_logger;
};