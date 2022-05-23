/* 本文件解析旧版pfile，适用于matlab 2007b(不包含)之前使用pcode生成的pfile，pfile文件开头有P-file字样 */
#include "pold_dec.h"
#include "logger.h"
#include "slot.h"
#include "pstring.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define __PFILE_MAGIC "P-file  "
#define LOCAL_ID(id) ((id)-3)

//定义字串属性，用于恢复m代码
//运算符优先级参考：https://ww2.mathworks.cn/help/matlab/matlab_prog/operator-precedence.html
#define __NONE					(0x00)
#define __KEYWORD				(0x01)	//关键词，如break、continue
#define __FUNC					(0x02)	//函数名称，由function_table提供
#define __EXPRESSION			(0x04)	//表达式，由词根与函数或运算符组合而成
#define __CONST					(0x08)	//const常量
#define __OPERATOR				(0x10)	//运算符，如+、-
#define __NO_SEMI				(0x20)	//无分号结尾
#define __VARIABLE				(0x40)	//val变量

#define __CODE_HEAD				(0x100)	//标识代码块开头，例如function、if
#define __CODE_END				(0x200)	//标志代码块结束，例如end
#define __CODE_LINE				(0x400)	//标志代码块换行

#define __KEYWORD_HEAD			(__CODE_HEAD)			//代码块开头，如if、while
#define __KEYWORD_END			(__CODE_END)			//代码块结束，如end
#define __KEYWORD_BRANCH		(__KEYWORD_HEAD | __KEYWORD_END)	//既是函数开头又是函数结尾，如else、elseif

#define __OPERATOR0				( __OPERATOR | 0 )	//0级运算符，优先级最高，圆括号 ()
#define __OPERATOR1				( __OPERATOR | 1 )	//1级运算符，转置 (.')、幂 (.^)、复共轭转置 (')、矩阵幂 (^)
#define __OPERATOR2				( __OPERATOR | 2 )	//2级运算符，带一元减法 (.^-)、一元加法 (.^+) 或逻辑求反 (.^~) 的幂，以及带一元减法 (^-)、一元加法 (^+) 或逻辑求反 (^~) 的矩阵幂。
#define __OPERATOR3				( __OPERATOR | 3 )	//3级运算符，一元加法 (+)、一元减法 (-)、逻辑求反 (~)
#define __OPERATOR4				( __OPERATOR | 4 )	//4级运算符，乘法 (.*)、右除 (./)、左除 (.\)、矩阵乘法 (*)、矩阵右除 (/)、矩阵左除 (\)
#define __OPERATOR5				( __OPERATOR | 5 )	//5级运算符，加法 (+)、减法 (-)
#define __OPERATOR6				( __OPERATOR | 6 )	//6级运算符，冒号运算符 (:)
#define __OPERATOR7				( __OPERATOR | 7 )	//7级运算符，小于 (<)、小于或等于 (<=)、大于 (>)、大于或等于 (>=)、等于 (==)、不等于 (~=)
#define __OPERATOR8				( __OPERATOR | 8 )	//8级运算符，按元素 AND (&)
#define __OPERATOR9				( __OPERATOR | 9 )	//9级运算符，按元素 OR (|)
#define __OPERATOR10			( __OPERATOR | 10 )	//10级运算符，短路 AND (&&)
#define __OPERATOR11			( __OPERATOR | 11 )	//11级运算符，优先级最低，短路 OR (||)

#define __CHECK_FLAG__(FLAG, MASK)  (((FLAG) & (MASK))!=0)

struct lines_t
{
	int start_adr;
	int end_adr;
};

struct pfile_t
{
#define __PFILE_HEADER_SIZE 34
#pragma pack(1)
	char flag;
	char version_string[12];
	char unnamed;
	uint32_t pcode_size;
	uint32_t name_size;			//function name size
	uint32_t num_locals;		//number of local vals
	uint32_t checksum;			//size filed checksum
	uint32_t num_children;		//number of children
#pragma pack()
	float version;				//convert from version_string

	uint32_t const_size;		//constant table size, ver>1.1
	uint32_t func_size;			//symbolic function reference table size, ver>1.3
	uint32_t info_size;			//import information size, ver>3.0

	uint8_t verify_data[64];	//matlab verify data, ver>1.5

	uint32_t uk3;				//ver>2.3 && ver<2.8
	uint32_t uk4;				//ver>2.4 && ver<2.8
	uint32_t uk5;				//ver>2.5
	uint32_t uk6;				//ver>2.8
	
	char* func_name;
	uint8_t* pcode_data;

	uint8_t* func_data;			//ver>1.3
	uint8_t* const_data;		//ver>1.1
	uint8_t* uk4_data;			//ver>2.3 && ver<2.8
	uint8_t* info_data;			//ver>3.0

	struct pfile_t** child;		//p文件嵌套时使用

	uint32_t num_input_args;
	uint32_t* input_args_idx;
	uint32_t num_output_args;
	uint32_t* output_args_idx;

	struct slot_t* locals_slot;	//本地变量定义表
	uint32_t* locals_type;
};

