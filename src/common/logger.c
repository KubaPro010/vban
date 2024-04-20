#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include "logger.h"

enum LogLevel LoggerOutputLevel = LOG_WARNING;
 
void logger_set_output_level(enum LogLevel level)
{
    LoggerOutputLevel = level;
}

void logger_log(enum LogLevel msgLevel, const char* format, ... )
{
	va_list args;
	FILE* const output = (msgLevel <= LOG_ERROR) ? stderr : stdout;

	if (msgLevel > LoggerOutputLevel) return;

    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char datetime_str[50]; // HH:MM:SS DD.MM.YYYY
    strftime(datetime_str, sizeof(datetime_str), "%H:%M:%S %d.%m.%Y", tm_now);
	
	switch (msgLevel)
	{
        case LOG_FATAL:
            fprintf(output, "%s > [FATAL]: ", datetime_str);
            break;
        case LOG_ERROR:
            fprintf(output, "%s > [ERROR]: ", datetime_str);
            break;
        case LOG_WARNING:
            fprintf(output, "%s > [WARNING]: ", datetime_str);
            break;
        case LOG_INFO:
            fprintf(output, "%s > [INFO]: ", datetime_str);
            break;
        case LOG_DEBUG:
            fprintf(output, "%s > [DEBUG]: ", datetime_str);
            break;
        default:
            return;
    }

	va_start(args, format);
	vfprintf(output, format, args);
	va_end(args);
	fprintf(output, "\n" );
}

