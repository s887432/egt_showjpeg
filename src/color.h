#ifndef __COLOR_H__
#define __COLOR_H__

/*
顏色分為背景色和字體色，30~39用來設置字體色，40~49設置背景：
背景色字體色
40: 黑30: 黑
41: 紅31: 紅
42: 綠32: 綠
43: 黃33: 黃
44: 藍34: 藍
45: 紫35: 紫
46: 深綠36: 深綠
47: 白色37: 白色

\033[0m 關閉所有屬性
\033[1m 設置高亮度
\033[4m 下劃線
\033[5m 閃爍
\033[7m 反顯
\033[8m 消隱
\033[30m -- \033[37m 設置前景色
\033[40m -- \033[47m 設置背景色
\033[nA 光標上移n行
\033[nB 光標下移n行
\033[nC 光標右移n行
\033[nD 光標左移n行
\033[y;xH設置光標位置
\033[2J 清屏
\033[K 清除從光標到行尾的內容
\033[s 保存光標位置
\033[u 恢復光標位置
\033[?25l 隱藏光標
\033[?25h 顯示光標s
*/

#define NONE			"\033[m"
#define RED				"\033[0;32;31m"
#define LIGHT_RED		"\033[1;31m"
#define GREEN			"\033[0;32;32m"
#define LIGHT_GREEN		"\033[1;32m"
#define BLUE			"\033[0;32;34m"
#define LIGHT_BLUE		"\033[1;34m"
#define DARY_GRAY		"\033[1;30m"
#define CYAN			"\033[0;36m"
#define LIGHT_CYAN		"\033[1;36m"
#define PURPLE			"\033[0;35m"
#define LIGHT_PURPLE	"\033[1;35m"
#define BROWN			"\033[0;33m"
#define YELLOW			"\033[1;33m"
#define LIGHT_GRAY		"\033[0;37m"
#define WHITE			"\033[1;37m"

#endif

// end of file

