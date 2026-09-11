// ==========================================================================
// Log.h -- Structured logging with severity, timestamps, and categories
// ==========================================================================
// Provides LOG_DEBUG / LOG_INFO / LOG_WARN / LOG_ERROR macros that write
// to carnivor.log with [LEVEL] [file:line] prefix. Replaces ad-hoc PrintLog
// calls.

#pragma once

#include <cstdio>

enum class LogLevel {
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3
};

/// Open the log file. Called once during engine init.
void LogInit(const char* filename);

/// Write a formatted log entry with severity, file, and line info.
void LogWrite(LogLevel level, const char* file, int line, const char* fmt, ...);

/// Flush and close the log file.
void LogClose();

// --------------------------------------------------------------------------
// Convenience macros
// --------------------------------------------------------------------------
#define LOG_DEBUG(...)  LogWrite(LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)   LogWrite(LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)   LogWrite(LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)  LogWrite(LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
