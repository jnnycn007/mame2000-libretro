/*************************************************************************

	Driver for Williams/Midway T-unit games.

**************************************************************************/

#include "driver.h"
#include "cpu/tms34010/tms34010.h"



/* compile-time options */
#define FAST_DMA			1		/* DMAs complete immediately; reduces number of CPU switches */
#define LOG_DMA				0		/* DMAs are logged if the 'L' key is pressed */


/* constants for the  DMA chip */
enum
{
	DMA_LRSKIP = 0,
	DMA_COMMAND,
	DMA_OFFSETLO,
	DMA_OFFSETHI,
	DMA_XSTART,
	DMA_YSTART,
	DMA_WIDTH,
	DMA_HEIGHT,
	DMA_PALETTE,
	DMA_COLOR,
	DMA_SCALE_X,
	DMA_SCALE_Y,
	DMA_TOPCLIP,
	DMA_BOTCLIP,
	DMA_UNKNOWN_E,	/* MK1/2 never write here; NBA only writes 0 */
	DMA_CONTROL,
	DMA_LEFTCLIP,	/* pseudo-register */
	DMA_RIGHTCLIP	/* pseudo-register */
};



/* graphics-related variables */
extern uint8_t *	wms_gfx_rom;
extern size_t	wms_gfx_rom_size;
       uint8_t	wms_gfx_rom_large;
static uint16_t	wms_control;

/* videoram-related variables */
static uint32_t 	gfxbank_offset[2];
static uint16_t *	local_videoram;
static uint8_t	videobank_select;

/* DMA-related variables */
static uint16_t	dma_register[18];
static struct
{
	uint32_t		offset;			/* source offset, in bits */
	int32_t 		rowbits;		/* source bits to skip each row */
	int32_t 		xpos;			/* x position, clipped */
	int32_t		ypos;			/* y position, clipped */
	int32_t		width;			/* horizontal pixel count */
	int32_t		height;			/* vertical pixel count */
	uint16_t		palette;		/* palette base */
	uint16_t		color;			/* current foreground color with palette */

	uint8_t		yflip;			/* yflip? */
	uint8_t		bpp;			/* bits per pixel */
	uint8_t		preskip;		/* preskip scale */
	uint8_t		postskip;		/* postskip scale */
	int32_t		topclip;		/* top clipping scanline */
	int32_t		botclip;		/* bottom clipping scanline */
	int32_t		leftclip;		/* left clipping column */
	int32_t		rightclip;		/* right clipping column */
	int32_t		startskip;		/* pixels to skip at start */
	int32_t		endskip;		/* pixels to skip at end */
	uint16_t		xstep;			/* 8.8 fixed number scale x factor */
	uint16_t		ystep;			/* 8.8 fixed number scale y factor */
} dma_state;



/* prototypes */
void wms_tunit_vh_stop(void);



/*************************************
 *
 *	Video startup
 *
 *************************************/

int wms_tunit_vh_start(void)
{
	/* allocate memory */
	local_videoram = (uint16_t*)malloc(0x100000);
	
	/* handle failure */
	if (!local_videoram)
	{
		wms_tunit_vh_stop();
		return 1;
	}
	
	/* reset all the globals */
	gfxbank_offset[0] = 0x000000;
	gfxbank_offset[1] = 0x400000;
	
	memset(dma_register, 0, sizeof(dma_register));
	memset(&dma_state, 0, sizeof(dma_state));

	return 0;
}


int wms_wolfu_vh_start(void)
{
	int result = wms_tunit_vh_start();
	wms_gfx_rom_large = 1;
	return result;
}



/*************************************
 *
 *	Video shutdown
 *
 *************************************/

void wms_tunit_vh_stop(void)
{
	if (local_videoram)
		free(local_videoram);
	local_videoram = NULL;
}



/*************************************
 *
 *	Banked graphics ROM access
 *
 *************************************/

