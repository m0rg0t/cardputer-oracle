# 安装 Cardputer Oracle 简体中文版

适用于 **M5Stack Cardputer ADV（ESP32-S3）** 和 M5Apps 启动器。
请使用 FAT32 格式的 microSD 卡。原版 Cardputer 不在支持范围内。

1. 解压 `Oracle-ADV-0.6.0-zh.zip`。
2. 将 `ORACLE` 文件夹复制到存储卡根目录。更新时保留自己的
   `ORACLE/config.json` 和网络设置，替换其他资源。
3. 将压缩包中的 `.bin` 文件复制到卡根目录。
4. 在 M5Apps 中选择 **Installer → SD**，安装该 BIN 文件。
   不要选择名称以 `._` 开头的 macOS 文件。
5. 启动应用，确认顶部显示 `SD`。

## 内容与字体

包含 120 条中文趣味预言：科技与日常 60 条、啤酒 30 条、茶 30 条，
配有普通话语音和可打印图片。屏幕、纸质小票和塔罗解读均使用简体中文。
英文仍是项目的默认构建语言；请同时更换固件与对应语言的资源包。

中文字体使用可再分发的 /efont/ 12 像素点阵字库。
界面和离线预言需要的字形内置于固件；完整字库位于
`ORACLE/fonts/chinese12.bin`，供在线塔罗的额外汉字使用。
请勿删除该文件。字体许可随包附在 `third_party/Efont-LICENSE`。
无可用资源卡时，每个离线主题仍有 6 条无语音的备用预言。

## 操作

- 首页左右键切换主题，其他键开始。逗号和斜杠键无需 Fn 即可切换主题。
- 结果页：N / Enter / 空格重新抽取，P 打印，R 重播语音，I 查看图片。
- ; / W 与 . / S 滚动，A 切换自动滚动，M 静音，+ / - 调整音量。
- Esc 返回；键盘左上角的反引号键也可返回。
- 塔罗问题目前使用英文或拼音输入，尚无中文输入法；直接 Enter 可获取通用解读。
  解读始终为中文。Tab 可切换英文与俄文键盘映射，不会切换应用语言。

## 联网与打印

三个离线主题无需网络或账号。在线塔罗需要在卡上的私有 `ORACLE/config.json`
中配置 2.4 GHz Wi-Fi、可访问的 HTTPS LiteLLM 服务、模型和令牌。
可从 `config.example.json` 复制模板。不要公开真实配置或提交到 Git。
在线塔罗提供文字与小票，目前没有实时语音。

BQ95 相机需停留在 FunPrint 二维码界面，并关闭自动关机。
显示预言时会尝试自动打印一次；P 可重试或再打印一份。
中断后不会自动重印，请先检查纸张。图片宽 336 点，小票宽 384 点。

此语言版本经过软件测试；实机显示、扬声器和蓝牙打印仍需实际确认。

## 首次使用：先安装启动器

