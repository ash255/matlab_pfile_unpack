#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include "ptom.h"
#include "logger.h"

/* 定义解密后的明文文件最大的大小，若明文文件过大，应该适当增加改值 */
#define __MAX_MFILE_SIZE__ 102400 

struct mfile_t
{
	int32_t token[7];
	int32_t size;
	uint8_t* mdata;
};

struct pfile_t
{
	char major[6];
	char minor[6];
	uint32_t  scramble;
	uint32_t  crc;
	uint32_t  uk2;	//unknown
	uint32_t  size_after_compress;
	uint32_t  size_before_compress;
	uint8_t* pdata;
};

struct slot_t
{
	char** name;	//slot name的指针列表
	uint32_t size;
};

typedef int (*uncompress)(uint8_t*, uint32_t*, uint8_t*, uint64_t);

static const uint32_t g_scrambling_tbl[256] = {
	0x050F0687,0xC3F63AB0,0x2E022A9C,0x036DAA8C,0x32ED8AE2,0xF5571876,0xC66FE7F3,0x6CF0D7C0,
	0xBE08BA59,0x0CBB32BE,0x2E1E76F9,0x5B095029,0xD7B83753,0xB949C2EA,0x002B7101,0x10BF6F59,
	0x5A565564,0xCF31F672,0x49B64869,0x30B5AE91,0x33D84C72,0xE4B5B87D,0x97EF0BD8,0x58A53999,
	0xA2D54211,0x040D16F3,0x8ED0F2AB,0xA1123692,0x7CAD41FD,0x47FD2EE5,0xD5B56675,0x01BC4884,
	0x8BF36995,0x83B79111,0x8529F311,0x3EE0F477,0x790EA987,0x4B99DB04,0x2BD1CC37,0x371763E1,
	0x58550DC3,0xD9F04330,0x1220B40A,0xB00D4516,0x133A061B,0x924C250C,0x40CCB470,0x6D905B7F,
	0x617E1B7E,0x0A82FCD9,0x1E460A11,0x155667F0,0x6F38B557,0x363515E9,0x6DFBA189,0x920DF768,
	0x3A422CDD,0x7CCC9435,0xB3202DFB,0x36EF6EDA,0x44C9C31A,0x08D59470,0xB8ABB75E,0x50BD2CAF,
	0x8C8D2582,0x3DD5AA6F,0x0F9E2126,0x059BCF09,0x096F8574,0x3B6FED5D,0x3CB332EA,0x61C49337,
	0x9560308D,0x4ED3E6F5,0x91D1D84D,0xA89A36A8,0xE1200C01,0xD29E8CBD,0x162A9228,0x429E277F,
	0x5D218997,0x34709C39,0x57F48F70,0x4C5A3EEE,0x6AA5B222,0xC5F030F9,0xDE683656,0xA4E7DEFF,
	0xC2BCC52E,0x11886451,0xDBD74DD9,0x87868848,0x1A5DF8C2,0x14830538,0xD843B4F7,0x26EB1E44,
	0x5258AFA7,0xE7E1D61D,0x2C86ED4D,0x5BC8351B,0x2351C37A,0x693A2038,0x3D8CC852,0xB8B1F408,
	0x380E072D,0x4F5EA0A0,0xE14C2AB0,0x192E132E,0xA1FD2D5D,0xF776BCD8,0x5BCC3AAD,0xFF1EB6F4,
	0xABE75911,0x33C0CA1D,0xCB78F5E2,0x168D0B34,0xF9B0FB17,0xA9E12C39,0xBB74EA33,0x3C6DC045,
	0xBB69908A,0x174C380D,0x43F4488E,0x55C7894C,0xABCF3D45,0x9C37FD85,0x7CB2A790,0xFE27ECEC,
	0x8419D3A3,0x293994DE,0x59F02208,0xA76B971D,0x1273B516,0x177CEA5A,0x601D8B25,0x4A81BC43,
	0x66DB8AFA,0xC169B5D6,0x63AFCF71,0x08D8B858,0x38E072AE,0x3F7C0A1E,0x87F76F4C,0x64C7CBC0,
	0xF33CD43C,0xD370652F,0x7B54D6F4,0x6CEDCF53,0x7D519168,0xB6C9C127,0xA95B8F98,0xB8BB21F2,
	0xCE15F934,0xED4FD826,0x8E82AB3F,0x79E53679,0x0987D5AC,0x8B3552CF,0x780D2366,0x8DA1A94F,
	0xB46EE7AD,0x51FD456E,0x350D406C,0xC6E29CC3,0x697A2FC8,0x952ACB92,0x11645906,0xD055BAC3,
	0x56948168,0x75142877,0xD92E731B,0x8F74F416,0xB4903296,0x6125E267,0xF43CBFD6,0x27CD06D2,
	0xB4964796,0xEF9196CA,0x14BAD625,0xB1E7D8FE,0x265B57F2,0xBE1665BD,0xEAA2FAF1,0xF4715126,
	0x2B663DE4,0x7925A630,0x6E5687A0,0xB4EE1390,0x045AF8FF,0x6663AB06,0x428FBCDF,0xB8C9E0AD,
	0x3860F074,0xF79CFD4B,0xFFAC7D70,0x21DB203C,0x0CC7C8DD,0x9110D677,0xF230DAFF,0x635C4A45,
	0x8624FEEE,0x4B5F4E1A,0xF2D13E5C,0x3AB53184,0xAC853082,0x670DFE32,0x62823856,0x611B7818,
	0xD69F94FD,0xF73D0E7B,0x13035117,0xFCFAECEF,0x35537439,0xFDA64C08,0xF16C3E15,0xE0B9B21D,
	0xF6CBF238,0xDFC2C5B5,0x15A7C5AD,0xFB26EB37,0xC62670BB,0x5837828C,0xB3F0CBE4,0xFE87612F,
	0xCFD47FD7,0x339D4955,0xA062816C,0xDC9C48B5,0xC4AE1FCC,0x92935C6B,0x3FF892FA,0x4AD31EBA,
	0xDDF2AA86,0xB2C9D156,0x8588503F,0x0A77DB08,0x19D7CF89,0xE80A8895,0xEB935320,0xF0776486,
	0x5F479711,0xFE96A437,0xED725175,0x949B0B4A,0x7C3CF03F,0x5EDE8F8A,0x7554BD67,0xF308E277,
	0xBEA15540,0x0AFC8314,0xEE2FCDAF,0x04C7C5FB,0x633405A0,0x22209993,0x834F272B,0x33088577,
};
static const char g_token[134][25] = {
	 "",
	 "function",
	 "function",
	 "if",
	 "switch",
	 "try",
	 "while",
	 "for",
	 "end",
	 "else",
	 "elseif",
	 "break",
	 "return",
	 "parfor",
	 "",
	 "global",
	 "persistent",
	 "",
	 "",
	 "",
	 "catch",
	 "continue",
	 "case",
	 "otherwise",
	 "",
	 "classdef",
	 "",
	 "",
	 "properties",
	 "",
	 "methods",
	 "events",
	 "enumeration",
	 "spmd",
	 "parsection",
	 "section",
	 "",
	 "",
	 "",
	 "",
	 "id",
	 "end",
	 "int",
	 "float",
	 "string",
	 "dual",
	 "bang",
	 "?",
	 "",
	 "",
	 ";",
	",",
	 "(",
	 ")",
	 "[",
	 "]",
	 "{",
	 "}",
	 "feend",
	 "",
	 "'",
	 "dottrans",
	 "~",
	 "@",
	 "$",
	 "`",
	 "\"",
	 "",
	 "",
	 "",
	 "+",
	 "-",
	 "*",
	 "/",
	 "\\",
	 "^",
	 ":",
	 "",
	 "",
	 "",
	 ".",
	 ".*",
	 "./",
	 ".\\",
	 ".^",
	 "&",
	 "|",
	 "&&",
	 "||",
	 "<",
	 ">",
	 "<=",
	 ">=",
	 "==",
	 "~=",
	 "=",
	 "cne",
	 "arrow",
	 "",
	 "",
	 "\n",
	 "\n",
	 "\n",
	 "...\n   ",
	 "",
	 "comment",
	 "blkstart",
	 "blkcom",
	 "blkend",
	 "cpad",
	 "pragma",
	 "...",
	 "..",
	 "deep_nest",
	 "deep_stmt",
	 "",
	 "white",
	 "",
	 "negerr",
	 "semerr",
	 "eolerr",
	 "unterm",
	 "badchar",
	 "deep_paren",
	 "fp_err",
	 "res_err",
	 "deep_com",
	 "begin_type",
	 "end_type",
	 "string_literal",
	 "unterm_string_literal",
	 "arguments_block",
	 "last_token",
	 ""
};