READ_HANDLER( wms_tunit_gfxrom_r )
{
	uint8_t *base = &wms_gfx_rom[gfxbank_offset[(offset >> 22) & 1]];
	offset &= 0x03fffff;
	return base[offset] | (base[offset + 1] << 8);
}


READ_HANDLER( wms_wolfu_gfxrom_r )
{
	uint8_t *base = &wms_gfx_rom[gfxbank_offset[0]];
	return base[offset] | (base[offset + 1] << 8);
}



/*************************************
 *
 *	Video RAM read/write
 *
 *************************************/

WRITE_HANDLER( wms_tunit_vram_w )
{
	if (videobank_select)
	{
		if (!(data & 0x00ff0000))
			local_videoram[offset] = (data & 0xff) | ((dma_register[DMA_PALETTE] & 0xff) << 8);
		if (!(data & 0xff000000))
			local_videoram[offset + 1] = ((data >> 8) & 0xff) | (dma_register[DMA_PALETTE] & 0xff00);
	}
	else
	{
		if (!(data & 0x00ff0000))
			local_videoram[offset] = (local_videoram[offset] & 0xff) | ((data & 0xff) << 8);
		if (!(data & 0xff000000))
			local_videoram[offset + 1] = (local_videoram[offset + 1] & 0xff) | (data & 0xff00);
	}
}


READ_HANDLER( wms_tunit_vram_r )
{
	if (videobank_select)
		return (local_videoram[offset] & 0x00ff) | (local_videoram[offset + 1] << 8);
	else
		return (local_videoram[offset] >> 8) | (local_videoram[offset + 1] & 0xff00);
}



/*************************************
 *
 *	Shift register read/write
 *
 *************************************/

void wms_tunit_to_shiftreg(uint32_t address, uint16_t *shiftreg)
{
	memcpy(shiftreg, &local_videoram[address >> 3], 2 * 512 * sizeof(uint16_t));
}


void wms_tunit_from_shiftreg(uint32_t address, uint16_t *shiftreg)
{
	memcpy(&local_videoram[address >> 3], shiftreg, 2 * 512 * sizeof(uint16_t));
}



/*************************************
 *
 *	Control register
 *
 *************************************/

WRITE_HANDLER( wms_tunit_control_w )
{
	/* 
		other important bits:
			bit 2 (0x0004) is toggled periodically
	*/
	//logerror("T-unit control = %04X\n", data);
	
	COMBINE_WORD_MEM(&wms_control, data);

	/* gfx bank select is bit 7 */
	if (!(wms_control & 0x0080) || !wms_gfx_rom_large)
		gfxbank_offset[0] = 0x000000;
	else
		gfxbank_offset[0] = 0x800000;

	/* video bank select is bit 5 */
	videobank_select = (wms_control >> 5) & 1;
}


WRITE_HANDLER( wms_wolfu_control_w )
{
	/* 
		other important bits:
			bit 2 (0x0004) is toggled periodically
	*/
	//logerror("Wolf-unit control = %04X\n", data);
	
	COMBINE_WORD_MEM(&wms_control, data);
	
	/* gfx bank select is bits 8-9 */
	gfxbank_offset[0] = 0x800000 * ((wms_control >> 8) & 3);
	
	/* video bank select is unknown */
	videobank_select = (wms_control >> 11) & 1;
}


READ_HANDLER( wms_wolfu_control_r )
{
	return wms_control;
}



/*************************************
 *
 *	Palette handlers
 *
 *************************************/

WRITE_HANDLER( wms_tunit_paletteram_w )
{
	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword, data);
	
	int r = (newword >> 10) & 0x1f;
	int g = (newword >>  5) & 0x1f;
	int b = (newword      ) & 0x1f;
	
	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);
	
	WRITE_WORD(&paletteram[offset], newword);
	palette_change_color(offset / 2, r, g, b);
}



/*************************************
 *
 *	DMA drawing routines
 *
 *************************************/

/*** constant definitions ***/
#define	PIXEL_SKIP		0
#define PIXEL_COLOR		1
#define PIXEL_COPY		2

