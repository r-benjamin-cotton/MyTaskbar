// header.h : 標準のシステム インクルード ファイルのインクルード ファイル、
// またはプロジェクト専用のインクルード ファイル
//

#pragma once

#include "targetver.h"
//#define WIN32_LEAN_AND_MEAN             // Windows ヘッダーからほとんど使用されていない部分を除外する
// Windows ヘッダー ファイル
#include <windows.h>
#include <windowsx.h>
// C ランタイム ヘッダー ファイル
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <stdio.h>
#include <stdarg.h>

#include <string>
#include <vector>
//#include <list>
#include <map>

#include <functional>

#pragma comment (lib, "Winmm.lib")

#include <shellscalingapi.h>
#pragma comment (lib, "Shcore.lib")

#include <gdiplus.h>
#pragma comment (lib, "Gdiplus.lib")

#include <uxtheme.h>
#include <vssym32.h>
#pragma comment (lib, "UxTheme.lib")


#include"debug.h"
