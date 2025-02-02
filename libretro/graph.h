#ifndef GRAPH_H
#define GRAPH_H 1

typedef struct
{
	int x, y;
	int dx, dy;
} box;

extern void DrawFBoxBmp(unsigned short *buffer, int x, int y, int dx, int dy, unsigned short color);
extern void DrawFBoxBmp32(uint32_t *buffer, int x, int y, int dx, int dy, uint32_t color);

extern void DrawBoxBmp(unsigned short *buffer, int x, int y, int dx, int dy, unsigned short color);
extern void DrawBoxBmp32(uint32_t *buffer, int x, int y, int dx, int dy, uint32_t color);

extern void DrawPointBmp(unsigned short *buffer, int x, int y, unsigned short color);
extern void DrawHlineBmp(unsigned short *buffer, int x, int y, int dx, int dy, unsigned short color);
extern void DrawVlineBmp(unsigned short *buffer, int x, int y, int dx, int dy, unsigned short color);
extern void DrawlineBmp(unsigned short *buffer, int x1, int y1, int x2, int y2, unsigned short color);

extern void Draw_string(unsigned short *surf, signed short int x, signed short int y, const char *string, unsigned short int maxstrlen, unsigned short int xscale, unsigned short int yscale, unsigned short int fg, unsigned short int bg);
extern void Draw_string32(uint32_t *surf, signed short int x, signed short int y, const char *string, unsigned short int maxstrlen, unsigned short int xscale, unsigned short int yscale, uint32_t fg, uint32_t bg);

extern void Draw_text(unsigned short *buffer, int x, int y, unsigned short fgcol, unsigned short int bgcol, int scalex, int scaley, int max, char *string, ...);
extern void Draw_text32(uint32_t *buffer, int x, int y, uint32_t fgcol, uint32_t bgcol, int scalex, int scaley, int max, char *string, ...);

#endif