#define XFLIP_NO		0
#define XFLIP_YES		1

#define SKIP_NO			0
#define SKIP_YES		1

#define SCALE_NO		0
#define SCALE_YES		1


typedef void (*dma_draw_func)(void);


/*** fast pixel extractors ***/
#if !defined(ALIGN_SHORTS) && !defined(MSB_FIRST)
#define EXTRACTGEN(m)	((*(uint16_t *)&base[o >> 3] >> (o & 7)) & (m))
#elif defined(powerc)
#define EXTRACTGEN(m)	((__lhbrx(base, o >> 3) >> (o & 7)) & (m))
#else
#define EXTRACTGEN(m)	(((base[o >> 3] | (base[(o >> 3) + 1] << 8)) >> (o & 7)) & (m))
#endif

/*** core blitter routine macro ***/
#define DMA_DRAW_FUNC_BODY(name, bitsperpixel, extractor, xflip, skip, scale, zero, nonzero) \
{																				\
	int height = dma_state.height << 8;											\
	uint8_t *base = wms_gfx_rom;													\
	uint32_t offset = dma_state.offset;											\
	uint16_t pal = dma_state.palette;												\
	uint16_t color = pal | dma_state.color;										\
	int sy = dma_state.ypos, iy = 0, ty;										\
	int bpp = bitsperpixel;														\
	int mask = (1 << bpp) - 1;													\
	int xstep = scale ? dma_state.xstep : 0x100;								\
																				\
	/* loop over the height */													\
	while (iy < height)															\
	{																			\
		int startskip = dma_state.startskip << 8;								\
		int endskip = dma_state.endskip << 8;									\
		int width = dma_state.width << 8;										\
		int sx = dma_state.xpos, ix = 0, tx;									\
		uint32_t o = offset;														\
		int pre, post;															\
		uint16_t *d;																\
																				\
		/* handle skipping */													\
		if (skip)																\
		{																		\
			uint8_t value = EXTRACTGEN(0xff);										\
			o += 8;																\
																				\
			/* adjust for preskip */											\
			pre = (value & 0x0f) << (dma_state.preskip + 8);					\
			tx = pre / xstep;													\
			xflip ? (sx -= tx) : (sx += tx);									\
			ix += tx * xstep;													\
																				\
			/* adjust for postskip */											\
			post = ((value >> 4) & 0x0f) << (dma_state.postskip + 8);			\
			width -= post;														\
			endskip -= post;													\
		}																		\
																				\
		/* handle Y clipping */													\
		if (sy < dma_state.topclip || sy > dma_state.botclip)					\
			goto clipy;															\
																				\
		/* handle left clip */													\
		if (!xflip && sx < 0)													\
		{																		\
			tx = -sx * xstep;													\
			ix += tx;															\
			o += (tx >> 8) * bpp;												\
			sx = 0;																\
		}																		\
		else if (xflip && sx >= 512)											\
		{																		\
			tx = (sx - 511) * xstep;											\
			ix += tx;															\
			o += (tx >> 8) * bpp;												\
			sx = 511;															\
		}																		\
																				\
		/* handle start skip */													\
		if (ix < startskip)														\
		{																		\
			tx = ((startskip - ix) / xstep) * xstep;							\
			ix += tx;															\
			o += (tx >> 8) * bpp;												\
		}																		\
																				\
		/* handle end skip */													\
		if ((width >> 8) > dma_state.width - dma_state.endskip)					\
			width = (dma_state.width - dma_state.endskip) << 8;					\
																				\
		/* determine destination pointer */										\
		d = &local_videoram[sy * 512 + sx];										\
																				\
		/* loop until we draw the entire width */								\
		while (ix < width && ((!xflip && sx < 512) || (xflip && sx >= 0)))		\
		{																		\
			/* special case similar handling of zero/non-zero */				\
			if (zero == nonzero)												\
			{																	\
				if (zero == PIXEL_COLOR)										\
					*d = color;													\
				else if (zero == PIXEL_COPY)									\
					*d = (extractor(mask)) | pal;								\
			}																	\
																				\
			/* otherwise, read the pixel and look */							\
			else																\
			{																	\
				int pixel = (extractor(mask));									\
																				\
				/* non-zero pixel case */										\
				if (pixel)														\
				{																\
					if (nonzero == PIXEL_COLOR)									\
						*d = color;												\
					else if (nonzero == PIXEL_COPY)								\
						*d = pixel | pal;										\
				}																\
																				\
				/* zero pixel case */											\
				else															\
				{																\
					if (zero == PIXEL_COLOR)									\
						*d = color;												\
					else if (zero == PIXEL_COPY)								\
						*d = pal;												\
				}																\
			}																	\
																				\
			/* update pointers */												\
			if (xflip) 															\
				d--, sx--; 														\
			else 																\
				d++, sx++;														\
																				\
			/* advance to the next pixel */										\
			if (!scale)															\
			{																	\
				ix += 0x100;													\
				o += bpp;														\
			}																	\
			else																\
			{																	\
				tx = ix >> 8;													\
				ix += xstep;													\
				tx = (ix >> 8) - tx;											\
				o += bpp * tx;													\
			}																	\
		}																		\
																				\
	clipy:																		\
		/* advance to the next row */											\
		dma_state.yflip ? sy-- : sy++;											\
		if (!scale)																\
		{																		\
			iy += 0x100;														\
			width = dma_state.width;											\
			if (skip)															\
			{																	\
				offset += 8;													\
				width -= (pre + post) >> 8;										\
				if (width > 0) offset += width * bpp;							\
			}																	\
			else																\
				offset += width * bpp;											\
		}																		\
		else																	\
		{																		\
			ty = iy >> 8;														\
			iy += dma_state.ystep;												\
			ty = (iy >> 8) - ty;												\
			if (!skip)															\
				offset += ty * dma_state.width * bpp;							\
			else if (ty--)														\
			{																	\
				o = offset + 8;													\
				width = dma_state.width - ((pre + post) >> 8);					\
				if (width > 0) o += width * bpp;								\
				while (ty--)													\
				{																\
					uint8_t value = EXTRACTGEN(0xff);								\
					o += 8;														\
					pre = (value & 0x0f) << dma_state.preskip;					\
					post = ((value >> 4) & 0x0f) << dma_state.postskip;			\
					width = dma_state.width - pre - post;						\
					if (width > 0) o += width * bpp;							\
				}																\
				offset = o;														\
			}																	\
		}																		\
	}																			\
}


