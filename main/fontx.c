/*
	Библиотека формирования шрифтов
	для дисплея ST7789
*/
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "fontx.h"

#define FontxDebug 0 // for Debug

//-------------------------------------------------------------------------------------
void AddFontx(FontxFile *fx, const char *path)
{
    memset(fx, 0, sizeof(FontxFile));
    fx->path = path;
    fx->opened = false;
}
//-------------------------------------------------------------------------------------
void InitFontx(FontxFile *fxs, const char *f0, const char *f1)
{
    AddFontx(&fxs[0], f0);
    AddFontx(&fxs[1], f1);
}
//-------------------------------------------------------------------------------------
bool OpenFontx(FontxFile *fx)
{
FILE *f;

		if (!fx->opened) {
			if (FontxDebug) printf("[openFont]fx->path=[%s]\n", fx->path);
	    f = fopen(fx->path, "r");
	    if (FontxDebug) printf("[openFont]fopen=%p\n", f);
	    if (!f) {
				fx->valid = false;
				printf("Fontx:%s not found.\n", fx->path);
				return fx->valid ;
	    }
	    fx->opened = true;
	    fx->file = f;
	    char buf[18];
	    if (fread(buf, 1, sizeof(buf), fx->file) != sizeof(buf)) {
				fx->valid = false;
				printf("Fontx:%s not FONTX format.\n", fx->path);
				fclose(fx->file);
				return fx->valid ;
	    }

	    if (FontxDebug) {
				for (int i = 0; i < sizeof(buf); i++) {
		    	printf("buf[%d]=0x%x\n", i, buf[i]);
				}
	    }
	    memcpy(fx->fxname, &buf[6], 8);
	    fx->w = buf[14];
	    fx->h = buf[15];
	    fx->is_ank = (buf[16] == 0);
	    fx->bc = buf[17];
	    fx->fsz = (fx->w + 7)/8 * fx->h;
	    if (fx->fsz > FontxGlyphBufSize) {
				printf("Fontx:%s is too big font size.\n", fx->path);
				fx->valid = false;
				fclose(fx->file);
				return fx->valid ;
	    }
	    fx->valid = true;
		}

		return fx->valid;
}
//-------------------------------------------------------------------------------------
// フォントファイルをCLOSE
void CloseFontx(FontxFile *fx)
{
    if (fx->opened) {
			fclose(fx->file);
			fx->opened = false;
    }
}
//-------------------------------------------------------------------------------------
// フォント構造体の表示
void DumpFontx(FontxFile *fxs)
{
    for (int i = 0; i < 2; i++) {
			printf("fxs[%d]->path=%s\n", i, fxs[i].path);
			printf("fxs[%d]->opened=%d\n", i, fxs[i].opened);
			printf("fxs[%d]->fxname=%s\n", i, fxs[i].fxname);
			printf("fxs[%d]->valid=%d\n", i, fxs[i].valid);
			printf("fxs[%d]->is_ank=%d\n", i, fxs[i].is_ank);
			printf("fxs[%d]->w=%d\n", i, fxs[i].w);
			printf("fxs[%d]->h=%d\n", i, fxs[i].h);
			printf("fxs[%d]->fsz=%d\n", i, fxs[i].fsz);
			printf("fxs[%d]->bc=%d\n", i, fxs[i].bc);
    }
}
//-------------------------------------------------------------------------------------
uint8_t getFontWidth(FontxFile *fx)
{
    if (FontxDebug) printf("fx->w=%d\n",fx->w);

    return (fx->w);
}
//-------------------------------------------------------------------------------------
uint8_t getFontHeight(FontxFile *fx)
{
    if (FontxDebug) printf("fx->h=%d\n",fx->h);

    return (fx->h);
}
//-------------------------------------------------------------------------------------
bool GetFontx(FontxFile *fxs, uint8_t ascii , uint8_t *pGlyph, uint8_t *pw, uint8_t *ph)
{
int i;
uint32_t offset;

  if (FontxDebug) printf("[GetFontx]ascii=0x%x\n", ascii);

	for (i = 0; i < 2; i++) {
		if (!OpenFontx(&fxs[i])) continue;
		if (FontxDebug) printf("[GetFontx]openFontxFile[%d] ok\n",i);
	
		if (ascii < 0xFF) {
			if (fxs[i].is_ank) {
				if (FontxDebug) printf("[GetFontx]fxs.is_ank fxs.fsz=%d\n",fxs[i].fsz);
				offset = 17 + ascii * fxs[i].fsz;
				if (FontxDebug) printf("[GetFontx]offset=%d\n",offset);
				if (fseek(fxs[i].file, offset, SEEK_SET)) {
					printf("Fontx:seek(%u) failed.\n",offset);
					return false;
				}

				if (fread(pGlyph, 1, fxs[i].fsz, fxs[i].file) != fxs[i].fsz) {
				  printf("Fontx:fread failed.\n");
					return false;
				}
				if (pw) *pw = fxs[i].w;
				if (ph) *ph = fxs[i].h;
				return true;
			}
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------
void Font2Bitmap(uint8_t *fonts, uint8_t *line, uint8_t w, uint8_t h, uint8_t inverse)
{
int x,y;

	for (y = 0; y < (h / 8); y++) {
		for (x = 0; x < w; x++) line[y * 32 + x] = 0;
	}

	int mask = 7;
	int fontp;
	fontp = 0;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			uint8_t d = fonts[fontp + x / 8];
			uint8_t linep = (y / 8) * 32 + x;
			if (d & (0x80 >> (x % 8))) line[linep] = line[linep] + (1 << mask);
		}
		mask--;
		if (mask < 0) mask = 7;
		fontp += (w + 7) / 8;
	}

	if (inverse) {
		for (y = 0; y < (h / 8); y++) {
			for (x = 0; x < w; x++) line[y * 32 + x] = RotateByte(line[y * 32 + x]);
		}
	}
}
//-------------------------------------------------------------------------------------
// アンダーラインを追加
void UnderlineBitmap(uint8_t *line, uint8_t w, uint8_t h)
{
	int x,y;
	uint8_t wk;
	for (y = 0; y < (h / 8); y++) {
		for (x = 0; x < w; x++) {
			wk = line[y * 32 + x];
			if ( (y + 1) == (h / 8)) line[y * 32 + x] = wk + 0x80;
		}
	}
}
//-------------------------------------------------------------------------------------
// ビットマップを反転
void ReversBitmap(uint8_t *line, uint8_t w, uint8_t h)
{
	int x,y;
	uint8_t wk;
	for (y = 0; y < (h / 8); y++) {
		for (x = 0; x < w; x++) {
			wk = line[y * 32 + x];
			line[y * 32 + x] = ~wk;
		}
	}
}
//-------------------------------------------------------------------------------------
// フォントパターンの表示
void ShowFont(uint8_t *fonts, uint8_t pw, uint8_t ph)
{
	int x, y, fpos;
	printf("[ShowFont pw=%d ph=%d]\n", pw, ph);
	fpos = 0;
	for (y = 0;y < ph; y++) {
		printf("%02d", y);
		for (x = 0;x < pw; x++) {
			if (fonts[fpos + x / 8] & (0x80 >> (x % 8))) {
			printf("*");
			} else {
				printf(".");
			}
		}
		printf("\n");
		fpos = fpos + (pw + 7) / 8;
	}
	printf("\n");
}
//-------------------------------------------------------------------------------------
// Bitmapの表示
void ShowBitmap(uint8_t *bitmap, uint8_t pw, uint8_t ph)
{
	int x, y, fpos = 0;
	printf("[ShowBitmap pw=%d ph=%d]\n", pw, ph);
#if 0
	for (y = 0; y < (ph + 7) / 8; y++) {
		for (x = 0; x < pw; x++) {
			printf("%02x ",bitmap[x + y * 32]);
		}
		printf("\n");
	}
#endif

	for (y = 0; y < ph; y++) {
		printf("%02d", y);
		for (x = 0; x < pw; x++) {
//printf("b=%x m=%x\n",bitmap[x+(y/8)*32],0x80 >> fpos);
			if (bitmap[x + (y / 8) * 32] & (0x80 >> fpos)) {
				printf("*");
			} else {
				printf(".");
			}
		}
		printf("\n");
		fpos++;
		if (fpos > 7) fpos = 0;
	}
	printf("\n");
}
//-------------------------------------------------------------------------------------
// 8ビットデータを反転
uint8_t RotateByte(uint8_t ch1)
{
	uint8_t ch2 = 0;
	int j;
	for (j = 0; j < 8; j++) {
		ch2 = (ch2 << 1) + (ch1 & 0x01);
		ch1 = ch1 >> 1;
	}
	return ch2;
}
//-------------------------------------------------------------------------------------
#if 0
// UTF code(3Byte) を SJIS Code(2 Byte) に変換
// https://www.mgo-tec.com/blog-entry-utf8sjis01.html
uint16_t UTF2SJIS(spiffs_file fd, uint8_t *utf8) {

  uint32_t offset = 0;
  uint32_t ret;
  uint32_t UTF8uint = utf8[0]*256*256 + utf8[1]*256 + utf8[2];
   
  if(utf8[0]>=0xC2 && utf8[0]<=0xD1){ //0xB0からS_JISコード実データ。0x00-0xAFまではライセンス文ヘッダなのでそれをカット。
    offset = ((utf8[0]*256 + utf8[1])-0xC2A2)*2 + 0xB0; //文字"￠" UTF8コード C2A2～、S_jisコード8191
  }else if(utf8[0]==0xE2 && utf8[1]>=0x80){
    offset = (UTF8uint-0xE28090)*2 + 0x1EEC; //文字"‐" UTF8コード E28090～、S_jisコード815D
  }else if(utf8[0]==0xE3 && utf8[1]>=0x80){
    offset = (UTF8uint-0xE38080)*2 + 0x9DCC; //スペース UTF8コード E38080～、S_jisコード8140
  }else if(utf8[0]==0xE4 && utf8[1]>=0x80){
    offset = (UTF8uint-0xE4B880)*2 + 0x11CCC; //文字"一" UTF8コード E4B880～、S_jisコード88EA
  }else if(utf8[0]==0xE5 && utf8[1]>=0x80){
    offset = (UTF8uint-0xE58085)*2 + 0x12BCC; //文字"倅" UTF8コード E58085～、S_jisコード98E4
  }else if(utf8[0]==0xE6 && utf8[1]>=0x80){
    offset = (UTF8uint-0xE6808E)*2 + 0x1AAC2; //文字"怎" UTF8コード E6808E～、S_jisコード9C83
  }else if(utf8[0]==0xE7 && utf8[1]>=0x80){
    offset = (UTF8uint-0xE78081)*2 + 0x229A6; //文字"瀁" UTF8コード E78081～、S_jisコードE066
  }else if(utf8[0]==0xE8 && utf8[1]>=0x80){
    offset = (UTF8uint-0xE88080)*2 + 0x2A8A4; //文字"耀" UTF8コード E88080～、S_jisコード9773
  }else if(utf8[0]==0xE9 && utf8[1]>=0x80){
    offset = (UTF8uint-0xE98080)*2 + 0x327A4; //文字"退" UTF8コード E98080～、S_jisコード91DE
  }else if(utf8[0]>=0xEF && utf8[1]>=0xBC){
    offset = (UTF8uint-0xEFBC81)*2 + 0x3A6A4; //文字"！" UTF8コード EFBC81～、S_jisコード8149
    if(utf8[0]==0xEF && utf8[1]==0xBD && utf8[2]==0x9E){
      offset = 0x3A8DE; // "～" UTF8コード EFBD9E、S_jisコード8160
    }
  }

if(FontxDebug)printf("[UTF2SJIS] offset=%d\n",offset);
  char buf[2];
  ret = SPIFFS_lseek(&fs, fd, offset, SPIFFS_SEEK_SET);
if(FontxDebug)printf("[UTF2SJIS] lseek ret=%d\n",ret);
  if (ret != offset) {
    printf("UTF2SJIS:seek(%u) failed.\n",offset);
    return 0;
  }
  if (SPIFFS_read(&fs, fd, buf, sizeof(buf)) != sizeof(buf)) {
    printf("UTF2SJIS:read failed.\n");
    return 0;
  }
if(FontxDebug)printf("[UTF2SJIS] sjis=0x%x%x\n",buf[0],buf[1]);
  return buf[0]*256+buf[1];
}


// UTFを含む文字列をSJISに変換
int String2SJIS(spiffs_file fd, unsigned char *str_in, size_t stlen,
		uint16_t *sjis, size_t ssize) {
  int i;
  uint8_t sp;
  uint8_t c1 = 0;
  uint8_t c2 = 0;
  uint8_t utf8[3];
  uint16_t sjis2;
  int spos = 0;

  for(i=0;i<stlen;i++) {
    sp = str_in[i];
if(FontxDebug)printf("[String2SJIS]sp[%d]=%x\n",i,sp);
    if ((sp & 0xf0) == 0xe0) { // 上位4ビットが1110なら、3バイト文字の1バイト目
      c1 = sp;
    } else if ((sp & 0xc0) == 0x80) { // 上位2ビットが10なら、他バイト文字の2バイト目以降
      if (c2 == 0) {
        c2 = sp;
      } else {
        if (c1 == 0xef && c2 == 0xbd) {
if(FontxDebug)printf("[String2SJIS]hankaku kana %x-%x-%x\n",c1,c2,sp);
          sjis2 = sp;
if(FontxDebug)printf("[String2SJIS]sjis2=%x\n",sjis2);
          if (spos < ssize) sjis[spos++] = sjis2;
        } else if (c1 == 0xef && c2 == 0xbe) {
if(FontxDebug)printf("[String2SJIS]hankaku kana %x-%x-%x\n",c1,c2,sp);
          sjis2 = 0xc0 + (sp - 0x80);
if(FontxDebug)printf("[String2SJIS]sjis2=%x\n",sjis2);
          if (spos < ssize) sjis[spos++] = sjis2;
        } else {
if(FontxDebug)printf("[String2SJIS]UTF8 %x-%x-%x\n",c1,c2,sp);
          utf8[0] = c1;
          utf8[1] = c2;
          utf8[2] = sp;
          sjis2 = UTF2SJIS(fd, utf8);
if(FontxDebug)printf("[String2SJIS]sjis2=%x\n",sjis2);
          if (spos < ssize) sjis[spos++] = sjis2;
        }
        c1 = c2 = 0;
      }
    } else if ((sp & 0x80) == 0) { // 1バイト文字の場合
if(FontxDebug)printf("[String2SJIS]ANK %x\n",sp);
        if (spos < ssize) sjis[spos++] = sp;
    }
  }
  return spos;
}
#endif