static const char g_major_version[] = "v01.00";
static const char g_minor_version[] = "v00.00";
static HMODULE g_dll_handle;
static uncompress g_uncompress_func;
static float g_version = 1.1f;

static struct slot_t* slot_init(uint32_t num)
{
	struct slot_t* slot_ptr = nullptr;
	char** name_ptr = nullptr;

	slot_ptr = (struct slot_t*)malloc(sizeof(struct slot_t));
	if (slot_ptr == nullptr)
		return nullptr;

	memset(slot_ptr, 0, sizeof(struct slot_t));
	slot_ptr->size = num;
	if (num > 0)
	{
		name_ptr = (char**)malloc(num * sizeof(char*));
		if (!name_ptr)
		{
			free(slot_ptr);
			return nullptr;
		}
		slot_ptr->name = name_ptr;
	}
	return slot_ptr;
}

static int slot_set(struct slot_t* slot, uint32_t id, char* name)
{
	if (id < slot->size && slot->name != nullptr)
	{
		slot->name[id] = name;
		return 1;
	}
	return 0;
}

static char* slot_get(struct slot_t* slot, uint32_t id)
{
	if (id < slot->size && slot->name != nullptr && slot->name[id] != nullptr)
	{
		return slot->name[id];
	}
	return nullptr;
}