enum pcode_ins_type
{
	PCODE_NOOP = 0,
	PCODE_IMAGCONST,
	PCODE_REALCONST,
	PCODE_CHARCONST,
	PCODE_ARRAYCONST,
	PCODE_STACKSW1,
	PCODE_STACKSW2,
	PCODE_MARKSTACK,
	PCODE_EATSTACKMARK,
	PCODE_COUNTSTACKFCN,
	PCODE_INDEXPROD,
	PCODE_JUMP,
	PCODE_IFJUMP,
	PCODE_FOROP,
	PCODE_WHILEOP,
	PCODE_ENDOP,
	PCODE_RETURNOP,
	PCODE_BREAKOP,
	PCODE_NORMALEXIT,
	PCODE_SWITCHOP,
	PCODE_CASEOP,
	PCODE_CASEEXP,
	PCODE_FACMAT,
	PCODE_ADDTOROW,
	PCODE_ENDROW,
	PCODE_MATEND,
	PCODE_FACCELL,
	PCODE_MATREF1,
	PCODE_MATREF2,
	PCODE_MATREF3,
	PCODE_MATREF4,
	PCODE_MATREF5,
	PCODE_MATREF6,
	PCODE_MATREFND,
	PCODE_STRUCTREF,
	PCODE_CELLREF,
	PCODE_CMPLXREF,
	PCODE_MATRIXREFERENCE,
	PCODE_DOTREFERENCE,
	PCODE_MATASS1,
	PCODE_MATASS2,
	PCODE_MATASS3,
	PCODE_MATASS4,
	PCODE_MATASS5,
	PCODE_MATASS6,
	PCODE_MATASS7,
	PCODE_MATASS7P,
	PCODE_MATASSND,
	PCODE_STRUCTASS,
	PCODE_CELLASS,
	PCODE_CMPLXASS,
	PCODE_MATRIXASSIGNMENT,
	PCODE_MATASS7CE,
	PCODE_DOTASSIGNMENT,
	PCODE_PUTLHSONSTACK,
	PCODE_LHSDOTNAMECLEANUP,
	PCODE_EXFILE,
	PCODE_EXTWORD,
	PCODE_MATOP,
	PCODE_RETURNMATOP,
	PCODE_MMEX,
	PCODE_PRINTMAT,
	PCODE_RETSET,
	PCODE_LINENO,
	PCODE_LINESTRING,
	PCODE_PUSH,
	PCODE_POP,
	PCODE_DUP,
	PCODE_POPDUP,
	PCODE_EATDUP,
	PCODE_POPSCALAREAL,
	PCODE_COMPEND,
	PCODE_INIT2RHSUNASSIGNED,
	PCODE_TRYCATCH,
	PCODE_SET2UNASSIGNED,
	PCODE_CONSTREF,
	PCODE_FCNNAME,
	PCODE_INTFCN,
	PCODE_EXTFCN,
	PCODE_MATLABFCN,
	PCODE_DYNAMICREF,
	PCODE_MXCHARCONST,
	PCODE_MXARRAYCONST,
	PCODE_SCRIPTREFERENCE,
	PCODE_SCRIPTASSIGNMENT,
	PCODE_SCRIPTPUSH,
	PCODE_MATREFNDCX,
	PCODE_MATASSNDCX,
	PCODE_CONTINUEOP,
	PCODE_GPOINTERFCN,
	PCODE_GPOINTERFCNEVAL,
	PCODE_CMPLXDYNAMICREF,
	PCODE_RHSDOTNAMECLEANUP,
	PCODE_DEFUNCTHOTCODE,
	PCODE_MXMARSHALCONST,
	PCODE_SETEMPTYFLAG,
	PCODE_IF0,
	PCODE_IF1,
	PCODE_MX2BOOL,
	PCODE_SMX2BOOL,
	PCODE_CHECKSWITCH,
	PCODE_SWITCHCMP,
	PCODE_UPLEVEL,
	PCODE_IDIOM,
	PCODE_ENDIDIOM,
	PCODE_IDIOMVAR,
	PCODE_IDIOMCONST,
	PCODE_MATRIXREFERENCENLHS,
	PCODE_LAST
};

struct pcode_ins_t
{
	char* ins;	//instruction string
	char* ops;	//operand type string
};

struct insn_t
{
	int type;
	int adr;
	void* ops[7];
	struct insn_t *prev;
	struct insn_t *next;
	int status;
};

const struct pcode_ins_t g_pcode_ins[108] =
{
	{ "NOOP", "" },
	{ "IMAGCONST", "R" },
	{ "REALCONST", "R" },
	{ "CHARCONST", "C" },
	{ "ARRAYCONST", "C" },
	{ "STACKSW1", "" },
	{ "STACKSW2", "" },
	{ "MARKSTACK", "" },
	{ "EATSTACKMARK", "" },
	{ "COUNTSTACKFCN", "" },
	{ "INDEXPROD", "BMI" },
	{ "JUMP", "O" },
	{ "IFJUMP", "O" },
	{ "FOROP", "MO" },
	{ "WHILEOP", "O" },
	{ "ENDOP", "" },
	{ "RETURNOP", "" },
	{ "BREAKOP", "" },
	{ "NORMALEXIT", "!" },
	{ "SWITCHOP", "WI" },
	{ "CASEOP", "" },
	{ "CASEEXP", "I" },
	{ "FACMAT", "" },
	{ "ADDTOROW", "" },
	{ "ENDROW", "" },
	{ "MATEND", "" },
	{ "FACCELL", "" },
	{ "MATREF1", "M" },
	{ "MATREF2", "M" },
	{ "MATREF3", "M" },
	{ "MATREF4", "M" },
	{ "MATREF5", "M" },
	{ "MATREF6", "M" },
	{ "MATREFND", "MI" },
	{ "STRUCTREF", "I" },
	{ "CELLREF", "I" },
	{ "CMPLXREF", "MI" },
	{ "MATRIXREFERENCE", "" },
	{ "DOTREFERENCE", "S" },
	{ "MATASS1", "M" },
	{ "MATASS2", "M" },
	{ "MATASS3", "M" },
	{ "MATASS4", "M" },
	{ "MATASS5", "M" },
	{ "MATASS6", "M" },
	{ "MATASS7", "M" },
	{ "MATASS7P", "M" },
	{ "MATASSND", "MI" },
	{ "STRUCTASS", "I" },
	{ "CELLASS", "I" },
	{ "CMPLXASS", "MI" },
	{ "MATRIXASSIGNMENT", "" },
	{ "MATASS7CE", "MI" },
	{ "DOTASSIGNMENT", "S" },
	{ "PUTLHSONSTACK", "IM" },
	{ "LHSDOTNAMECLEANUP", "" },
	{ "EXFILE", "SII" },
	{ "EXTWORD", "" },
	{ "MATOP", "III" },
	{ "RETURNMATOP", "III" },
	{ "MMEX", "IIIIS" },
	{ "PRINTMAT", "" },
	{ "RETSET", "I" },
	{ "LINENO", "I" },
	{ "LINESTRING", "IL" },
	{ "PUSH", "M" },
	{ "POP", "" },
	{ "DUP", "" },
	{ "POPDUP", "" },
	{ "EATDUP", "" },
	{ "POPSCALAREAL", "" },
	{ "COMPEND", "" },
	{ "INIT2RHSUNASSIGNED", "M" },
	{ "TRYCATCH", "II" },
	{ "SET2UNASSIGNED", "M" },
	{ "CONSTREF", "K" },
	{ "FCNNAME", "IS" },
	{ "INTFCN", "XII" },
	{ "EXTFCN", "IIIS" },
	{ "MATLABFCN", "FIII" },
	{ "DYNAMICREF", "FIIIM" },
	{ "MXCHARCONST", "C" },
	{ "MXARRAYCONST", "C" },
	{ "SCRIPTREFERENCE", "I" },
	{ "SCRIPTASSIGNMENT", "" },
	{ "SCRIPTPUSH", "M" },
	{ "MATREFNDCX", "I" },
	{ "MATASSNDCX", "I" },
	{ "CONTINUEOP", "" },
	{ "GPOINTERFCN", "FI" },
	{ "GPOINTERFCNEVAL", "S" },
	{ "CMPLXDYNAMICREF", "FIIIIMI" },
	{ "RHSDOTNAMECLEANUP", "" },
	{ "DEFUNCTHOTCODE", "IO" },
	{ "MXMARSHALCONST", "m" },
	{ "SETEMPTYFLAG", "" },
	{ "IF0", "O" },
	{ "IF1", "O" },
	{ "MX2BOOL", "" },
	{ "SMX2BOOL", "" },
	{ "CHECKSWITCH", "" },
	{ "SWITCHCMP", "M" },
	{ "UPLEVEL", "I" },
	{ "IDIOM", "IO" },
	{ "ENDIDIOM", "IIIIO" },
	{ "IDIOMVAR", "II" },
	{ "IDIOMCONST", "IK" },
	{ "MATRIXREFERENCENLHS", "I" },
};

