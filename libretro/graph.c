#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include "libretro.h"

#include "graph.h"
#include "libretro-glue.h"

void DrawFBoxBmp(unsigned short *buffer, int x, int y, int dx, int dy, unsigned short color)
{
   int i,j,idx;

   for(i=x;i<x+dx;i++)
   {
      for(j=y;j<y+dy;j++)
      {
         idx= i + j * retrow;
         buffer[idx]=color;	
      }
   }
}

void DrawFBoxBmp32(uint32_t *buffer, int x, int y, int dx, int dy, uint32_t color)
{
   int i,j,idx;

   for(i=x;i<x+dx;i++)
   {
      for(j=y;j<y+dy;j++)
      {
         idx= i + j * retrow;
         buffer[idx]=color;	
      }
   }
}

void DrawBoxBmp(unsigned short *buffer, int x, int y, int dx, int dy, unsigned short color)
{
   int i,j,idx;

   for(i=x;i<x+dx;i++)
   {
      idx=i+y*retrow;
      buffer[idx]=color;
      idx=i+(y+dy)*retrow;
      buffer[idx]=color;
   }

   for(j=y;j<y+dy;j++)
   {
      idx=x+j*retrow;
      buffer[idx]=color;	
      idx=(x+dx)+j*retrow;
      buffer[idx]=color;	
   }
}

void DrawBoxBmp32(uint32_t *buffer, int x, int y, int dx, int dy, uint32_t color)
{
   int i,j,idx;

   for(i=x;i<x+dx;i++)
   {
      idx=i+y*retrow;
      buffer[idx]=color;
      idx=i+(y+dy)*retrow;
      buffer[idx]=color;
   }

   for(j=y;j<y+dy;j++)
   {
      idx=x+j*retrow;
      buffer[idx]=color;	
      idx=(x+dx)+j*retrow;
      buffer[idx]=color;	
   }
}

void DrawPointBmp(unsigned short *buffer, int x, int y, unsigned short color)
{
   int idx;

   idx = x + y * retrow;
   buffer[idx]=color;
}

void DrawHlineBmp(unsigned short *buffer, int x, int y, int dx, int dy, unsigned short color)
{
   int i,j,idx;

   (void)j;

   for(i=x;i<x+dx;i++)
   {
      idx=i+y*retrow;
      buffer[idx]=color;		
   }
}

void DrawVlineBmp(unsigned short *buffer, int x, int y, int dx, int dy, unsigned short color)
{
   int i,j,idx;

   (void)i;

   for(j=y;j<y+dy;j++)
   {
      idx=x+j*retrow;
      buffer[idx]=color;		
   }	
}

void DrawlineBmp(unsigned short *buffer, int x1, int y1, int x2, int y2, unsigned short color)
{
   int pixx, pixy, x, y, dx, dy, sx, sy, swaptmp, idx;

   dx = x2 - x1;
   dy = y2 - y1;
   sx = (dx >= 0) ? 1 : -1;
   sy = (dy >= 0) ? 1 : -1;

   if (dx == 0)
   {
      if (dy > 0)
      {
         DrawVlineBmp(buffer, x1, y1, 0, dy, color);
         return;
      }
      else if (dy < 0)
      {
         DrawVlineBmp(buffer, x1, y2, 0, -dy, color);
         return;
      }
      else
      {
         idx=x1+y1*retrow;
         buffer[idx]=color;
         return;
      }
   }

   if (dy == 0)
   {
      if (dx > 0)
      {
         DrawHlineBmp(buffer, x1, y1, dx, 0, color);
         return;
      }
      else if (dx < 0)
      {
         DrawHlineBmp(buffer, x2, y1, -dx,0, color);
         return;
      }
   }

   dx = sx * dx + 1;
   dy = sy * dy + 1;

   pixx = 1;
   pixy = retrow;

   pixx *= sx;
   pixy *= sy;

   if (dx < dy)
   {
      swaptmp = dx;
      dx = dy;
      dy = swaptmp;
      swaptmp = pixx;
      pixx = pixy;
      pixy = swaptmp;
   }

   x = 0;
   y = 0;

   idx=x1+y1*retrow;

   for (; x < dx; x++, idx +=pixx)
   {
      buffer[idx]=color;
      y += dy;
      if (y >= dx)
      {
         y -= dx;
         idx += pixy;
      }
   }
}

#include "font2.c"