static void slot_free(struct slot_t* slot)
{
	if (slot != nullptr)
	{
		if (slot->size > 0 && slot->name != nullptr)
		{
			free(slot->name);
		}
		free(slot);
	}
}

static long fsize(FILE* fp)
{
	long len = 0, cur = 0;

	if (fp != nullptr)
	{
		cur = fseek(fp, 0, SEEK_CUR);
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);
		fseek(fp, 0, cur);
	}
	return len;
}

static void descrambling(struct pfile_t* pfile)
{
	uint32_t i;
	uint8_t scramble_number;
	uint32_t* pdata = (uint32_t*)pfile->pdata;

	scramble_number = (uint8_t)(pfile->scramble >> 12);
	for (i = 0; i < pfile->size_after_compress / 4; i++)
	{
		*pdata ^= g_scrambling_tbl[(uint8_t)(i + scramble_number)];
		pdata++;
	}

}

static uint32_t my_ntohl(uint32_t in)
{
	uint32_t out;
	uint8_t* pin = (uint8_t*)&in;
	uint8_t* pout = (uint8_t*)&out;

	pout[0] = pin[3];
	pout[1] = pin[2];
	pout[2] = pin[1];
	pout[3] = pin[0];
	return out;
}

static int check_pfile(struct pfile_t* pfile)
{
	if (memcmp(pfile->major, g_major_version, strlen(g_major_version)) == 0)
	{
		if (memcmp(pfile->minor, g_minor_version, strlen(g_minor_version)) == 0)
		{
			pfile->scramble = my_ntohl(pfile->scramble);
			pfile->size_after_compress = my_ntohl(pfile->size_after_compress);
			pfile->size_before_compress = my_ntohl(pfile->size_before_compress);
			if (pfile->size_after_compress > 0 && pfile->size_before_compress > 0)
			{
				return 1;
			}
		}
	}
	return 0;
}

static int check_mfile(struct mfile_t* mfile)
{
	int i;

	for (i = 0; i < 7; i++)
	{
		mfile->token[i] = my_ntohl(mfile->token[i]);
	}
	return 1;
}

static int uncompress_pfile(struct mfile_t* mfile, struct pfile_t* pfile)
{
	uint32_t size;
	uint8_t* tmp_ptr;

	if (!g_dll_handle || !g_uncompress_func)
		return 0;

	descrambling(pfile);

	size = pfile->size_before_compress;
	tmp_ptr = (uint8_t*)malloc(size);
	if (tmp_ptr == nullptr)
		return 0;

	g_uncompress_func(tmp_ptr, &size, pfile->pdata, (uint64_t)pfile->size_after_compress);

	if (size != pfile->size_before_compress)
	{
		free(tmp_ptr);
		return 0;
	}

	size -= 7 * sizeof(uint32_t);
	mfile->mdata = (uint8_t*)malloc(size);
	if (mfile->mdata != nullptr)
	{
		memcpy(mfile->token, tmp_ptr, 7 * sizeof(uint32_t));
		memcpy(mfile->mdata, tmp_ptr + 7 * sizeof(uint32_t), size);
		mfile->size = pfile->size_before_compress - 7 * sizeof(uint32_t);
		free(tmp_ptr);
		return 1;
	}
	else
	{
		free(tmp_ptr);
		memset(mfile, 0, sizeof(struct mfile_t));
		return 0;
	}
}

static int parse_pfile(struct pfile_t* pfile, char* path)
{
	FILE* pfp = nullptr;
	long psize = 0;

	//pfp = fopen(path, "rb");
	fopen_s(&pfp, path, "rb");
	if (pfp != nullptr)
	{
		psize = fsize(pfp);
		if (psize >= 32)
		{
			fread(pfile, 1, 32, pfp);
			if (check_pfile(pfile))
			{
				pfile->pdata = (uint8_t*)malloc(pfile->size_after_compress);
				if (pfile->pdata)
				{
					if (pfile->size_after_compress == fread(pfile->pdata, 1, pfile->size_after_compress, pfp))
					{
						fclose(pfp);
						return 1;
					}
					else
					{
						free(pfile->pdata);
					}
				}
			}
			memset(pfile, 0, sizeof(struct pfile_t));
		}
		fclose(pfp);
	}
	return 0;
}

