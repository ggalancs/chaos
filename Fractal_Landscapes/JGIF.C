#include "gif.h"
#include <stdio.h>
#include <dos.h>
#include <mem.h>
#include <string.h>
#include <alloc.h>

#include "forge.h"

struct gif_header gif;
struct gif_image gim;
FILE *giffd;
unsigned char defaultpalette[64][3] = {
	0x00, 0x00, 0x00,
	0x00, 0x00, 0x2a,
	0x00, 0x2a, 0x00,
	0x00, 0x2a, 0x2a,
	0x2a, 0x00, 0x00,
	0x2a, 0x00, 0x2a,
	0x2a, 0x2a, 0x00,
	0x2a, 0x2a, 0x2a,
	0x00, 0x00, 0x15,
	0x00, 0x00, 0x3f,
	0x00, 0x2a, 0x15,
	0x00, 0x2a, 0x3f,
	0x2a, 0x00, 0x15,
	0x2a, 0x00, 0x3f,
	0x2a, 0x2a, 0x15,
	0x2a, 0x2a, 0x3f,
	0x00, 0x15, 0x00,
	0x00, 0x15, 0x2a,
	0x00, 0x3f, 0x00,
	0x00, 0x3f, 0x2a,
	0x2a, 0x15, 0x00,
	0x2a, 0x15, 0x2a,
	0x2a, 0x3f, 0x00,
	0x2a, 0x3f, 0x2a,
	0x00, 0x15, 0x15,
	0x00, 0x15, 0x3f,
	0x00, 0x3f, 0x15,
	0x00, 0x3f, 0x3f,
	0x2a, 0x15, 0x15,
	0x2a, 0x15, 0x3f,
	0x2a, 0x3f, 0x15,
	0x2a, 0x3f, 0x3f,
	0x15, 0x00, 0x00,
	0x15, 0x00, 0x2a,
	0x15, 0x2a, 0x00,
	0x15, 0x2a, 0x2a,
	0x3f, 0x00, 0x00,
	0x3f, 0x00, 0x2a,
	0x3f, 0x2a, 0x00,
	0x3f, 0x2a, 0x2a,
	0x15, 0x00, 0x15,
	0x15, 0x00, 0x3f,
	0x15, 0x2a, 0x15,
	0x15, 0x2a, 0x3f,
	0x3f, 0x00, 0x15,
	0x3f, 0x00, 0x3f,
	0x3f, 0x2a, 0x15,
	0x3f, 0x2a, 0x3f,
	0x15, 0x15, 0x00,
	0x15, 0x15, 0x2a,
	0x15, 0x3f, 0x00,
	0x15, 0x3f, 0x2a,
	0x3f, 0x15, 0x00,
	0x3f, 0x15, 0x2a,
	0x3f, 0x3f, 0x00,
	0x3f, 0x3f, 0x2a,
	0x15, 0x15, 0x15,
	0x15, 0x15, 0x3f,
	0x15, 0x3f, 0x15,
	0x15, 0x3f, 0x3f,
	0x3f, 0x15, 0x15,
	0x3f, 0x15, 0x3f,
	0x3f, 0x3f, 0x15,
	0x3f, 0x3f, 0x3f,
};
unsigned char conversion_table[64];

extern int maxx;
extern int minx;
extern int maxy;
unsigned char *ega_lut;
extern unsigned char vgacolortable[];
extern int mode;
extern short gif_decoder(int);
extern int gif_compress_data(int, int);

static int gifcol, gifrow;

static unsigned char *rowbuf;
static int rowbufptr;
static int OurMode;

int gif_get_byte(void)
{
	if (rowbufptr > 511)
	{
		fread(rowbuf, 512, 1, giffd);
		rowbufptr = 0;
	}
	return rowbuf[rowbufptr++];

}

int gif_out_line(unsigned char far * pixels, int len)
{
   extern void rowblast(unsigned char far *,int,int,int);

   if ((OurMode == 0x13 && gifrow >= 200) ||
       (OurMode == 0x12 && gifrow >= 480) ||
       (OurMode == 0x10 && gifrow >= 350))
	 return -1;


   if (OurMode == 0x13)
      len = min(len,320 - gim.x);
   else
      len = min(len,640 - gim.x);

   if (OurMode != 0x13)
   	rowblast(pixels, gim.x, gifrow + gim.y, len);
   else
   {
      char *screen = MK_FP(0xa000,(gifrow+gim.y)*320+gim.x);
      memcpy(screen,pixels,len);
   }

 	gifrow++;
	return 0;
}