void Draw_string(unsigned short *surf, signed short int x, signed short int y,
      const char *string, unsigned short maxstrlen,
      unsigned short xscale, unsigned short yscale,
      unsigned short fg, unsigned short bg)
{
   int k,strlen;
   int xrepeat, yrepeat;
   unsigned char *linesurf;
   signed short int ypixel;
   unsigned short *yptr; 
   int col, bit;
   unsigned char b;

   (void)k;

   if(!string)
      return;

   for(strlen = 0; strlen < maxstrlen && string[strlen]; strlen++) {}

   int surfw = strlen * 7 * xscale;
   int surfh = 8 * yscale;

   linesurf = (unsigned char*)malloc(sizeof(unsigned short)*surfw*surfh);

   yptr = (unsigned short *)&linesurf[0];

   for(ypixel = 0; ypixel < 8; ypixel++)
   {
      for(col = 0; col < strlen; col++)
      {
         b = font_array[(((unsigned char)string[col])^0x80)*8 + ypixel];

         for(bit = 0; bit < 7; bit++, yptr++)
         {
            *yptr = (b & (1<<(7-bit))) ? fg : bg;
            for(xrepeat = 1; xrepeat < xscale; xrepeat++, yptr++)
               yptr[1] = *yptr;
         }
      }

      for(yrepeat = 1; yrepeat < yscale; yrepeat++)
         for(xrepeat = 0; xrepeat < surfw; xrepeat++, yptr++)
            *yptr = yptr[-surfw];
   }

   yptr = (unsigned short*)&linesurf[0];

   for(yrepeat = y; yrepeat < y+surfh; yrepeat++)
      for(xrepeat = x; xrepeat < x+surfw; xrepeat++, yptr++)
         if(*yptr != 0) surf[xrepeat+yrepeat*retrow] = *yptr;

   free(linesurf);
}

void Draw_string32(uint32_t *surf, signed short int x, signed short int y,
      const char *string, unsigned short maxstrlen,
      unsigned short xscale, unsigned short yscale,
      uint32_t fg, uint32_t bg)
{
   int k,strlen;
   int xrepeat, yrepeat;
   uint32_t *linesurf;
   signed short int ypixel;
   uint32_t *yptr;
   int col, bit;
   unsigned char b;

   (void)k;

   if(!string)
      return;

   for(strlen = 0; strlen < maxstrlen && string[strlen]; strlen++) {}

   int surfw = strlen * 7 * xscale;
   int surfh = 8 * yscale;

   linesurf = (uint32_t *)malloc(sizeof(uint32_t)*surfw*surfh);

   yptr = (uint32_t *)&linesurf[0];

   for(ypixel = 0; ypixel < 8; ypixel++)
   {
      for(col = 0; col < strlen; col++)
      {
         b = font_array[(((unsigned char)string[col])^0x80)*8 + ypixel];

         for(bit = 0; bit < 7; bit++, yptr++)
         {
            *yptr = (b & (1<<(7-bit))) ? fg : bg;
            for(xrepeat = 1; xrepeat < xscale; xrepeat++, yptr++)
               yptr[1] = *yptr;
         }
      }

      for(yrepeat = 1; yrepeat < yscale; yrepeat++)
         for(xrepeat = 0; xrepeat < surfw; xrepeat++, yptr++)
            *yptr = yptr[-surfw];

   }

   yptr = (uint32_t *)&linesurf[0];

   for(yrepeat = y; yrepeat < y+surfh; yrepeat++)
      for(xrepeat = x; xrepeat < x+surfw; xrepeat++, yptr++)
         if(*yptr != 0) surf[xrepeat+yrepeat*retrow] = *yptr;

   free(linesurf);
}

void Draw_text(unsigned short *buffer, int x, int y,
      unsigned short fgcol, unsigned short bgcol,
      int scalex, int scaley, int max, char *string, ...)
{
   int boucle=0;
   char text[256];
   va_list ap;

   (void)boucle;

   if (string == NULL)
      return;

   va_start(ap, string);
   vsnprintf(text, sizeof(text), string, ap);
   va_end(ap);

   Draw_string(buffer, x, y, text, max, scalex, scaley, fgcol, bgcol);
}

void Draw_text32(uint32_t *buffer, int x, int y,
      uint32_t fgcol, uint32_t bgcol,
      int scalex, int scaley, int max, char *string,...)
{
   int boucle=0;
   char text[256];
   va_list ap;

   (void)boucle;

   if (string == NULL)
      return;

   va_start(ap, string);
   vsnprintf(text, sizeof(text), string, ap);
   va_end(ap);

   Draw_string32(buffer, x, y, text, max, scalex, scaley, fgcol, bgcol);
}