static const char g_indented_character[] = "    ";	//定义缩进所用的字符

static void decode(uint8_t* data, int size, uint32_t init)
{
	int i;
	uint32_t mask;

	if (init == 0xBB)
		mask = 0x3800B;
	else if (init == 0x74)
		mask = 0xB537;
	else
		return;				//plain code

	for (i = 0; i < size; i++)
	{
		mask = (0x2455 * mask + 0xC091) % 0x38F40;
		data[i] ^= mask;
	}
}

static uint8_t check_size(struct pfile_t* pfile)
{
	uint32_t xor_val = pfile->pcode_size ^ pfile->name_size ^ pfile->num_locals ^ pfile->checksum;
	uint8_t* pxor = (uint8_t*)&xor_val;
	return pxor[0] ^ pxor[1] ^ pxor[2] ^ pxor[3] ^ 0xA9;
}

static void free_pfile(struct pfile_t* pfile)
{
	int i;

	if (pfile->func_name != nullptr)
		free(pfile->func_name);
	if (pfile->pcode_data != nullptr)
		free(pfile->pcode_data);
	if (pfile->func_data != nullptr)
		free(pfile->func_data);
	if (pfile->const_data != nullptr)
		free(pfile->const_data);
	if (pfile->uk4_data != nullptr)
		free(pfile->uk4_data);
	if (pfile->info_data != nullptr)
		free(pfile->info_data);
	if (pfile->locals_type != nullptr)
		free(pfile->locals_type);
	if (pfile->locals_slot != nullptr)
		slot_free(pfile->locals_slot);
	if (pfile->input_args_idx != nullptr)
		free(pfile->input_args_idx);
	if (pfile->output_args_idx != nullptr)
		free(pfile->output_args_idx);

	if (pfile->child != nullptr)
	{
		int i;
		for (i = 0; i < pfile->num_children; i++)
		{
			free_pfile(pfile->child[i]);
			free(pfile->child[i]);
		}
		free(pfile->child);
	}

	memset(pfile, 0, sizeof(struct pfile_t));
}

static int load_pfile(struct pfile_t* pfile, FILE* pfp)
{
	if (pfile == nullptr || pfp == nullptr)
	{
		__LOG_ERROR__("load_pfile", "pfile nullptr or pfp nullptr\n");
		return 0;
	}
	memset(pfile, 0, sizeof(struct pfile_t));

	// ******  读取固定长度的头  ******
	if (fread(pfile, 1, __PFILE_HEADER_SIZE, pfp) != __PFILE_HEADER_SIZE)
		goto _label_load_pfile_error_;

	// ******  检查pfile有效性  ******
	if (pfile->flag > 6 || pfile->flag < 0)
		goto _label_load_pfile_error_;
	if(memcmp(pfile->version_string, __PFILE_MAGIC, strlen(__PFILE_MAGIC)) != 0)
		goto _label_load_pfile_error_;
	float ver = pfile->version = strtof(&pfile->version_string[8], nullptr);
	uint8_t checksum = check_size(pfile);

	// ******  获取额外字段的大小  ******
	if (ver > 1.1f)
	{
		if (fread(&pfile->const_size, 1, 4, pfp) != 4)
			goto _label_load_pfile_error_;
	}
	if (ver > 1.3f)
	{
		if (fread(&pfile->func_size, 1, 4, pfp) != 4)
			goto _label_load_pfile_error_;
	}
	if (ver > 3.0f)
	{
		if (fread(&pfile->info_size, 1, 4, pfp) != 4)
			goto _label_load_pfile_error_;
	}

	// ******  获取matlab认证信息，但这里跳过认证  ******
	if(fread(&pfile->verify_data, 1, 64, pfp) != 64)
		goto _label_load_pfile_error_;
	
	// ******  获取额外字段  ******
	if (ver > 2.3f && ver < 2.8f)
	{
		if (fread(&pfile->uk3, 1, 4, pfp) != 4)
			goto _label_load_pfile_error_;
	}
	if (ver > 2.4f && ver < 2.8f)
	{
		if (fread(&pfile->uk4, 1, 4, pfp) != 4)
			goto _label_load_pfile_error_;
	}
	if (ver > 2.5f )
	{
		if (fread(&pfile->uk5, 1, 4, pfp) != 4)
			goto _label_load_pfile_error_;
	}
	if (ver > 2.8f)
	{
		if (fread(&pfile->uk6, 1, 4, pfp) != 4)
			goto _label_load_pfile_error_;
	}

	// ******  获取函数名称  ******
	if (pfile->unnamed == 0 && pfile->name_size > 0)
	{
		pfile->func_name = (char*)malloc(pfile->name_size);
		if (pfile->func_name == nullptr ||
			fread(pfile->func_name, 1, pfile->name_size, pfp) != pfile->name_size)
			goto _label_load_pfile_error_;
	}

	// ******  获取pcode  ******
	if (pfile->pcode_size <= 0)
	{
		__LOG_ERROR__("load_pfile", "pcode size <= 0\n");
		goto _label_load_pfile_error_;
	}
	pfile->pcode_data = (uint8_t*)malloc(pfile->pcode_size);
	if (pfile->pcode_data == nullptr ||
		fread(pfile->pcode_data, 1, pfile->pcode_size, pfp) != pfile->pcode_size)
		goto _label_load_pfile_error_;
	decode(pfile->pcode_data, pfile->pcode_size, checksum);

	// ******  处理嵌套数据  ******
	if (pfile->num_children > 0)
	{
		int i;
		pfile->child = (pfile_t**)malloc(sizeof(struct pfile_t*) * pfile->num_children);
		if(pfile->child == nullptr)
			goto _label_load_pfile_error_;
		for (i = 0; i < pfile->num_children; i++)
		{
			pfile->child[i] = (pfile_t*)malloc(sizeof(struct pfile_t));
			if (pfile->child[i] == nullptr)
				goto _label_load_pfile_error_;
			if (load_pfile(pfile->child[i], pfp) == 0)
			{
				__LOG_ERROR__("load_pfile", "load pfile failed, parent(%s) child(%d)\n", pfile->func_name ,i);
				goto _label_load_pfile_error_;
			}
		}
	}

	// ******  获取符号函数数据  ******
	if (ver > 1.3f && pfile->func_size > 0)
	{
		pfile->func_data = (uint8_t*)malloc(pfile->func_size);
		if (pfile->func_data == nullptr ||
			fread(pfile->func_data, 1, pfile->func_size, pfp) != pfile->func_size)
			goto _label_load_pfile_error_;
		decode(pfile->func_data, pfile->func_size, checksum);
	}

	// ******  获取常量数据  ******
	if (ver > 1.1f && pfile->const_size > 0)
	{
		pfile->const_data = (uint8_t*)malloc(pfile->const_size);
		if (pfile->const_data == nullptr ||
			fread(pfile->const_data, 1, pfile->const_size, pfp) != pfile->const_size)
			goto _label_load_pfile_error_;
		decode(pfile->const_data, pfile->const_size, checksum);
	}

	// ******  获取导入信息  ******
	if (ver > 3.0f && pfile->info_size > 0)
	{
		pfile->info_data = (uint8_t*)malloc(pfile->info_size);
		if (pfile->info_data == nullptr ||
			fread(pfile->info_data, 1, pfile->info_size, pfp) != pfile->info_size)
			goto _label_load_pfile_error_;
		decode(pfile->info_data, pfile->info_size, checksum);
	}

	return 1;

_label_load_pfile_error_:
	free_pfile(pfile);
	return 0;
}