static unsigned char cmap[256 * 3];

int GifDisplay(char *filename)
{
	int i, j;
	int c;
	int colors;
	union REGS regs;
	struct SREGS sregs;
	unsigned char ctab[17];
	int ret = GIF_COOLMAN;

	if (!memok(16133L))
	{			/* Added mallocs in decoder.c  */
		ErrorBox("Not enough memory to view a Gif file.");
		return OUT_OF_MEMORY;
	}


	giffd = fopen(filename, "rb");
	if (!giffd)
   {
      FileError(filename,NULL);
		return -1;
   }
	gifrow = 0;
	gifcol = 0;
	safe_alloc = 1;
	if ((rowbuf = malloc(512)) == NULL)
	{
		ret = OUT_OF_MEMORY;
		goto TRUNCOUT;
	}
	rowbufptr = 512;	/* to get it started */


	fread(&gif, 1, sizeof(gif), giffd);
	colors = (1 << ((gif.colpix & PIXMASK) + 1));
   if (colors > 16 && hasVGA)
   {
      regs.h.ah = 0;
      regs.h.al = OurMode = 0x13;
      int86(0x10,&regs,&regs);
   }
   else
   {
      if (gif.h > 350 && hasVGA)
	       OurMode = 0x12;
      else
      	 OurMode = 0x10;

      regs.h.ah = 0;
      regs.h.al = OurMode;
      int86(0x10,&regs,&regs);
   }

	if (gif.colpix & COLTAB)
	{
		fread(cmap, 1, colors * 3, giffd);
		/* Now set up the colormap if appropriate */
		for (i = 0; i < colors * 3; i++)
			cmap[i] >>= 2;
		if (hasVGA)
		{
			/*
			 * This is a VGA. Just write the map, shifted
			 * appropriately.
			 */


			regs.h.ah = 0x10;
			regs.h.al = 0x12;
			regs.x.bx = 0;
			regs.x.cx = colors;
			regs.x.dx = FP_OFF(cmap);
			sregs.es = FP_SEG(cmap);
			int86x(0x10, &regs, &regs, &sregs);

			/* We'll also want to set the 0-16 map here? */
			for (i = 0; i < 16; i++)
				ctab[i] = i;
			ctab[16] = 0;
			regs.h.ah = 0x10;
			regs.h.al = 0x2;
			regs.x.dx = FP_OFF(ctab);
			sregs.es = FP_SEG(ctab);
			int86x(0x10, &regs, &regs, &sregs);

		}
		else
		{

			for (i = 0; i < 16; i++)
			{
				for (j = 0; j < 64; j++)
				{
					unsigned char *p = defaultpalette[j];

					if (p[0] == cmap[i * 3] &&
					    p[1] == cmap[i * 3 + 1] &&
					    p[2] == cmap[i * 3 + 2])
						break;
				}
				ctab[i] = j;
			}
			ctab[i] = 0;
			regs.h.ah = 0x10;
			regs.h.al = 0x2;
			regs.x.dx = FP_OFF(ctab);
			sregs.es = FP_SEG(ctab);
			int86x(0x10, &regs, &regs, &sregs);

		}
	}
	for (;;)		/* skip over extension blocks and other junk
				 * til get ',' */
	{
		if ((c = fgetc(giffd)) == EOF)
      {
         ret = BAD_FILE;
			goto TRUNCOUT;
      }
		if (c == ',')
			break;
		if (c == ';')	/* semi-colon is end of piccie */
      {
         ret = BAD_FILE;
			goto TRUNCOUT;
      }
		if (c == '!')	/* extension block */
		{
			if ((c = fgetc(giffd)) == EOF)	/* skip extension type */
         {
            ret = BAD_FILE;
				goto TRUNCOUT;
         }
			for (;;)
			{
				if ((c = fgetc(giffd)) == EOF)
            {
               ret = BAD_FILE;
					goto TRUNCOUT;
            }
				if (c == 0)	/* zero 'count' means end of
						 * extension */
					break;
				while (--c >= 0)
				{
					if (fgetc(giffd) == EOF)
               {
                  ret = BAD_FILE;
						goto TRUNCOUT;
               }
				}
			}
		}
	}
	fread(&gim, 1, sizeof(gim), giffd);
	if (gim.flags & COLTAB)
	{
		colors = (1 << ((gim.flags & PIXMASK) + 1));
		fread(&cmap, 1, colors * 3, giffd);
	}
	ret = gif_decoder(gim.w);
TRUNCOUT:
	fclose(giffd);
	if (rowbuf)
      free(rowbuf);
	return ret;
}




