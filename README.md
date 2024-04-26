# A traditional Unix/Linux alternative to those ugly shells full of flaws

I needed a shell scripting language that at least looked like normal, not the ugly, flawed stuff of traditional Unix/Linux, so I modified the Lua 5.1 interpreter (lua.c), the current development environment is MSYS2, and the source code is modified in /// On the basis of retaining the original function, the following adjustments have been made: only the input that starts with backticks is a Lua statement, and all other are commands, first determine whether it is an internal command, otherwise try to execute it as an external command.

You can use the -i parameter to simulate a traditional shell loading rc file, in which you can use variable _PROMPT and _PROMPT2 custom prompts, and support escape characters, as shown in the following table. The process of executing the internal command is to check whether there is a Lua function lish_exec_internal(), if there is, the line entered by the user is used as a parameter, if the return value is 0, the execution is successful, otherwise the external command is attempted. The lishrc.lua file contains the default prompt style and the default internal commands.

|Escape Character|Description|
|-|-|
|\e|ASCII 0x1B character, the ANSI escape character |
|\w|current directory|

# 一个传统Unix/Linux的那些丑陋的到处是缺陷的Shell的替代品

我需要一个至少看上去是正常的Shell脚本语言，而不是传统Unix/Linux的那些丑陋的、到处都是缺陷的东西，所以我修改了Lua 5.1的解释器（lua.c），当前的开发环境是MSYS2环境，源代码修改的位置用///标出，在保留原有功能的基础上，做了如下调整：只有以反引号开头的输入才是Lua语句，此外皆为命令，先判断是否为内部命令，否则尝试作为外部命令执行。

使用-i参数可以模拟传统Shell加载rc文件，其中可以用变量_PROMPT和_PROMPT2自定义提示符，支持转义符，含义如下表所示。执行内部命令的流程是：检查是否存在Lua函数lish_exec_internal()，如果有则以用户输入的行作为参数，如果返回值为0即执行成功，否则尝试执行外部命令。lishrc.lua文件中包含默认的提示符样式和默认的内部命令。

|转义符|说明|
|-|-|
|\e|ASCII 0x1B字符，即ANSI转义符|
|\w|当前目录|