static int parse_mfile(char* mpath, struct mfile_t* mfile)
{
	int i, j, k, success, res_id, mfile_rsize;
	struct slot_t* slot[1] = { nullptr };
	char* name_ptr = nullptr;
	char* mfile_ptr = nullptr, * mfile_tmp = nullptr;
	uint8_t* cur_ptr = nullptr;
	uint8_t* end_ptr = nullptr;

	check_mfile(mfile);

	/* parse name symbol */
	for (i = 0, j = 0; i < 7; i++)
		j += mfile->token[i];
	slot[0] = slot_init(j);

	name_ptr = (char*)(mfile->mdata);
	k = 0;
	for (i = 0; i < 7; i++)
	{
		for (j = 0; j < mfile->token[i]; j++)
		{
			slot_set(slot[0], k++, name_ptr);
			name_ptr += strlen(name_ptr) + 1; //+1 is because of \0
			//printf("%04X	->	%s\n",k,name_ptr);
		}
	}

	/* parse code */
	cur_ptr = (uint8_t*)name_ptr;
	end_ptr = mfile->mdata + mfile->size;
	mfile_tmp = mfile_ptr = (char*)malloc(__MAX_MFILE_SIZE__);
	mfile_rsize = __MAX_MFILE_SIZE__;
	if (mfile_ptr == nullptr)
	{
		slot_free(slot[0]);
		return 0;
	}

	success = 1;
	while (1)
	{
		if (cur_ptr >= end_ptr)
			break;

		/* parse 2 byte code */
		if ((cur_ptr[0] & 0x80) == 0x80)
		{
			res_id = 128 + 256 * ((cur_ptr[0] & 0x7F) - 1) + cur_ptr[1];
			name_ptr = slot_get(slot[0], res_id);

			/* 每个用户定义的符号前面加一个空格 */
			mfile_tmp[0] = ' ';
			mfile_tmp++;
			mfile_rsize--;
			strcpy_s(mfile_tmp, mfile_rsize, name_ptr);
			mfile_tmp += strlen(name_ptr);
			mfile_rsize -= strlen(name_ptr);
			cur_ptr += 2;
			continue;
		}

		/* parse 1 byte code */
		if (cur_ptr[0] < 134)
		{
			name_ptr = (char*)g_token[cur_ptr[0]];

			if (strlen(name_ptr) != 0)
			{
				strcpy_s(mfile_tmp, mfile_rsize, name_ptr);
				mfile_tmp += strlen(name_ptr);
				mfile_rsize -= strlen(name_ptr);
			}
			cur_ptr += 1;
			continue;
		}

		/* parse 3 byte code */
		__LOG_ERROR__("parse_mfile", "无法解析3字节代码，请到github提交issues\n");
		__LOG_ERROR_ARRAY__("无法解析的字节", (char*)cur_ptr, end_ptr - cur_ptr);
		success = 0;
		break;
	}

	if (success == 1)
	{
		FILE* fp = nullptr;
		//fp = fopen(mpath, "w");
		fopen_s(&fp, mpath, "w");
		if (fp != nullptr)
		{
			fwrite(mfile_ptr, 1, strlen(mfile_ptr), fp);
			fclose(fp);
		}
	}

	free(mfile_ptr);
	slot_free(slot[0]);
	return success;
}

float ptom_getVersion()
{
	return g_version;
}

int ptom_init()
{
	g_dll_handle = LoadLibraryA("zlib1.dll");

	if (!g_dll_handle)
		return 0;

	g_uncompress_func = (uncompress)GetProcAddress(g_dll_handle, "uncompress");
	if (!g_uncompress_func)
	{
		FreeLibrary(g_dll_handle);
		return 0;
	}
	return 1;
}

void ptom_deinit()
{
	if (g_dll_handle)
		FreeLibrary(g_dll_handle);
}

int ptom_parse(char* mpath, char* ppath)
{
	struct pfile_t pfile;
	struct mfile_t mfile;

	if (ppath == nullptr || mpath == nullptr || strlen(ppath) == 0 || strlen(mpath) == 0)
		return 0;

	if (parse_pfile(&pfile, ppath) == 0)
	{
		__LOG_ERROR__("ptom_parse", "parse_pfile failed\n");
		return 0;
	}

	if (uncompress_pfile(&mfile, &pfile) == 0)
	{
		__LOG_ERROR__("ptom_parse", "uncompress_pfile failed\n");
		return 0;
	}
	if (parse_mfile(mpath, &mfile) == 0)
	{
		__LOG_ERROR__("ptom_parse", "parse_mfile failed\n");
		return 0;
	}
	return 1;
}