static void printf_pfile_tree(struct pfile_t* pfile, int level=0)
{
	int i;

	if (pfile != nullptr)
	{
		for (i = 0; i < level; i++)
			printf("|");
		printf("-- ");
		if (pfile->unnamed == 0 && pfile->func_name != nullptr)
			printf("%s\n", pfile->func_name);
		else
			printf("unamed\n");

		if (pfile->child != nullptr)
		{
			for (i = 0; i < pfile->num_children; i++)
			{
				if(pfile->child[i] != nullptr)
					printf_pfile_tree(pfile->child[i], level + 1);
			}
		}
	}
}

static uint8_t* parser_locals(struct pfile_t* pfile)
{
	int i;
	uint8_t* pcode = pfile->pcode_data;

	uint32_t local_vals_num = 0;
	uint32_t* local_vals_type = nullptr;
	char* local_vals_name = nullptr;

	if (pfile->unnamed)
	{
		pfile->num_locals = DWORD(pcode);
		pfile->num_input_args = 0;
		pfile->num_output_args = 0;
		pcode += 4;
	}
	else
	{
		pfile->num_input_args = DWORD(pcode);
		pfile->num_output_args = DWORD(pcode + 4);
		pcode += 8;
	}

	if (pfile->num_locals > 0)
	{
		// ******  初始化变量数组  ******
		pfile->locals_slot = slot_init(pfile->num_locals);	//前3个变量已被占用, 使用slot需要用宏LOCAL_ID包含id
		pfile->locals_type = (uint32_t*)malloc(sizeof(uint32_t) * pfile->num_locals);
		pfile->input_args_idx = (uint32_t*)malloc(sizeof(uint32_t) * pfile->num_input_args);
		pfile->output_args_idx = (uint32_t*)malloc(sizeof(uint32_t) * pfile->num_output_args);

		if (pfile->locals_slot == nullptr || pfile->locals_type == nullptr ||
			pfile->input_args_idx == nullptr || pfile->output_args_idx == nullptr)
			goto __parser_pcode_failed;

		// ******  解析变量类型  ******
		if (pfile->version > 2.3)
		{
			for (i = 0; i < pfile->num_locals; i++)
			{
				pfile->locals_type[i] = DWORD(pcode);
				pcode += 4;
			}
		}
		
		// ******  解析变量名称  ******
		for (i = 0; i < pfile->num_locals; i++)
		{
			slot_set(pfile->locals_slot, i, (char*)pcode);
			pcode += (strlen((char*)pcode) + 4) >> 2 << 2;
		}

		// ******  解析输入变量索引  ******
		if (pfile->num_input_args > 0)
		{
			for (i = 0; i < pfile->num_input_args; i++)
			{
				pfile->input_args_idx[i] = DWORD(pcode);
				pcode += 4;
			}
		}

		// ******  解析输出变量索引  ******
		if (pfile->num_output_args > 0)
		{
			uint8_t* tail = pfile->pcode_data + pfile->pcode_size - 4 * pfile->num_output_args;
			for (i = 0; i < pfile->num_output_args; i++)
			{
				pfile->output_args_idx[i] = DWORD(tail);
				tail += 4;
			}
		}
	}

	return pcode;
__parser_pcode_failed:
	if (pfile->locals_type != nullptr)
		free(pfile->locals_type);
	if (pfile->input_args_idx != nullptr)
		free(pfile->input_args_idx);
	if (pfile->output_args_idx != nullptr)
		free(pfile->output_args_idx);
	slot_free(pfile->locals_slot);

	pfile->locals_type = nullptr;
	pfile->locals_slot = nullptr;
	pfile->input_args_idx = nullptr;
	pfile->output_args_idx = nullptr;
	return nullptr;
}

static void printf_locals(struct pfile_t* pfile)
{
	int i, in_arg_idx, out_arg_idx;

	if (pfile->locals_slot == nullptr)
		parser_locals(pfile);

	in_arg_idx = out_arg_idx = 0;
	printf("num_locals: %d\n", pfile->num_locals);
	for (i = 0; i < pfile->num_locals; i++)
	{
		printf("|-- %d(%X): %s", i + 3, pfile->locals_type[i], slot_get(pfile->locals_slot, i));
		if ((i+3) == pfile->input_args_idx[in_arg_idx])
			printf("(input args %d)", in_arg_idx++);
		if ((i+3) == pfile->output_args_idx[out_arg_idx])
			printf("(output args %d)", out_arg_idx++);
		printf("\n");
	}
	printf("\n");
}

