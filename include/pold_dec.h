/* 本文件解析旧版pfile，适用于matlab 2007b(不包含)之前使用pcode生成的pfile，pfile文件开头有P-file字样 */
#ifndef _POLD_DEC_H_
#define _POLD_DEC_H_
#include <string>
#include <stack>
#include <queue>
#include <map>

#define SDWORD(x) (*(int32_t*)(x))
#define SWORD(x) (*(int16_t*)(x))
#define SBYTE(x) (*(int8_t*)(x))
#define DWORD(x) (*(uint32_t*)(x))
#define WORD(x) (*(uint16_t*)(x))
#define BYTE(x) (*(uint8_t*)(x))
#define DOUBLE(x) (*(double*)(x))
#define FLOAT(x) (*(float*)(x))
#define MAKE_QWORD(x, y) (((x)<<32) | (y))

typedef std::string string;
typedef std::wstring wstring;
#define stack std::stack
#define queue std::queue
#define map std::map

extern int ptom_old_parse(char* mpath, char* ppath);


#endif