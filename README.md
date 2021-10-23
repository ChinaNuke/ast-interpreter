# 国科大编译作业：基于 Clang 的 C 语言解释执行器

恭喜又一个入坑国科大编译课程的可怜娃！完成本次大作业大约需要 1 到 2 周的时间，请提前做好规划。在实现过程中，你可以把给定的 25 个测试用例按照从简到难的顺序进行一个分组和排序，一次实现一块功能，一下子通过一组测试用例。不建议你直接把我这个作为你的作业原样交上去，你可以参照我的代码，按照自己的想法去实现一遍，还有一些我没有实现或者实现比较草率的地方，你可以去完善一下。如果这个仓库有帮助到你，欢迎点亮上面的 Star ！祝你在这个大作业中有所收获！

## 下载本仓库

```shell
$ git clone https://github.com/ChinaNuke/ast-interpreter.git
```

## Docker 环境

课程提供了一个编译安装好了 clang 的 docker 环境，你可以直接拿来使用，也可以自己创建一个新的 docker 容器或者虚拟机，自行通过包管理工具或者通过源码编译安装相同版本的 clang 。第二个命令中 `-v` 参数指定一个从本机到 docker 容器的目录映射，这样你就可以直接在本机使用 VSCode 等工具编写代码，在 docker 容器中编译运行，而不用把文件拷来拷去。

```shell
$ docker pull lczxxx123/llvm_10_hw:0.2
$ docker run -td --name llvm_experiment -v "$PWD/ast-interpreter":/root/ lczxxx123/llvm_10_hw:0.2
$ docker exec -it llvm_experiment bash
```

重新开机后，docker 容器可能没有在运行，你不必再执行一遍 `docker run` 命令，只需要执行 `docker start llvm_experiment` 就可以了。如果你还是不太熟悉 docker 命令的话，[这里](https://dockerlabs.collabnix.com/docker/cheatsheet/)有一份 Docker Cheat Sheet 可以查阅。

## 编译

从这一小节开始的所有命令都是在 docker 容器中执行的。创建一个名为 `build` 的目录并进入，使用 `-DLLVM_DIR` 指定 LLVM 的安装路径，`-DCMAKE_BUILD_TYPE=Debug` 参数指明编译时包含符号信息以方便调试。以后的每次编译只需要执行 `make` 就可以了，除非你修改了 Cmakefiles.txt 。

```shell
$ mkdir build && cd $_
$ cmake -DLLVM_DIR=/usr/local/llvm10ra/ -DCMAKE_BUILD_TYPE=Debug ..
$ make
```

## 运行

编译出来的程序是以要解释执行的源文件内容为参数的，而不是文件名。

```shell
$ ./ast-interpreter "$(cat ../tests/test00.c)"
```

## 调试

对于一个测试用例，可以使用下面的命令很方便地打印出语法树，可以对照这个语法树来进行调试。

```shell
$ clang -Xclang -ast-dump -fsyntax-only ../tests/test20.c
```

最朴素的调试工具是 GDB ，[这里](https://darkdust.net/files/GDB%20Cheat%20Sheet.pdf)也有一份 GDB Cheat Sheet 供你查阅。或者你可以使用 CLion 或者 VSCode 配置一下远程编译和调试，我没有用过，但是据说效果不错。

```shell
$ gdb ./ast-intepreter
(gdb) run "$(cat ../tests/test00.c)"
```

## 参考

组传代码代代相传！感谢师兄们打下的基础。

https://clang.llvm.org/docs/RAVFrontendAction.html

https://github.com/ycdxsb/ast-interpreter

https://github.com/plusls/ast-interpreter