static void printf_const(struct pfile_t* pfile)
{	
	if (pfile->const_data != nullptr)
	{
		int n;
		uint8_t* addr = pfile->const_data;
		int const_num = SDWORD(addr);
		addr += 4;

		printf("num_const: %d\n", const_num);
		for (n = 0; n < const_num; n++)
		{
			while (DWORD(addr) == 0)
				addr += 4;

			uint32_t ins = DWORD(addr);
			uint32_t id = DWORD(addr + 4);
			uint32_t offset = (pfile->const_data - addr) / 4;
			addr += 8;

			switch (ins)
			{
			case PCODE_MXCHARCONST:
			{
				int len = SDWORD(addr);
				addr += 4;
				printf("|-- %d(%X) = '%s'\n", n, offset, wstring2string(wstring((wchar_t*)addr, len)).c_str());
				addr += (len * 2 + 3) >> 2 << 2;
			}
			break;
			case PCODE_MXARRAYCONST:
			{
				string ret;
				uint32_t rows = DWORD(addr);
				uint32_t cols = DWORD(addr + 4);
				uint32_t complex_flag = DWORD(addr + 8);
				addr += 12;
				int i, j;

				ret = "[";
				for (i = 0; i < rows; i++)
				{
					for (j = 0; j < cols; j++)
					{
						ret += std::to_string(DOUBLE(addr));
						if ((j + 1) != cols)
							ret += ",";
						addr += 8;
					}
					if ((i + 1) != rows)
						ret += ";";
				}
				ret += "]";
				printf("|-- %d(%X) = %s\n", n, offset, ret.c_str());
			}
			break;
			case PCODE_MXMARSHALCONST:
			{
				int len = SDWORD(addr);
				addr += 4;
				int i;
				string ret;

				printf("|-- %d(%X) = ", n, offset);
				for (i = 0; i < len; i++)
				{
					printf("%X,", DWORD(addr));
					addr += 4;
				}
				printf("\n");
				//printf("|-- %d = %s\n", n, ret.c_str());
			}
			break;
			default:
				printf("ins=(%d) error\n", ins);
				__LOG_ERROR_ARRAY__("printf_const", (char*)pfile->const_data, pfile->const_size);
				return;
			}
		}
		printf("\n");
	}
}

static void printf_func(struct pfile_t* pfile)
{
	//函数引用使用正偏移
	if (pfile->func_data != nullptr)
	{
		uint8_t* addr = pfile->func_data;
		int n = 0;
		printf("function\n");
		while (true)
		{
			uint32_t ins = DWORD(addr);
			addr += 4;
			if (PCODE_COMPEND == ins)
				break;

			switch (ins)
			{
				case PCODE_NOOP:
					break;
				case PCODE_FCNNAME:
				{
					uint32_t func_num = DWORD(addr);
					addr += 4;
					string res = string((char*)addr);
					addr += (res.length() + 3) >> 2 << 2;
					printf("|-- %d: %s\n", n++, res.c_str());
				}
				break;
			}
			
		}
	}
}

static insn_t* get_insn(map<int, insn_t*> insn, int adr)
{
	return insn.count(adr) ? insn[adr] : nullptr;
}

//检查double是否为整数
static bool check_double(double val)
{
	static const double err = 1e-10;

	return abs(int(val) - val) < err;
}

static void printf_pcode(struct pfile_t* pfile, map<int, insn_t*> insn)
{
	int i;

	insn_t* ins = get_insn(insn, 0);
	while (ins != nullptr)
	{
		printf("%X: ins=%s(0x%X)\n", ins->adr, g_pcode_ins[ins->type].ins, ins->type);
		for (i = 0; i < 7; i++)
		{
			if (ins->ops[i] != nullptr)
			{
				printf("%X: op=%c 0x%X", ins->adr + (i + 1) * 4, g_pcode_ins[ins->type].ops[i], SDWORD(ins->ops[i]));
				switch (g_pcode_ins[ins->type].ops[i])
				{
				case 'O':
					if (ins->next != nullptr)
						printf(" => 0x%X", ins->adr + SDWORD(ins->ops[i]) * 4);
					break;
				case 'I':
					if (ins->type == PCODE_LINENO)
						printf("=%d", SDWORD(ins->ops[i]));
					break;
				}
				printf("\n");
			}
		}
		printf("\n");
		ins = ins->next;
	}
}

static uint32_t* make_ops(insn_t* ins, const pcode_ins_t *ops, uint32_t *pcode)
{
	int i;
	for (i = 0; i < strlen(ops->ops); i++)
	{
		ins->ops[i] = pcode;
		switch (ops->ops[i])
		{
			case 'B':	//boolean
			case 'I':	//int
			case 'O':	//offset[goto]
			case 'W':	//offset[switch]
			case 'X':	//fucntion id
			case 'M':	//matrix
			case 'K':	//const target
			case 'F':	//function[id]
				pcode++;
				break;
			case 'C':	//const data
			{
				if (ins->type == PCODE_CHARCONST)
				{
					int n = *pcode++;
					pcode += (n + 3) / 4;
				}
				else if (ins->type == PCODE_MXCHARCONST)
				{
					int id = *pcode++;
					int n = *pcode++;
					pcode += (2 * n + 3) / 4;
				}
				else if (ins->type == PCODE_ARRAYCONST)
				{
					int row = *pcode++;
					int col = *pcode++;
					int cmplx_flag = *pcode++;
					pcode += 2 * row * col;
				}
				else if (ins->type == PCODE_MXARRAYCONST)
				{
					int id = *pcode++;
					int row = *pcode++;
					int col = *pcode++;
					int cmplx_flag = *pcode++;
					pcode += 2 * row * col;
				}
				break;
			}
			case 'L':	//line string
			case 'S':	//string
				pcode += (strlen((const char*)pcode) + 4) / 4;
				break;
			case 'R':	//reference
				pcode += 2;
				break;
			case 'm':	//matix array
				pcode += *pcode + 2;
				break;
		}
	}
	return pcode;
}

static int ana(map<int, lines_t> &lines, map<int, insn_t*> &insn, uint8_t* pcode, int len)
{
	int i;
	struct insn_t *prev_adr = nullptr;
	uint32_t* dw_pcode = (uint32_t*)pcode;
	uint32_t* dw_end = (uint32_t*)(pcode + len);
	int last_line = 0;

	while (dw_pcode < dw_end)
	{
		int ins_idx = SDWORD(dw_pcode);
		if (ins_idx > 108)
		{
			__LOG_ERROR__("ana", "opcode %d invalid\n", ins_idx);
			return 0;
		}

		insn_t* ins = (insn_t*)calloc(1, sizeof(insn_t));
		if (ins == nullptr)
			return 0;

		ins->type = ins_idx;
		ins->adr = (uint8_t*)dw_pcode - pcode;
		ins->prev = prev_adr;
		dw_pcode++;
		dw_pcode = make_ops(ins, &g_pcode_ins[ins_idx], dw_pcode);

		if (ins->type == PCODE_LINENO)
		{
			int line_num = DWORD(ins->ops[0]);
			lines[line_num].start_adr = ins->adr;
			if (last_line != 0)
				lines[last_line].end_adr = ins->adr;
			last_line = line_num;
		}

		if (prev_adr != nullptr)
			prev_adr->next = ins;
		ins->next = nullptr;

		insn[ins->adr] = ins;
		if (ins_idx == PCODE_COMPEND)
			break;

		prev_adr = ins;
	}
	lines[last_line].end_adr = len;
	return 1;
}

