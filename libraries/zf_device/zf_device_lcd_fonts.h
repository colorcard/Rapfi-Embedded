#ifndef _zf_device_lcd_fonts_h_
#define _zf_device_lcd_fonts_h_

#include <stdint.h>


// 字体相关结构定义
typedef struct _pFont
{    
	const uint8_t 		*pTable;  		//	字模数组地址
	uint16_t 			Width; 		 	//	单个字符的字模宽度
	uint16_t 			Height; 			//	单个字符的字模长度
	uint16_t 			Sizes;	 		//	单个字符的字模数据个数
	uint16_t				Table_Rows;		// 该参数只有汉字字模用到，表示二维数组的行大小
} pFONT;


/*------------------------------------ ASCII字体 ---------------------------------------------*/

extern pFONT ASCII_Font32;		// 3216 字体
extern pFONT ASCII_Font24;		// 2412 字体
extern pFONT ASCII_Font20; 	// 2010 字体
extern pFONT ASCII_Font16; 	// 1608 字体
extern pFONT ASCII_Font12; 	// 1206 字体

#endif 
 

