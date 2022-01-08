# Title
本项目用于解析由matlab使用pfile命令编译出来的(*.p)文件并将其转为文本文件(*.m)

## Usage

```
usage: %s [-i input_file] [-o output_file]
-i: input file will be decrypted
-o: output file will be decrypted
```
## Contributing

@yumingxue

## Note

```
1. 注释并不会被matlab编译，所以转换出来的*.m文件没有注释，在原来注释的未知留下空行
2. 关于其中的压缩功能，目前使用的是zlib.dll，并且该zlib为64位dll，所以程序目前只能编译为64位。
而在我的另一个项目exe2m使用crypto++库实现了类似解压缩的功能，此项目没有使用是考虑到代码的体积。
3. 3字节代码暂时未遇到，所以无法解析
4. 1字节代码有些token也没有遇到过，所以如果解析到这些字节会失败
5. 本项目优化了恢复后的*.m文件的文字排版，使之更加接近原生的*.m文件
```

## Preview
![image](https://github.com/ash255/ptom/blob/main/%E6%95%88%E6%9E%9C%E5%9B%BE.PNG)