static pstring get_const_ref(struct pfile_t* pfile, int ktarget)
{
	//常量引用使用负偏移

	pstring ret;
	if (ktarget < 0 && pfile->const_data != nullptr)
	{
		uint8_t* addr = (pfile->const_data - ktarget * 4);
		while (DWORD(addr) == 0)
			addr += 4;

		uint32_t ins = DWORD(addr);
		uint32_t id = DWORD(addr + 4);
		addr += 8;

		switch (ins)
		{
		case PCODE_MXCHARCONST:
		{
			int len = SDWORD(addr);
			addr += 4;
			ret = "'" + wstring2string(wstring((wchar_t*)addr, len)) + "'";
			ret.set_flag(__CONST);
			return ret;
		}
		case PCODE_MXARRAYCONST:
		{
			uint32_t rows = DWORD(addr);
			uint32_t cols = DWORD(addr + 4);
			uint32_t complex_flag = DWORD(addr + 8);
			addr += 12;
			int i, j;

			ret = "";
			if(rows * cols != 1)
				ret = "[";
			for (i = 0; i < rows; i++)
			{
				for (j = 0; j < cols; j++)
				{
					if (check_double(DOUBLE(addr)))
						ret += std::to_string(int(DOUBLE(addr)));
					else
						ret += std::to_string(DOUBLE(addr));
					if ((j + 1) != cols)
						ret += ",";
					addr += 8;
				}
				if ((i + 1) != rows)
					ret += ";";
			}
			if (rows * cols != 1)
				ret += "]";
			ret.set_flag(__CONST);
			return ret;
		}
		case PCODE_MXMARSHALCONST:
			__LOG_ERROR__("get_const_ref", "PCODE_MXMARSHALCONST no implement\n", ins);
			break;
		default:
			__LOG_ERROR__("get_const_ref", "ins=(%d) error\n", ins);
		}
	}
	return "";
}

static pstring get_func_ref(struct pfile_t* pfile, int foffset)
{
#define __FUNC_MAP_LEN 19
	//matlab已定义好的函数，需要映射为实际的操作符
	struct 
	{
		string func_ori;
		string func_map;
		char flag;
	}func_map[__FUNC_MAP_LEN]
	{
		{ "plus", "+", __OPERATOR5 },
		{ "uplus", "+", __OPERATOR5 },
		{ "minus", "-", __OPERATOR5 },
		{ "uminus", "-", __OPERATOR5 },
		{ "times", ".*", __OPERATOR4 },
		{ "mtimes", "*", __OPERATOR4 },
		{ "rdivide", "./", __OPERATOR4 },
		{ "mrdivide", "/", __OPERATOR4 },
		{ "ldivide", ".\\", __OPERATOR4 },
		{ "mldivide", "\\", __OPERATOR4 },
		{ "power", ".^", __OPERATOR4 },
		{ "mpower", "^", __OPERATOR4 },
		{ "transpose", ".'", __OPERATOR1 },
		{ "ctranspose", "'", __OPERATOR1 },
		{ "_colonobj", ":", __OPERATOR6 },
		{ "eq", "==", __OPERATOR7 },
		{ "gt", ">", __OPERATOR7 },
		{ "lt", "<", __OPERATOR7 },
		{ "display", "", __NO_SEMI},
	};

	//函数引用使用正偏移
	if (foffset > 0 && pfile->func_data != nullptr)
	{
		uint8_t* addr = (pfile->func_data + foffset * 4);
		while (DWORD(addr) == 0)
			addr += 4;

		uint32_t ins = DWORD(addr);
		uint32_t func_num = DWORD(addr + 4);
		addr += 8;
		if (ins == PCODE_FCNNAME)
		{
			pstring res = pstring((char*)addr);
			int i;

			for (i = 0; i < __FUNC_MAP_LEN; i++)
			{
				if (res.compare(func_map[i].func_ori) == 0)
				{
					res = pstring(func_map[i].func_map);
					res.set_flag(func_map[i].flag);
					return res;
				}		
			}
			res.set_flag(__FUNC);
			return res;
		}		
	}
	return "";
}

static pstring get_local_ref(struct pfile_t* pfile, int id)
{
	pstring ret = pstring(slot_get(pfile->locals_slot, LOCAL_ID(id)));
	ret.set_flag(__VARIABLE);
	return ret;
}

static int out(FILE* pfp, queue<pstring> &expression)
{
	pstring temp, res, prev;
	int i, spaces = 0;

	while (expression.empty() == false)
	{
		temp = expression.front();
		expression.pop();

		if (temp.check_flag(__CODE_END) && spaces > 0)
			spaces--;

		if (prev.check_flag(__CODE_LINE))
		{
			for (i = 0; i < spaces; i++)
			{
				res += g_indented_character;
			}
		}
		res += temp;

		if (temp.check_flag(__CODE_HEAD))
			spaces++;
		prev = temp;
	}
	fwrite(res.c_str(), 1, res.length(), pfp);
	fflush(pfp);
	return 1;
}

//函数指令处理
static void function_proc(struct pfile_t* pfile, insn_t* ins, stack<pstring>& ins_stack)
{
	pstring temp = get_func_ref(pfile, SDWORD(ins->ops[0]));
	//__LOG_DEBUG__("emu", "func: %s\n", (char*)temp.c_str());
	int in_arg_num = SDWORD(ins->ops[2]);
	int out_arg_num = SDWORD(ins->ops[3]);
	char flag = temp.get_flag();

	if (flag >= __OPERATOR0 && flag <= __OPERATOR11)
	{
		//matlab自带的运算符函数，只有1个或2个参数
		if (in_arg_num > 2)
		{
			__LOG_ERROR__("emu", "matlab function error\n");
			temp = "";
		}
		pstring arg1, arg2;
		if (in_arg_num == 2)
		{
			arg2 = ins_stack.top();
			ins_stack.pop();
			arg1 = ins_stack.top();
			ins_stack.pop();

			//若表达式优先级低，则需要加上()
			if (arg1.get_flag() > flag&& arg1.get_flag() <= __OPERATOR11)
				arg1 = "(" + arg1 + ")";
			if (arg2.get_flag() > flag&& arg2.get_flag() <= __OPERATOR11)
				arg2 = "(" + arg2 + ")";
			temp = arg1 + temp + arg2;
		}
		else
		{
			arg1 = ins_stack.top();
			ins_stack.pop();

			//若表达式优先级低，则需要加上()
			if (arg1.get_flag() > flag&& arg1.get_flag() <= __OPERATOR11)
				arg1 = "(" + arg1 + ")";
			temp = arg1 + temp;
		}
		temp.set_flag(__EXPRESSION);
		ins_stack.push(temp);
	}
	else if (__CHECK_FLAG__(flag, __NO_SEMI))
	{
		ins_stack.push(temp);
	}
	else
	{
		//用户自定义函数
		temp += "(";
		if (in_arg_num > 0)
		{
			int i;
			string args;	//调整参数用，matlab函数参数从左到右入栈
			for (i = 0; i < in_arg_num; i++)
			{
				args = ins_stack.top() + args;
				if ((i + 1) != in_arg_num)
					args = "," + args;
				ins_stack.pop();
			}
			temp += args;
		}
		temp += ")";
		temp.set_flag(__EXPRESSION);
		ins_stack.push(temp);
	}
	
}