/*** slightly simplified one for most blitters ***/
#define DMA_DRAW_FUNC(name, bpp, extract, xflip, skip, scale, zero, nonzero)	\
static void name(void)															\
{																				\
	DMA_DRAW_FUNC_BODY(name, bpp, extract, xflip, skip, scale, zero, nonzero)	\
}

/*** empty blitter ***/
static void dma_draw_none(void)
{
}

/*** super macro for declaring an entire blitter family ***/
#define DECLARE_BLITTER_SET(prefix, bpp, extract, skip, scale)										\
DMA_DRAW_FUNC(prefix##_p0,      bpp, extract, XFLIP_NO,  skip, scale, PIXEL_COPY,  PIXEL_SKIP)		\
DMA_DRAW_FUNC(prefix##_p1,      bpp, extract, XFLIP_NO,  skip, scale, PIXEL_SKIP,  PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_c0,      bpp, extract, XFLIP_NO,  skip, scale, PIXEL_COLOR, PIXEL_SKIP)		\
DMA_DRAW_FUNC(prefix##_c1,      bpp, extract, XFLIP_NO,  skip, scale, PIXEL_SKIP,  PIXEL_COLOR)		\
DMA_DRAW_FUNC(prefix##_p0p1,    bpp, extract, XFLIP_NO,  skip, scale, PIXEL_COPY,  PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_c0c1,    bpp, extract, XFLIP_NO,  skip, scale, PIXEL_COLOR, PIXEL_COLOR)		\
DMA_DRAW_FUNC(prefix##_c0p1,    bpp, extract, XFLIP_NO,  skip, scale, PIXEL_COLOR, PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_p0c1,    bpp, extract, XFLIP_NO,  skip, scale, PIXEL_COPY,  PIXEL_COLOR)		\
																									\
DMA_DRAW_FUNC(prefix##_p0_xf,   bpp, extract, XFLIP_YES, skip, scale, PIXEL_COPY,  PIXEL_SKIP)		\
DMA_DRAW_FUNC(prefix##_p1_xf,   bpp, extract, XFLIP_YES, skip, scale, PIXEL_SKIP,  PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_c0_xf,   bpp, extract, XFLIP_YES, skip, scale, PIXEL_COLOR, PIXEL_SKIP)		\
DMA_DRAW_FUNC(prefix##_c1_xf,   bpp, extract, XFLIP_YES, skip, scale, PIXEL_SKIP,  PIXEL_COLOR)		\
DMA_DRAW_FUNC(prefix##_p0p1_xf, bpp, extract, XFLIP_YES, skip, scale, PIXEL_COPY,  PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_c0c1_xf, bpp, extract, XFLIP_YES, skip, scale, PIXEL_COLOR, PIXEL_COLOR)		\
DMA_DRAW_FUNC(prefix##_c0p1_xf, bpp, extract, XFLIP_YES, skip, scale, PIXEL_COLOR, PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_p0c1_xf, bpp, extract, XFLIP_YES, skip, scale, PIXEL_COPY,  PIXEL_COLOR)		\
																											\
static dma_draw_func prefix[32] =																			\
{																											\
/*	B0:N / B1:N			B0:Y / B1:N			B0:N / B1:Y			B0:Y / B1:Y */								\
	dma_draw_none,		prefix##_p0,		prefix##_p1,		prefix##_p0p1,		/* no color */ 			\
	prefix##_c0,		prefix##_c0,		prefix##_c0p1,		prefix##_c0p1,		/* color 0 pixels */ 	\
	prefix##_c1,		prefix##_p0c1,		prefix##_c1,		prefix##_p0c1,		/* color non-0 pixels */\
	prefix##_c0c1,		prefix##_c0c1,		prefix##_c0c1,		prefix##_c0c1,		/* fill */ 				\
																											\
	dma_draw_none,		prefix##_p0_xf,		prefix##_p1_xf,		prefix##_p0p1_xf,	/* no color */ 			\
	prefix##_c0_xf,		prefix##_c0_xf,		prefix##_c0p1_xf,	prefix##_c0p1_xf,	/* color 0 pixels */ 	\
	prefix##_c1_xf,		prefix##_p0c1_xf,	prefix##_c1_xf,		prefix##_p0c1_xf,	/* color non-0 pixels */\
	prefix##_c0c1_xf,	prefix##_c0c1_xf,	prefix##_c0c1_xf,	prefix##_c0c1_xf	/* fill */ 				\
};


/* allow for custom blitters */
#ifdef WMS_TUNIT_CUSTOM_BLITTERS
#include "wmstblit.c"
#endif


/*** blitter family declarations ***/
DECLARE_BLITTER_SET(dma_draw_skip_scale,       dma_state.bpp, EXTRACTGEN,   SKIP_YES, SCALE_YES)
DECLARE_BLITTER_SET(dma_draw_noskip_scale,     dma_state.bpp, EXTRACTGEN,   SKIP_NO,  SCALE_YES)
DECLARE_BLITTER_SET(dma_draw_skip_noscale,     dma_state.bpp, EXTRACTGEN,   SKIP_YES, SCALE_NO)
DECLARE_BLITTER_SET(dma_draw_noskip_noscale,   dma_state.bpp, EXTRACTGEN,   SKIP_NO,  SCALE_NO)



/*************************************
 *
 *	DMA finished callback
 *
 *************************************/

static int temp_irq_callback(int irqline)
{
	tms34010_set_irq_line(0, CLEAR_LINE);
	return 0;
}


static void dma_callback(int is_in_34010_context)
{
	dma_register[DMA_COMMAND] &= ~0x8000; /* tell the cpu we're done */
	if (is_in_34010_context)
	{
		tms34010_set_irq_callback(temp_irq_callback);
		tms34010_set_irq_line(0, ASSERT_LINE);
	}
	else
		cpu_cause_interrupt(0,TMS34010_INT1);
}



/*************************************
 *
 *	DMA reader
 *
 *************************************/

READ_HANDLER( wms_tunit_dma_r )
{
	return READ_WORD(&dma_register[offset / 2]);
}



/*************************************
 *
 *	DMA write handler
 *
 *************************************/

/*
 * DMA registers
 * ------------------
 *
 *  Register | Bit              | Use
 * ----------+-FEDCBA9876543210-+------------
 *     0     | xxxxxxxx-------- | pixels to drop at the start of each row
 *           | --------xxxxxxxx | pixels to drop at the end of each row
 *     1     | x--------------- | trigger write (or clear if zero)
 *           | -421------------ | image bpp (0=8)
 *           | ----84---------- | post skip size = (1<<x)
 *           | ------21-------- | pre skip size = (1<<x)
 *           | --------8------- | pre/post skip enable
 *           | ---------4------ | pixel skip control
 *           | ----------2----- | flip y
 *           | -----------1---- | flip x
 *           | ------------8--- | blit nonzero pixels as color
 *           | -------------4-- | blit zero pixels as color
 *           | --------------2- | blit nonzero pixels
 *           | ---------------1 | blit zero pixels
 *     2     | xxxxxxxxxxxxxxxx | source address low word
 *     3     | xxxxxxxxxxxxxxxx | source address high word
 *     4     | xxxxxxxxxxxxxxxx | detination x
 *     5     | xxxxxxxxxxxxxxxx | destination y
 *     6     | xxxxxxxxxxxxxxxx | image columns
 *     7     | xxxxxxxxxxxxxxxx | image rows
 *     8     | xxxxxxxxxxxxxxxx | palette
 *     9     | xxxxxxxxxxxxxxxx | color
 *    10     | xxxxxxxxxxxxxxxx | scale x
 *    11     | xxxxxxxxxxxxxxxx | scale y
 *    12     | xxxxxxxxxxxxxxxx | top clip
 *    13     | xxxxxxxxxxxxxxxx | bottom clip
 */

WRITE_HANDLER( wms_tunit_dma_w )
{
	static const uint8_t register_map[2][16] =
	{
		{ 0,1,2,3,4,5,6,7,8,9,10,11,16,17,14,15 },
		{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 }
	};
	int regbank = (dma_register[DMA_CONTROL] >> 5) & 1;
	int command, bpp, regnum;
	uint32_t gfxoffset;

	/* blend with the current register contents */
	regnum = register_map[regbank][offset / 2];
	COMBINE_WORD_MEM(&dma_register[regnum], data);

#if LOG_DMA
	if (keyboard_pressed(KEYCODE_L))
		logerror("%08X:DMA %d = %04X\n", cpu_get_pc(), regnum, data);
#endif

	/* only writes to DMA_COMMAND actually cause actions */
	if (regnum != DMA_COMMAND)
		return;

	/* high bit triggers action */
	command = dma_register[DMA_COMMAND];
	if (!(command & 0x8000))
	{
		tms34010_set_irq_line(0, CLEAR_LINE);
		return;
	}

#if LOG_DMA
	if (keyboard_pressed(KEYCODE_L))
	{
		logerror("DMA command %04X: (bpp=%d skip=%d xflip=%d yflip=%d preskip=%d postskip=%d)\n",
				command, (command >> 12) & 7, (command >> 7) & 1, (command >> 4) & 1, (command >> 5) & 1, (command >> 8) & 3, (command >> 10) & 3);
		logerror("  offset=%08X pos=(%d,%d) w=%d h=%d clip=(%d,%d)\n", 
				dma_register[DMA_OFFSETLO] | (dma_register[DMA_OFFSETHI] << 16),
				(int16_t)dma_register[DMA_XSTART], (int16_t)dma_register[DMA_YSTART],
				dma_register[DMA_WIDTH], dma_register[DMA_HEIGHT],
				dma_register[DMA_TOPCLIP], dma_register[DMA_BOTCLIP]);
		logerror("  palette=%04X color=%04X lskip=%02X rskip=%02X xstep=%04X ystep=%04X unkE=%04X unkF=%04X\n",
				dma_register[DMA_PALETTE], dma_register[DMA_COLOR],
				dma_register[DMA_LRSKIP] >> 8, dma_register[DMA_LRSKIP] & 0xff,
				dma_register[DMA_SCALE_X], dma_register[DMA_SCALE_Y], dma_register[DMA_UNKNOWN_E],
				dma_register[DMA_CONTROL]);
		logerror("----\n");
	}
#endif
	
	profiler_mark(PROFILER_USER1);

	/* determine bpp */
	bpp = (command >> 12) & 7;

	/* fill in the basic data */
	dma_state.xpos = (int16_t)dma_register[DMA_XSTART];
	dma_state.ypos = (int16_t)dma_register[DMA_YSTART];
	dma_state.width = dma_register[DMA_WIDTH];
	dma_state.height = dma_register[DMA_HEIGHT];
	dma_state.palette = dma_register[DMA_PALETTE] & 0x7f00;
	dma_state.color = dma_register[DMA_COLOR] & 0xff;

	/* fill in the rev 2 data */
	dma_state.yflip = (command & 0x20) >> 5;
	dma_state.bpp = bpp ? bpp : 8;
	dma_state.preskip = (command >> 8) & 3;
	dma_state.postskip = (command >> 10) & 3;
	dma_state.topclip = (int16_t)dma_register[DMA_TOPCLIP];
	dma_state.botclip = (int16_t)dma_register[DMA_BOTCLIP];
	dma_state.leftclip = (int16_t)dma_register[DMA_LEFTCLIP];
	dma_state.rightclip = (int16_t)dma_register[DMA_RIGHTCLIP];
	dma_state.xstep = dma_register[DMA_SCALE_X] ? dma_register[DMA_SCALE_X] : 0x100;
	dma_state.ystep = dma_register[DMA_SCALE_Y] ? dma_register[DMA_SCALE_Y] : 0x100;

	/* clip the clippers */	
	if (dma_state.topclip < 0) dma_state.topclip = 0;
	if (dma_state.botclip > 512) dma_state.botclip = 512;
	if (dma_state.leftclip < 0) dma_state.leftclip = 0;
	if (dma_state.rightclip > 512) dma_state.rightclip = 512;
	
	/* determine the offset */
	gfxoffset = dma_register[DMA_OFFSETLO] | (dma_register[DMA_OFFSETHI] << 16);
	
	/* special case: drawing mode C doesn't need to know about any pixel data */
	if ((command & 0x0f) == 0x0c)
		gfxoffset = 0;
	
	/* determine the location */
	if (!wms_gfx_rom_large && gfxoffset >= 0x2000000)
		gfxoffset -= 0x2000000;
	if (gfxoffset < 0x10000000)
		dma_state.offset = gfxoffset;
	else
	{
		//logerror("DMA source out of range: %08X\n", gfxoffset);
		goto skipdma;
	}

	/* there seems to be two types of behavior for the DMA chip */
	/* for MK1 and MK2, the upper byte of the LRSKIP is the     */
	/* starting skip value, and the lower byte is the ending    */
	/* skip value; for the NBA Jam, Hangtime, and Open Ice, the */
	/* full word seems to be the starting skip value.           */
	if (command & 0x40)
	{
		dma_state.startskip = dma_register[DMA_LRSKIP] & 0xff;
		dma_state.endskip = dma_register[DMA_LRSKIP] >> 8;
	}
	else
	{
		dma_state.startskip = 0;
		dma_state.endskip = dma_register[DMA_LRSKIP];
	}
	
	/* then draw */
	if (dma_state.xstep == 0x100 && dma_state.ystep == 0x100)
	{
		if (command & 0x80) 
			(*dma_draw_skip_noscale[command & 0x1f])();
		else 
			(*dma_draw_noskip_noscale[command & 0x1f])();
	}
	else
	{
		if (command & 0x80) 
			(*dma_draw_skip_scale[command & 0x1f])();
		else 
			(*dma_draw_noskip_scale[command & 0x1f])();
	}

	/* signal we're done */
skipdma:

	/* special case for Open Ice: use a timer for command 0x8000, which is */
	/* used to initiate the DMA. What they do is start the DMA, *then* set */
	/* up the memory for it, which means that there must be some non-zero  */
	/* delay that gives them enough time to build up the DMA command list  */
	if (FAST_DMA)
	{
		if (command != 0x8000)
			dma_callback(1);
		else
		{
			tms34010_set_irq_line(0, CLEAR_LINE);
			timer_set(TIME_IN_NSEC(41 * dma_state.width * dma_state.height * 4), 0, dma_callback);
		}
	}
	else
	{
		tms34010_set_irq_line(0, CLEAR_LINE);
		timer_set(TIME_IN_NSEC(41 * dma_state.width * dma_state.height), 0, dma_callback);
	}

	profiler_mark(PROFILER_END);
}



/*************************************
 *
 *	Screen updater
 *
 *************************************/

static void update_screen(struct osd_bitmap *bitmap)
{
	uint16_t *pens = Machine->pens;
	int v, h, width, xoffs;
	uint32_t offset;

	/* determine the base of the videoram */
	offset = ((~tms34010_get_DPYSTRT(0) & 0x1ff0) << 5) & 0x3ffff;

	/* determine how many pixels to copy */
	xoffs = Machine->visible_area.min_x;
	width = Machine->visible_area.max_x - xoffs + 1;
	
	/* adjust the offset */
	offset += xoffs;
	offset += 512 * Machine->visible_area.min_y;
	offset &= 0x3ffff;

	/* 16-bit refresh case */
	if (bitmap->depth == 16)
	{
		/* loop over rows */
		for (v = Machine->visible_area.min_y; v <= Machine->visible_area.max_y; v++)
		{
			/* handle the refresh */
			uint16_t *src = &local_videoram[offset];
			uint16_t *dst = &((uint16_t *)bitmap->line[v])[xoffs];

			/* copy one row */
			for (h = 0; h < width; h++)
				*dst++ = pens[*src++];

			/* point to the next row */
			offset = (offset + 512) & 0x3ffff;
		}
	}

	/* 8-bit refresh case */
	else
	{
		/* loop over rows */
		for (v = Machine->visible_area.min_y; v <= Machine->visible_area.max_y; v++)
		{
			/* handle the refresh */
			uint16_t *src = &local_videoram[offset];
			uint8_t *dst = &bitmap->line[v][xoffs];

			for (h = 0; h < width; h++)
				*dst++ = pens[*src++];

			/* point to the next row */
			offset = (offset + 512) & 0x3ffff;
		}
	}
}



/*************************************
 *
 *	34010 display address callback
 *
 *************************************/

void wms_tunit_display_addr_changed(uint32_t offs, int rowbytes, int scanline)
{
	//logerror("Display address = %08X\n", offs);
}



/*************************************
 *
 *	Core refresh routine
 *
 *************************************/

void wms_tunit_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	// recompute the palette
	palette_recalc();

	// update the entire screen
	update_screen(bitmap);
}
