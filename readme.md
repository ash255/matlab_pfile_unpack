# matlab_pfile_unpack
本项目用于解析由matlab使用pfile命令编译出来的(*.p)文件并将其转为文本文件(*.m)

## Usage
```
usage: ptom.exe [-i input_file] [-o output_file]
-i: input file will be decrypted
-o: output file will be decrypted
```

## Contributing

[@yumingxue](https://github.com/yumxcode)

## Note
1. 注释并不会被matlab编译，所以转换出来的*.m文件没有注释，在原来注释的未知留下空行
2. 关于其中的压缩功能，目前使用的是zlib.dll，并且该zlib为64位dll，所以程序目前只能编译为64位，并且将可执行程序与zlib.dll放在一起。
而在我的另一个项目exe2m使用crypto++库实现了类似解压缩的功能，此项目没有使用是考虑到代码的体积。
3. 3字节代码暂时未遇到，所以无法解析
4. 1字节代码有些token也没有遇到过，所以如果解析到这些字节会失败
5. 本项目优化了恢复后的*.m文件的文字排版，使之更加接近原生的*.m文件

## version
* v1.0 初始版本，实现基本的pcode解密，支持v00.00v00.00和v01.00v00.00
* v1.1 优化代码结构，优化代码输出
* v1.2 修复分支输出不整齐问题，支持旧版pcode解密，支持P-file  x.x解密（测试版）

## release
https://github.com/ash255/matlab_pfile_unpack/releases/tag/v1.0

## 已知问题
1. 旧版pcode的解析存在问题，过于复杂的代码解析可能有误。
2. 旧版pcode的解析结果不够美观。
3. 旧版pcode的switch处理未实现
4. 旧版pcode部分代码缺少结束符end
5. 不支持嵌套解析