如果设备已有 M5Apps 和 Installer，可跳过本节。否则需要一根支持数据传输的
USB-C 线，通过 [M5Burner](https://docs.m5stack.com/en/download) 安装支持 ADV 的
M5Apps。入口为 CardPuter 分类中的 M5Apps，具体操作参见
[M5Apps 安装说明](https://github.com/d4rkmen/M5Apps#installation)。
安装启动器会替换设备原有固件，请先保存需要保留的数据。

设备能充电不等于线缆能传输数据。电脑不显示端口时，尝试另一根数据线或直接
连接电脑；下载模式请参照 [M5Stack ADV 说明](https://docs.m5stack.com/en/uiflow2/cardputer-adv/program)。
Oracle BIN 是应用镜像，不能作为包含引导程序和分区表的完整镜像烧写到 0x0。
M5Apps 安装完成后，日常安装或更新 Oracle 只需读卡器，无需 USB 数据线。
安装现成 ZIP 不需要 Python、PlatformIO 或 ElevenLabs 账号。

## 准备存储卡与检查目录

1. 关闭 Cardputer，再取出 microSD 并连接读卡器。
2. 先备份原有内容。使用 FAT32；格式化会删除数据，已正常使用的 FAT32 卡无需重格式化。
   确认选中的是 microSD，而不是电脑上的其他磁盘。
3. 在电脑上解压所选 ZIP，并确认存储卡有足够空间存放解压后的内容。
4. 将整个 ORACLE 文件夹和同一 ZIP 的安装 BIN 复制到卡根目录。
5. 安全弹出卡，插入已关机的设备，再开机。
6. 在 M5Apps → Installer → SD 中选中安装 BIN 并完成提示步骤，然后启动应用。
   仅复制 BIN 到卡上并不等于安装；仅复制资源也不会更新固件。
7. 检查顶部 SD 标识、中文文字、图片与离线语音。

中文资源的正确位置：

```text
microSD 根目录/
├── Oracle-ADV-0.6.0-zh.bin
└── ORACLE/
    ├── index.json
    ├── cards/coat.json
    ├── images/… .jpg
    ├── audio/zh/coat.wav
    ├── print/coat.bin
    ├── fonts/chinese12.bin
    ├── config.example.json
    ├── tls/roots.pem
    └── config.json          # 可选，由用户创建
```

请勿额外套一层 ZIP 名称文件夹，也不要形成 ORACLE/ORACLE/。
卡的卷名可自行命名。英文语音路径是 audio/en/，俄文是 audio/ 下直接放 WAV，
没有 ru/ 子目录。请按各自压缩包的原始目录复制，不要混装。
仓库的 sdcard/ 是素材与旧俄文资源目录；安装应使用发布 ZIP，或构建生成的
build/packs/zh/ORACLE。

| ORACLE 内的路径 | 用途 |
| --- | --- |
| index.json | 语言、卡片 ID 和主题索引，必须与固件匹配 |
| cards/*.json | 每张预言的文字、标题与资源路径 |
| images/*.jpg | 屏幕图片 |
| audio/zh/*.wav | 120 条普通话录音 |
| print/*.bin | 离线小票点阵数据，不是固件 |
| fonts/chinese12.bin | 在线塔罗额外汉字字库，不是固件 |
| config.example.json | 无真实密钥的公开配置模板 |
| config.json | 私有 Wi-Fi 密码与 LiteLLM 令牌，离线使用不需要 |
| tls/roots.pem | HTTPS 信任根证书，塔罗需要 |
| diagnostic.txt | 设备生成的启动和重置诊断 |

不要在 Installer 中安装 print/ 或 fonts/ 内的 BIN。安装后程序运行于设备闪存，
根目录的安装 BIN 可保留用于重装，也可移除；正常使用需保留 ORACLE 资源。
ZIP 中的 README.md、SHA256SUMS 和 third_party/ 用于说明、校验和许可，可留在电脑上。
源代码目录中 WAV 旁的元数据 JSON 不需要复制到卡。按键和仪式音效由固件生成。

## 更新、换语言与恢复旧版

保留之前正常工作的 ZIP，并备份现有 ORACLE，尤其是 config.json。
用新 ZIP 的完整 ORACLE 替换资源，再将私有 config.json 放回新目录。
文件管理器的“替换文件夹”可能删除原内容，请先完成备份。
保留卡上其他 M5Apps 文件夹，并安装同一新 ZIP 的 BIN。
恢复旧版时，旧 BIN 和旧资源必须一起恢复。普通更新不需重装 M5Apps。

同一 /ORACLE 路径只能使用一种语言资源。切换语言需要同时更新 BIN 和文件夹，
不是在应用菜单中切换。错误语言或不可读的卡会显示 DEMO，并使用内置无语音预言。

## 配置与排错

将 config.example.json 复制成 config.json，替换所有占位值，保存为 UTF-8 纯文本 JSON。
不要添加 JSON 注释，也不要误存为 config.json.txt。wifi 支持按偏好排列的 1–4 个网络。
host 仅填写域名，不含 https://、端口或路径；请求路径为 /v1/responses。
model 必须是该代理令牌可用的准确模型名。M5Apps 的 Wi-Fi 设置不能替代 Oracle 配置。
Oracle 目前没有手动选网、输入新密码并保存的设置界面。

| 现象 | 检查方法 |
| --- | --- |
| Chip id mismatch | 确认 ADV/ESP32-S3、正确的 Oracle 安装 BIN 和完整下载；不要强制忽略芯片检查 |
| 显示 DEMO | 检查 FAT32、卡接触、根目录 ORACLE/index.json，以及固件与资源语言一致 |
| 没有语音 | 检查 SD 标识，M 取消静音，+ 提高音量，离线结果按 R；在线塔罗没有实时语音 |
| 汉字缺失 | 从中文 ZIP 恢复 fonts/chinese12.bin，不要重命名 |
| 找不到 Wi-Fi | 热点已打开，SSID 完全一致，提供 2.4 GHz；支持的 iPhone 可尝试“最大兼容性” |
| 塔罗失败 | 检查 config.json、代理连通性、模型权限、令牌、根证书和网络时间同步 |
| 打印失败 | BQ95 停留 FunPrint 二维码页，关闭自动关机，检查电量和纸，再按 P |
| 打印时 NVS 错误 | 更新 Oracle BIN；不要首先清空启动器的 NVS 设置 |
| 抽取后重启 | 更新匹配的固件和资源，检查电量与卡，并保存 diagnostic.txt |

反馈问题时说明语言、版本、错误原文及发生阶段；请勿上传包含密码和令牌的 config.json。
