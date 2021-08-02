---
name: Bug report / 反馈一个bug
about: Create a report about something not working in Swow / 反馈有Swow工作不正常的地方
title: '[bug] Replace with your short bug description / 替换标题为一个简短的bug描述'
labels: 'bug'
assignees: ''

---

**Describe the bug / 问题描述**
<!-- Please provide a short and clear description -->
<!-- 请提供简短准确的问题描述 -->

**To Reproduce / 如何复现**
<!-- Replace steps below with yours which causing the problem -->
<!-- 将下面的步骤替换为你出问题的步骤 -->

1. Do something first / 先做了什么
2. Then do another / 然后做了什么
3. Finally the software blowed up / 它炸了

**Expected behavior / 正确的行为**
<!-- A clear and concise description of what you expected to happen. -->
<!-- 描述一下你期望正常的行为应该是啥样的  -->


**Outputs / 程序输出**
<!-- If applicable, add logs to help explain your problem. -->
<!-- 如果可能的话，请提供日志输出截图一类有用的信息 -->

**Runtime Environment / 运行环境**
<!--
    For any unix-like (Linux, macOS, FreeBSD, ...):
        provide result of `uname -a` first,
    For Linux:
        result of `lsb-release -a`(if you have this command) or `cat /etc/os-release`(if you have this file) or describe the distro you used.
    For Windows:
        result of executing
            `wmic os get Caption,CSDVersion,OSArchitecture,OSLanguage,TotalVisibleMemorySize,Version /value`,
            and `wmic cpu get Caption,Name,NumberOfCores,NumberOfLogicalProcessors,Architecture /value` in Command prompt
-->
<!--
    对于任意类unix系统（Linux， macOS， FreeBSD等）：
        都提供下执行`uname -a`的结果；
    对于Linux：
        如果有`lsb-release -a`，提供它的执行结果，没有的话提供文件`/etc/os-release`的内容，也没有的话描述下你使用了什么发行版；
    对于Windows：
        在命令提示符（cmd）执行下
            `wmic os get Caption,CSDVersion,OSArchitecture,OSLanguage,TotalVisibleMemorySize,Version /value`，
            和 `wmic cpu get Caption,Name,NumberOfCores,NumberOfLogicalProcessors,Architecture /value`，
        提供下他们的结果
-->
OS:

```plain
fill here with your OS version / 在这填入你的OS信息
```

<!-- Provide your PHP version  -->
<!-- 提供下 PHP 版本  -->
PHP: <!-- e.g. 例如 "8.0.3 with debug, zend_thread_safe" / "php-8.0.3-nts-Win32-vs16-x64.zip" / php -v 结果 -->

<!-- Provide your Swow version  -->
<!-- 提供下 Swow 版本  -->
Swow: <!-- php --ri swow -->

**Additional context / 补充说明**
<!-- Add any other context about the problem here. -->
<!-- 有什么补充说明的写在这里 -->

