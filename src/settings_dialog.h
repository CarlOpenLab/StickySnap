#pragma once
// 设置对话框：原生 Win32 控件 + 自带模态消息循环（仓库无 dialog 资源脚本）
#include "settings.h"

// 模态显示设置窗口，owner 可为 nullptr。
// 返回 true  : 用户点了“保存”，*s 已写回新配置并落盘（config.ini）。
// 返回 false : 取消 / 关闭窗口（或创建失败），*s 保持原值。
bool ShowSettingsDialog(HWND owner, Settings* s);
