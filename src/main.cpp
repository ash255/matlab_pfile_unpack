#include "ptom.h"
#include "pold_dec.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static float g_version = 1.2f;

static void show_version()
{
	printf("********************************\n");
	printf("*****  pfile unpack v%.2f  *****\n", g_version);
	printf("********************************\n");
}

static int check_pfile_version(char *pfile)
{
	FILE* fp = nullptr;
	uint8_t buffer[16] = { 0 };
	int ver = 0;

	fopen_s(&fp, pfile, "rb");
	if (fp == nullptr)
	{
		__LOG_ERROR__("check_pfile_version","file %s can not open\n", pfile);
		return 0;
	}
	if (fread(buffer, 1, 16, fp) != 16)
	{
		__LOG_ERROR__("check_pfile_version", "file %s read failed\n", pfile);
		return 0;
	}

	//新版： v0x.00v0x.00
	//旧版：P-file  x.x
	if (buffer[0] == 'v' && buffer[3] == '.' && buffer[6] == 'v' && buffer[9] == '.')
		ver = 2;
	else if (buffer[0] >= 0 && buffer[0] <= 6 && memcmp(&buffer[1], "P-file", 6) == 0)
		ver = 1;

	fclose(fp);
	return ver;
}

#include "pstring.h"
int main(int argc, char** argv)
{
	int i;
	char pfile[512], mfile[512];

	show_version();

	if (argc < 3)
	{
		printf("usage: %s [-i input_file] [-o output_file]\n", argv[0]);
		printf("-i: input file will be decrypted\n");
		printf("-o: output file will be decrypted\n");
		system("pause");
		return 0;
	}

	log_with_fd(stdout);
	log_set_level(__LEVEL_DEBUG__);
	log_set_level(__LEVEL_MESSAGE__);
	log_set_level(__LEVEL_ERROR__);
	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-i") == 0)
		{
			strcpy_s(pfile, sizeof(pfile), argv[i + 1]);
			i++;
			continue;
		}
		else if (strcmp(argv[i], "-o") == 0)
		{
			strcpy_s(mfile, sizeof(mfile), argv[i + 1]);
			i++;
			continue;
		}
	}

	if (ptom_init() == 0)
	{
		__LOG_ERROR__("main", "初始化失败\n");
		return 0;
	}

	int result = 0;
	switch (check_pfile_version(pfile))
	{
	case 1:
		result = ptom_old_parse(mfile, pfile);
		break;
	case 2:
		result = ptom_parse(mfile, pfile);
		break;
	}

	if (result)
	{
		__LOG_MESSAGE__("main", "转换成功\n");
	}	
	else
	{
		__LOG_ERROR__("main", "转换失败\n");
	}

	ptom_deinit();
	//system("pause");
	return 1;
}