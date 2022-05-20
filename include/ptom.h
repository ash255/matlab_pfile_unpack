/* 本文件解析新版pfile，适用于matlab 2007b(包含)之后使用pcode生成的pfile，pfile文件开头类似v00.00v00.00字样 */
#ifndef _PTOM_H_
#define _PTOM_H_

extern int ptom_parse(char* mpath, char *ppath);
extern int ptom_init();
extern void ptom_deinit();

#endif