//生成函数定义头
static void define_proc(struct pfile_t* pfile, insn_t* ins, stack<pstring>& ins_stack)
{
	int i;
	pstring temp = "function ";
	if (pfile->num_output_args > 0)
	{
		if (pfile->num_output_args == 1)
		{
			temp += get_local_ref(pfile, pfile->output_args_idx[0]);
		}
		else
		{
			temp += "[";
			for (i = 0; i < pfile->num_output_args; i++)
			{
				temp += get_local_ref(pfile, pfile->output_args_idx[i]);
				if ((i + 1) != pfile->num_output_args)
					temp += ",";
			}
			temp += "]";
		}
		temp += "=";
	}
	temp += pfile->func_name;
	temp += "(";
	if (pfile->num_input_args > 0)
	{
		for (i = 0; i < pfile->num_input_args; i++)
		{
			temp += get_local_ref(pfile, pfile->input_args_idx[i]);
			if ((i + 1) != pfile->num_input_args)
				temp += ",";
		}
	}
	temp += ")";
	temp.set_flag(__KEYWORD_HEAD);
	ins_stack.push(temp);
}

//处理跳转的偏移
static int offset_proc(map<int, insn_t*>& insn, map<int, lines_t>& lines, int adr, int lineno)
{
	if (adr >= lines[lineno].start_adr && adr < lines[lineno].end_adr)
	{

	}
	else
	{
		insn_t* ins = get_insn(insn, adr);

		while (ins != nullptr)
		{
			if (ins->type == PCODE_LINENO)
				return adr;
			ins = ins->next;
		}
	}
	return -1;
}

static int emu(FILE *pfp, struct pfile_t* pfile, map<int, insn_t*>& insn, map<int, lines_t> &lines)
{
	stack<pstring> ins_stack;	//指令模拟使用的栈
	queue<pstring> out_queue;	//恢复的表达式队列，最后通过out函数输出
	pstring temp, temp2;
	int i = 0, cur_lineno = 0, out_semi = 1;
	insn_t* ins = get_insn(insn, 0);

	do
	{
		switch (ins->type)
		{
		//进栈指令
		case PCODE_CONSTREF:
			temp = get_const_ref(pfile, SDWORD(ins->ops[0]));
			ins_stack.push(temp);
			break;
		case PCODE_MATREF1:
			temp = get_local_ref(pfile, SDWORD(ins->ops[0]));
			ins_stack.push(temp);
			break;

		//出栈指令
		case PCODE_MATASS1:
			temp = get_local_ref(pfile, SDWORD(ins->ops[0]));
			if (temp.compare("v%") != 0)	//switch专用的中间变量
			{
				temp += " = ";
				temp.set_flag(__EXPRESSION);
				ins_stack.push(temp);
			}
			break;
		case PCODE_MATREF3:
			temp = get_local_ref(pfile, SDWORD(ins->ops[0]));
			if (ins_stack.empty() == false)
			{
				temp += "(" + ins_stack.top() + ")";
				temp.set_flag(__EXPRESSION);
				ins_stack.pop();
				ins_stack.push(temp);
			}
			break;

		case PCODE_MATLABFCN:
			function_proc(pfile, ins, ins_stack);
			break;

		case PCODE_CHECKSWITCH:
			if (ins_stack.empty() == false)
			{
				temp = "switch(" + ins_stack.top() + ")";
				temp.set_flag(__KEYWORD_HEAD);
				ins_stack.pop();
				while (ins_stack.empty() == false) ins_stack.pop();	//此时ins_stack应该没有东西了
				ins_stack.push(temp);
			}
			break;
		case PCODE_SWITCHCMP:
			if (ins_stack.empty() == false)
			{
				temp = "case " + ins_stack.top();
				temp.set_flag(__KEYWORD_BRANCH);
				ins_stack.pop();
				while (ins_stack.empty() == false) ins_stack.pop();	//此时ins_stack应该没有东西了
				ins_stack.push(temp);
			}
			break;

		//决策指令
		case PCODE_JUMP:
		{
			int branch_adr = ins->adr + SDWORD(ins->ops[0]) * 4;
			branch_adr = offset_proc(insn, lines, branch_adr, cur_lineno);
			insn_t* ins_branch = get_insn(insn, branch_adr);
			if (ins_branch == nullptr)
			{
				__LOG_ERROR__("emu", "ins branch null, ins_code=%X  => %X\n", ins->type, ins->adr + SDWORD(ins->ops[0]) * 4);
			}
			else
			{
				ins_branch->status = __KEYWORD_BRANCH;
			}
			break;
		}
		case PCODE_FOROP:
			temp = get_local_ref(pfile, SDWORD(ins->ops[0]));
			temp = "for " + temp + " = " + ins_stack.top();
			ins_stack.pop();
			temp.set_flag(__KEYWORD_HEAD);
			ins_stack.push(temp);
			break;
		case PCODE_BREAKOP:
			temp = "break";
			temp.set_flag(__KEYWORD);
			ins_stack.push(temp);
			break;
		case PCODE_CONTINUEOP:
			temp = "continue";
			temp.set_flag(__KEYWORD);
			ins_stack.push(temp);
			break;
		case PCODE_IF0:
		case PCODE_IF1:
		{
			temp = ins_stack.top();

			if (ins->prev != nullptr && ins->prev->type == PCODE_SWITCHCMP)
			//if (temp.get_flag() == __CONST)	
			{
				//switch处理
			}
			else
			{
				//if处理
				ins_stack.pop();
				if (ins_stack.empty() == false)
				{
					temp2 = ins_stack.top();
					int flag = temp2.get_flag();
					if (flag == PCODE_IF0 || flag == PCODE_IF1)
					{
						ins_stack.pop();
						temp = temp2 + (flag == PCODE_IF0 ? " && " : " || ") + temp;
					}
				}
				temp.set_flag(ins->type);
				ins_stack.push(temp);
			}

			int branch_adr = ins->adr + SDWORD(ins->ops[0]) * 4;	//todo: 跳转offset有问题
			
			branch_adr = offset_proc(insn, lines, branch_adr, cur_lineno);
			if (branch_adr != -1)
			{
				//printf("=>%X:%d\n", branch_adr, cur_lineno);
				insn_t* ins_branch = get_insn(insn, branch_adr);
				if (ins_branch == nullptr)
				{
					__LOG_ERROR__("emu", "ins branch null, ins_code=%X  => %X\n", ins->type, branch_adr);
				}
				else
				{
					ins_branch->status = __KEYWORD_BRANCH;
				}
			}
			break;
		}
		case PCODE_ENDOP:
			temp = "end";
			temp.set_flag(__KEYWORD_END);
			ins_stack.push(temp);
			break;
		case PCODE_RETURNOP:
			temp = "return";
			temp.set_flag(__KEYWORD_END);
			ins_stack.push(temp);
			break;


		//其他指令
		case PCODE_COMPEND:
		case PCODE_LINENO:
			if(ins->ops[0] != nullptr)
				cur_lineno = DWORD(ins->ops[0]);

			//换行时清空ins_stack
			if (ins_stack.empty() == false)
			{
				temp = ins_stack.top();
				if (temp.get_flag() == PCODE_IF0 || temp.get_flag() == PCODE_IF1)
				{
					ins_stack.pop();
					temp = "if(" + temp + ")";
					temp.set_flag(__KEYWORD_HEAD);
					out_queue.push(temp);
				}
			}

			temp = "";
			while (ins_stack.empty() == false)
			{
				if (ins_stack.top().check_flag(__NO_SEMI))
					out_semi = 0;
				temp += ins_stack.top();
				ins_stack.pop();
			}
			if (temp != "")
			{
				if (out_semi && temp.check_flag(__EXPRESSION))
					temp += ";";
				out_queue.push(temp);
			}

			temp = "\n";
			temp.set_flag(__CODE_LINE);
			out_queue.push(temp);
			out_semi = 1;
			break;
		case PCODE_RETSET:
			define_proc(pfile, ins, ins_stack);
			break;

		//忽略指令
		case PCODE_MARKSTACK:
		case PCODE_MX2BOOL:
		case PCODE_STACKSW1:
		case PCODE_STACKSW2:
			break;

		default:
			//__LOG_DEBUG__("emu", "%s(0x%X) ins no proc\n", g_pcode_ins[ins->type].ins, ins->type);
			break;
		}

		if (__CHECK_FLAG__(ins->status, __KEYWORD_BRANCH))
		{
			insn_t* prev_ins = ins->prev;
			while (prev_ins != nullptr && prev_ins->type == PCODE_LINENO)
			{
				prev_ins = prev_ins->prev;
			}

			//__LOG_DEBUG__("emu", "%X\n", ins->adr);

			if (prev_ins != nullptr)
			{
				if (prev_ins->type == PCODE_JUMP)
				{
					temp = "else";
					temp.set_flag(__KEYWORD_BRANCH);
					out_queue.push(temp);
				}
				else if(ins->next != nullptr && ins->next->type == PCODE_LINENO)
				{
					temp = "end";
					temp.set_flag(__KEYWORD_END);
					out_queue.push(temp);
					temp = "\n";
					temp.set_flag(__CODE_LINE);
					out_queue.push(temp);
				}
				else if (ins->next != nullptr && ins->next->type == PCODE_STACKSW2)
				{
					//todo: 感觉if0和if1指令的偏移有时正确，有时错误(偏了8个字节)
					if (ins->next->next->type == PCODE_LINENO)
					{
						temp = "end";
						temp.set_flag(__KEYWORD_END);
						out_queue.push(temp);
						temp = "\n";
						temp.set_flag(__CODE_LINE);
						out_queue.push(temp);
					}
				}
			}
		}

		ins = ins->next;
	} while (ins != nullptr);

	return out(pfp, out_queue);
}

