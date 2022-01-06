#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ptom.h"
#include "logger.h"

int main(int argc, char** argv)
{
	int i;
	char pfile[512], mfile[512];

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

	if (ptom_parse(mfile, pfile))
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