int GifOutput(char *filename, int wholescreen)
{
	int i, j;
   extern void clearGIFlines(int);

	memset(&gif, 0, sizeof gif);

	giffd = fopen(filename, "wb");
	if (!giffd)
	{
		/* Alert the user of the error */
		return 0;
	}
	strcpy(gif.giftype, "GIF87a");

   if (RenderingMode == 0)
   {
	   gif.w = gim.w = (wholescreen) ? 640 : 560;
	   gif.h = gim.h = (mode == 0x10) ? 350 : 480;
	   gim.y = 0;
	   if (wholescreen)
		   gim.x = 0;
	   else
		   gim.x = 80;
	   gim.flags = 0;
	   gif.colpix = 128 + (7 << 4) + 3;
   }
   else
   {
      gif.w = gim.w = 320;
      gif.h = gim.h = 200;
      gim.y = gim.x = 0;
      gim.flags = 0;
      gif.colpix = COLPIXVGA13;
   }

	fwrite(&gif, sizeof gif, 1, giffd);

	/*
	 * Build the conversion table... this will go from 0..63 to 0..255,
	 * since << 2 is just WRONG.
	 */
	for (i = 0; i < 64; i++)
		conversion_table[i] = (unsigned char) (((float) i) / 63.0 * 255.0);

   if (RenderingMode == 0)
   {



	   for (i = 0; i < 16; i++)
      {
		   for (j = 0; j < 3; j++)
		   {
			   putc(conversion_table[defaultpalette[brightpal[i]][j]], giffd);
		   }
      }
   }
   else
   {
	   for (i = 0; i < 256; i++)
      {
		   for (j = 0; j < 3; j++)
		   {
			   putc(conversion_table[clut[i][j]], giffd);
		   }
      }
   }


	putc(',', giffd);
	fwrite(&gim, sizeof gim, 1, giffd);
	putc(8, giffd);

	/* Now we crunch the data. */
	i = gif_compress_data(8, wholescreen);
   clearGIFlines(wholescreen);

	putc(';', giffd);
   if (i == -3 || ferror(giffd))
   {
      if (RenderingMode == 0)
         FileError(filename,giffd);
      fclose(giffd);
      remove(filename);
      return 0;

   }
   else
   	fclose(giffd);
	return 1;

}
static int lastrow = -1;
struct
{
	imageHeader ih;
	char imData[320];
} rowdata;

static int shifts[] = {7, 6, 5, 4, 3, 2, 1, 0};
unsigned char JGetPixel(int x, int y)
{

   if (RenderingMode == 0)
   {
	   int offset = x >> 3;
	   unsigned shift = shifts[x & 7];
	   char *p = rowdata.imData;

	   if (y != lastrow)
	   {
		   rect R;

		   R.Xmin = 0;
		   R.Xmax = 639;
		   R.Ymin = R.Ymax = y;
		   ReadImage(&R, (image *) & rowdata);
		   lastrow = y;
	   }

	   return (
		   ((p[offset] >> shift) & 1) |
		   (((p[offset + 81] >> shift) & 1) << 1) |
		   (((p[offset + 162] >> shift) & 1) << 2) |
		   (((p[offset + 243] >> shift) & 1) << 3));
   }
   else
   {
      char *addr = MK_FP(0xa000,y*320+x);
      return *addr;
   }

}


void JSetPixel(int x, int y, char c)
{
	union REGS regs;

   if (RenderingMode == 0)
   {

	   if (c & 0x80)
	   {
		   regs.h.ah = 0x0c;
		   regs.h.al = c;
		   regs.h.bh = 0;
		   regs.x.cx = x;
		   regs.x.dx = y;
		   int86(0x10, &regs, &regs);
	   }
	   else
		   SetPixel10(x, y, c);
   }
   else
   {
      char *addr = MK_FP(0xa000,y*320+x);
      *addr ^= c;
   }

}