int ptom_old_parse(char* mpath, char* ppath)
{
	FILE* pfp = nullptr;
	pfile_t pfile;
	map<int, lines_t> lines;	//记录指令中每一行的开始和结束，左闭右开
	map<int, insn_t*> insn;

	fopen_s(&pfp, ppath, "rb");
	if (pfp == nullptr)
	{
		__LOG_ERROR__("ptom_old_parse", "open pfile failed, path=%s\n", ppath);
		return 0;
	}
	if (load_pfile(&pfile, pfp) == 0)
	{
		__LOG_ERROR__("ptom_old_parse", "load_pfile faild\n");
		return 0;
	}
	fclose(pfp);

	//printf_pfile_tree(&pfile);
	//__LOG_DEBUG_ARRAY__("ptom_old_parse", (char*)pfile.pcode_data, pfile.pcode_size);
	//__LOG_DEBUG_ARRAY__("ptom_old_parse", (char*)pfile.func_data, pfile.func_size);
	//__LOG_DEBUG_ARRAY__("ptom_old_parse", (char*)pfile.const_data, pfile.const_size);

	uint8_t *pcode = parser_locals(&pfile);
	if (pcode == nullptr)
	{
		__LOG_ERROR__("ptom_old_parse", "parser_locals faild\n");
		free_pfile(&pfile);
		return 0;
	}

	fopen_s(&pfp, mpath, "w");
	if (pfp == nullptr)
	{
		__LOG_ERROR__("ptom_old_parse", "open mfile failed, path=%s\n", mpath);
		free_pfile(&pfile);
		return 0;
	}

	//__LOG_DEBUG_ARRAY__("ptom_old_parse", (char*)pcode, pfile.pcode_data + pfile.pcode_size - pcode);

	if(ana(lines, insn, pcode, pfile.pcode_data + pfile.pcode_size - pcode) == 0)
	{
		__LOG_ERROR__("ptom_old_parse", "ana faild\n");
		free_pfile(&pfile);
		return 0;
	}
	//printf_locals(&pfile);
	//printf_const(&pfile);
	//printf_func(&pfile);
	//printf_pcode(&pfile, insn);

	if (emu(pfp, &pfile, insn, lines)  == 0)
	{
		__LOG_ERROR__("ptom_old_parse", "emu faild\n");
		free_pfile(&pfile);
		return 0;
	}

	//if(parser_pcode(stdout, pcode, pfile.pcode_data + pfile.pcode_size - pcode) == 0)
	//{
	//	__LOG_ERROR__("ptom_old_parse", "parser_pcode faild\n");
	//	free_pfile(&pfile);
	//	return 0;
	//}

	//fclose(pfp);
	free_pfile(&pfile);
	return 1;
}