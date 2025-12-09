#include <stdarg.h>
#include <time.h>

#include "asserts.h"
#include "colors.h"
#include "logger.h"

static FILE *output_stream = nullptr;
static logger_output_type output_type = EXTERNAL_STREAM;
static int color_enabled = 1;

static const char* logger_mode_string(const logger_mode_type type);
static void        logger_time_string(char *buff, size_t n);
static const char* logger_color_on(const logger_mode_type mode);

#if defined(WinV)
    #define localtime_r(T,Tm) (localtime_s(Tm,T) ? nullptr : Tm)
#endif

static const char* logger_mode_string(const logger_mode_type type) {
    switch (type) {
        case LOGGER_MODE_DEBUG:                   return "DEBUG";
        case LOGGER_MODE_INFO:                    return "INFO";
        case LOGGER_MODE_WARNING:                 return "WARNING";
        case LOGGER_MODE_ERROR:                   return "ERROR";
        default: SOFT_ASSERT(false, "WRONG MODE");return "?";
    }
}

static void logger_time_string(char *buff, size_t n) {
    HARD_ASSERT(buff != nullptr, "buff is nullptr");
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    if(!strftime(buff, n, "%H:%M:%S:%Y-%m-%d", &tmv)) {
        SOFT_ASSERT(false, "Invalid time input");
    }
}

static const char* logger_color_on(const logger_mode_type mode) {
    if (!color_enabled) return "";
    switch (mode) {
        case LOGGER_MODE_DEBUG:   return CYAN_CONSOLE;
        case LOGGER_MODE_INFO:    return BLUE_CONSOLE; 
        case LOGGER_MODE_WARNING: return YELLOW_CONSOLE;
        case LOGGER_MODE_ERROR:   return RED_CONSOLE;

        default:                  return "";
    }
}

void logger_initialize_stream(FILE *stream) {
    if (output_type == OWNED_FILE && output_stream) {
        fclose(output_stream);
    }
    output_stream = stream ? stream : stderr;
    output_type = EXTERNAL_STREAM;
}

int logger_initialize_file(const char *path) {
    HARD_ASSERT(path != nullptr, "File path is empty");
    FILE *f = fopen(path, "a");
    HARD_ASSERT(path != nullptr, "Incorrect read of logger file");

    if (output_type == OWNED_FILE && output_stream) {
        fclose(output_stream);
    }
    output_stream = f;
    output_type = OWNED_FILE;
    return 0;
}

void logger_close() {
    if (output_type == OWNED_FILE && output_stream) fclose(output_stream);
}

void logger_log_message(const logger_mode_type mode,
                        const char *file, int line,
                        const char *format, ...) {
    if (!output_stream) {
        color_enabled = 0;
        output_stream = stderr;
    }
    char Time[32] = "";
    logger_time_string(Time, sizeof Time);
    if(output_type != EXTERNAL_STREAM) {
        fprintf(output_stream, "%s. %s:%d. %s. ",
            Time, file, line, logger_mode_string(mode));
    } else {
        const char *color_on  = logger_color_on(mode);
        fprintf(output_stream, "[%s] %s:%d. %s%s%s. ",
            Time, file, line, color_on, logger_mode_string(mode), RESET_CONSOLE);
    }

    va_list ap;
    va_start(ap, format);
    vfprintf(output_stream, format, ap);
    va_end(ap);
    
    fputc('\n', output_stream);
}

