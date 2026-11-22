# ChineseChess

<div align=center>
<img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2021/logo.svg" height="240" width="240" />
</div>
<br>

## 项目介绍

当前课程设计版的 Linux GCC 构建、功能操作、规则说明、网络协议和自动测试见 [课程设计说明](COURSE_DESIGN.md)。

<p align="right"><br><a href="README.md">English</a> | <a href="README.zh_CN.md">简体中文</a></p>

![](https://img.shields.io/github/license/XMuli/chineseChess) ![](https://img.shields.io/github/v/release/XMuli/ChineseChess?style=flat&color=birightgreen)![](https://img.shields.io/badge/powered%20by-XMuli-ff69b4)![](https://img.shields.io/github/stars/XMuli/ChineseChess?style=social) ![](https://img.shields.io/github/forks/XMuli/ChineseChess?style=social&label=Fork)

[![macos-badge](https://github.com/xmuli/ChineseChess/workflows/MacOS/badge.svg)](https://github.com/xmuli/ChineseChess/workflows/MacOS/badge.svg)  [![win-badge](https://github.com/xmuli/ChineseChess/workflows/Windows/badge.svg)](https://github.com/xmuli/ChineseChess/workflows/Windows/badge.svg)  [![ubuntu-badge](https://github.com/xmuli/ChineseChess/workflows/Ubuntu/badge.svg)](https://github.com/xmuli/ChineseChess/workflows/Ubuntu/badge.svg)   ![](https://img.shields.io/github/languages/code-size/XMuli/ChineseChess)  ![](https://img.shields.io/github/downloads/XMuli/ChineseChess/total)

[<img src="https://get.microsoft.com/images/zh-cn%20dark.svg" width="260"/>](https://apps.microsoft.com/detail/9PB0KV6JJL87?mode=direct)

<br>

> - 基于 `Qt 5 / Qt 6` 和 C++17 开发的中国象棋桌面应用，支持本地双人对弈、人机对弈、残局挑战和局域网房间对弈。
>
> - 本项目收录于 [product](https://product.xmuli.tech)
> - 演示效果：https://www.bilibili.com/video/av45509758

<br>

## 局域网房间对弈

1. 两台电脑连接同一个局域网，运行相同的新版本程序。
2. 房主在主菜单选择“局域网联机”，点击“创建房间”，将六位数字房间号告诉对方。
3. 另一位玩家切换到“加入房间”，输入房间号。连接成功后双方自动进入棋盘，房主执红先行，加入者执黑。
4. 悔棋、重新开始和求和需要另一位玩家同意，15 秒内未确认会自动取消。断线时暂停对局，房主保持窗口打开，加入者可点击“重新连接”恢复棋谱，也可返回菜单用原房间号重新加入。

房间搜索使用 UDP 45454，走棋使用房主临时分配的 TCP 端口。系统询问防火墙权限时，允许程序在当前局域网通信。自动搜索失败时，可在加入页面填写房主显示的 IPv4 地址。同一 Wi-Fi 下的访客网络或客户端隔离也可能阻止通信。

房间号用于当前局域网发现，跨公网对弈需要额外的公网服务或网络配置。房主退出程序后房间关闭。

联机部分使用 Qt Network 与 C++17，可随项目使用 Linux GCC 工具链的 `g++` 编译，Qt Network 随 Qt Base 开发包提供。

## 特性

**该项目主要功能模块分为：**

ⅰ 本地双人对弈

ⅱ 人机对弈，支持简单、中等、困难三种难度

ⅲ 残局挑战

ⅳ 对战计时

ⅴ 悔棋

ⅵ 下棋轨迹与文本棋谱

<br>

# 运行演示：

<font color=#70AD47 size=4 face="幼圆">**视频演示：**</font> 

[ChineseChess 基于QT的跨平台网络象棋对战演示](https://www.bilibili.com/video/av45509758)

<br>

<font color=#D0087E size=4 face="幼圆">**win10系统演示：**</font>

<img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2021/20190709004321.gif"  height="500" width="800">

<br>

<font color=#D0087E size=4 face="幼圆">**Linux系统演示：**</font>

<img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2021/20190709004216.gif"  height="500" width="800">

<br>

<font color=#D0087E size=4 face="幼圆">**MacOS系统演示：**</font>

<img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2021/20190709004745.gif"  height="500" width="800">

<br>

<font color=#70AD47 size=4 face="幼圆">**跨平台对战演示：**</font>   **MacOS 10.14  vs  Win10**

<img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2021/20190709004454.gif"  height="500" width="800">

<br>

<font color=#FE7207  size=4 face="幼圆">**作者作品详情：** </font> 

 <img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2021/about.png" width="100%"/>

<br>

## 更新(2019-2026 v7.x)

### 功能

- 添加悔棋功能，无限制悔棋
- 显示下棋轨迹，当前执手
- 重新开始游戏，点击重开
- 代码逻辑优化，新增注释
- 下棋界面更新，使用古象棋棋字
- 更新关于程序，添加贡献者姓名
- Linux ARM64 deb 树莓派支持： [apt.raspbian-addons.org](https://apt.raspbian-addons.org/debian/pool/main/c/chinesechess/)

<br>

### 演示

<img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2021/ChineChess.gif" width="90%"/>

<img src="https://fastly.jsdelivr.net/gh/XMuli/xmuliPic@pic/2024/202401282117185.png" width="80%"/>

<br>

## 开发教程

- [项目实战：Qt5／C++：QT象棋【初版】](https://xmuli.blog.csdn.net//article/details/80931400) 

- [QT5/C++项目：基于QT的跨平台网络对战象棋（一）](https://xmuli.blog.csdn.net//article/details/89284983)

- [QT5/C++项目：基于QT的跨平台网络对战象棋（二）](https://xmuli.blog.csdn.net//article/details/89285968)

- [QT5/C++项目：基于QT的跨平台网络对战象棋（三）](https://xmuli.blog.csdn.net//article/details/89286553)

## 构建

> 本项目构建同时支持 QT 5 和 QT 6，更多项目集： https://product.xmuli.tech/



- **环境：** 本工程可以用 Qt 5.15.2 和 6.8.3 LTS 版本同时可编译通过；实测试最低构建 Qt 5.12，更低版本未验证
- IDE： 推荐直接只用 Qt Creator 打开 CMakeLists.txt 进行构建
- Other： 也可以自行 CMake 转 .sln 使用 Visual Studio 2022 构建



### Windows 构建

1. 安装 Qt （推荐 Qt 5.15 或者 Qt 6.8 LTS）
2. 直接使用 Qt Crator IDE 打开 CMakeLists.txt 
3. 点击左下角构建-编译
4. 可以看到成功运行，Enjoy~ 



### Linux / Mac 构建

1. 以 Debian 和 Ubuntu 为例，安装 Qt （推荐 Qt 5.15 或者 Qt 6.8 LTS）执行:

   ```bash
   # Ubuntu 20.04 STL， apt 默认 qt5.12.8
   $ sudo apt install cmake qtbase5-dev qt5-default libqt5svg5-dev qtmultimedia5-dev qttools5-dev libqt5x11extras5-dev
   
   
   # Ubuntu 24.04 STL， apt 默认 qt6.4.2
   $ sudo apt install cmake g++ vim qt6-base-dev qt6-tools-dev             qt6-svg-dev    qt6-multimedia-dev
   ```

2. 运行命令

   ```bash
   $ git clone https://github.com/XMuli/ChineseChess.git
   $ cd ChineseChess
   $ mkdir build && cd build
   $ cmake ..
   $ make
   ```

3. 或者，你可以跳过第 2 步，同样直接使用 Qt Creator 进行编译和运行，更加方便。

<br>

## 贡献者

感谢的小伙伴提交的贡献 🥳🥳:

<br>

<table frame=void>
    <tr>
      <th align="center" style="width: 80px;">
        <a href="https://github.com/XMuli">
          <img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2020/20201101220931.png" width="50px"><br>            
          <sub>XMuli</sub>
        </a>
      </th>        
      <th align="center" style="width: 80px;">
        <a href="https://github.com/Bruce-Ch">
          <img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2020/20200709204616.png" width="50px"><br>
          <sub>Bruce-Ch</sub>
        </a>
      </th>
      <th align="center" style="width: 80px;">
        <a href="https://github.com/BlueArvin">
          <img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2020/20200727212509.png" width="50px"><br>
          <sub>BlueArvin</sub>
        </a>
      </th>
         <th align="center" style="width: 80px;">
        <a href="https://github.com/Ubuntuser2012">
          <img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2020/20201101215738.png" width="50px"><br>
          <sub>Ubuntuser2012</sub>
        </a>
      </th>
         <th align="center" style="width: 80px;">
        <a href="https://github.com/kira-yamatoo">
          <img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2021/20210911163133.png" width="50px"><br>
          <sub>kirayamatoo</sub>
      </th>
         <th align="center" style="width: 80px;">
        <a href="https://github.com/hmsjy2017">
          <img src="https://cdn.jsdelivr.net/gh/xmuli/xmuliPic@pic/2021/20211227232947.png" width="50px"><br>
          <sub>hmsjy2017</sub>
        </a>
    </tr>
         <th align="center" style="width: 80px;">
        <a href="https://github.com/ryanfortner">
          <img src="https://cdn.jsdelivr.net/gh/XMuli/xmuliPic@pic/2022/71036629.png" width="50px"><br>
          <sub>ryanfortner</sub>
        </a>
       <th align="center" style="width: 80px;">
        <a href="https://github.com/zjuyk">
          <img src="https://fastly.jsdelivr.net/gh/XMuli/xmuliPic@pic/2024/202401282118007.png" width="50px"><br>
          <sub>zjuyk</sub>
        </a>
       <th align="center" style="width: 80px;">
        <a href="https://github.com/sum2012">
          <img src="https://fastly.jsdelivr.net/gh/XMuli/xmuliPic@pic/2026/202602050959732.png" width="50px"><br>
          <sub>sum2012</sub>
        </a>
       <th align="center" style="width: 80px;">
        <a href="https://openai.com/codex">
          <img src="https://fastly.jsdelivr.net/gh/XMuli/xmuliPic@pic/2026/202602051002745.png" width="50px"><br>
          <sub>Codex</sub>
        </a>
     </tr>
  </thead>
</table>




<br>

若是帮助到了你，或者觉得有用，<font color=#FE7207  size=4 face="幼圆">可以点击该项目的的 <font color=#D0087E size=4 face="幼圆">**⭐Star** </font>和<font color=#D0087E size=4 face="幼圆">**🍴 Fork**</font> 的两个图标，方便抬手之间，表示点个赞，手有余香，</font>其次才是一份冰的肥宅快乐水。

<br>

## 作者

[![alt text](https://img.shields.io/badge/QQ-%E5%81%95%E8%87%A7-brightgreen)](https://sighttp.qq.com/authd?IDKEY=31f3ef7312b39e2c8dc822ae2f4c3b3118e1a6f31cc83373) [![alt text](https://img.shields.io/badge/GitHub-XMuli-brightgreen)](https://github.com/XMuli) [![alt text](https://img.shields.io/badge/Blog-%E5%81%95%E8%87%A7%E7%9A%84%E5%B0%8F%E7%AB%99-ff69b4)](https://ifmet.cn/) 

<br>

## 系列教程

[QtExamples](https://github.com/XMuli/QtExamples)  欢迎 `star` ⭐ 和 `fork` 🍴 这个系列的 `C++ / QT / DTK` 学习，这里你可以学到如何亲自编写这类软件的经验，这是一系列完整的教程，并且**免费**！

