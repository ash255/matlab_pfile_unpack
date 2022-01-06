#include "logger.h"
#include <stdarg.h>

static FILE* g_log_fp = NULL;
static int g_log_flag[__LEVEL_LAST__] = { 0 };

void log_with_fd(FILE* fp)
{
    g_log_fp = fp;
}

void log_with_file(char* file)
{
    if (file != NULL)
    {
        FILE* fp = nullptr;
        //fp = fopen(file, "w");
        fopen_s(&fp, file, "w");
        if (fp != NULL)
        {
            log_with_fd(fp);
        }
    }
}

void log_printf(char* level, char* module, char* format, ...)
{
    va_list va;
    char format_str[1024] = { 0 };

    va_start(va, format);

    if (g_log_fp != NULL)
    {
        snprintf(format_str, sizeof(format_str), "[%s][%s]:%s", level, module, format);
        vfprintf(g_log_fp, format_str, va);
        fflush(g_log_fp);
    }

    va_end(va);
}

void log_set_level(int level)
{
    if (level >= 0 && level < __LEVEL_LAST__)
    {
        g_log_flag[level] = 1;
    }
}

void log_clear_level(int level)
{
    if (level >= 0 && level < __LEVEL_LAST__)
    {
        g_log_flag[level] = 0;
    }
}

int chekc_level(int level)
{
    if (level >= 0 && level < __LEVEL_LAST__)
    {
        return g_log_flag[level];
    }
    return 0;
}

void log_printf_array(char* level, char* module, char* buffer, int len)
{
    int i;

    if (buffer != NULL && len > 0 && g_log_fp != NULL)
    {
        fprintf(g_log_fp, "[%s][%s]:<%d> - ", level, module, len);
        for (i = 0; i < len; i++)
        {
            fprintf(g_log_fp, "%02X ", (unsigned char)buffer[i]);
        }
        fprintf(g_log_fp, "\n");
        fflush(g_log_fp);
    }
}