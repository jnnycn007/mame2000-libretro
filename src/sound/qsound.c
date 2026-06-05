/***************************************************************************

  Capcom System QSound(tm)
  ========================

  Driver by Paul Leaman (paul@vortexcomputing.demon.co.uk)
		and Miguel Angel Horna (mahorna@teleline.es)

  A 16 channel stereo sample player.

  QSpace position is simulated by panning the sound in the stereo space.

  Register
  0	 xxbb	xx = unknown bb = start high address
  1	 ssss	ssss = sample start address
  2	 pitch
  3	 unknown (always 0x8000)
  4	 loop offset from end address
  5	 end
  6	 master channel volume
  7	 not used
  8	 Balance (right=0x0110  centre=0x0120 left=0x0130)
  9	 unknown (most fixed samples use 0 for this register)

  Many thanks to CAB (the author of Amuse), without whom this probably would
  never have been finished.

  If anybody has some information about this hardware, please send it to me
  to mahorna@teleline.es or 432937@cepsz.unizar.es.
  http://teleline.terra.es/personal/mahorna

***************************************************************************/

#include <math.h>
#include "driver.h"

/* Typedefs & defines */

#define QSOUND_CLOCKDIV 166			 /* Clock divider */
#define QSOUND_CHANNELS 16

struct qsound_channel
{
	int bank;	   /* bank (x16)	*/
	int address;	/* start address */
	int pitch;	  /* pitch */
	int reg3;	   /* unknown (always 0x8000) */
	int loop;	   /* loop address */
	int end;		/* end address */
	int vol;		/* master volume */
	int pan;		/* Pan value */
	int echo;	   /* echo send level (register 0xba+ch); 0 = no echo */

	/* Work variables */
	int key;		/* Key on / key off */

	int lvol;	   /* left volume */
	int rvol;	   /* right volume */
	int lastdt;	 /* last sample value */
	int offset;	 /* current offset counter */
};


/* Private variables */
static struct QSound_interface *intf;	/* Interface  */
static int qsound_stream;				/* Audio stream */
static struct qsound_channel qsound_channel[QSOUND_CHANNELS];
static int qsound_data;				  /* register latch data */
int8_t *qsound_sample_rom;	/* Q sound sample ROM */

static int qsound_pan_table[33];		 /* Pan volume table */
static float qsound_frq_ratio;		   /* Frequency ratio */

/* QSound echo effect state.  The DL-1425 DSP applies a feedback echo
 * to the summed voice contributions (each voice's send level is its
 * register 0xba+ch).  The delay-line ring buffer holds the last N
 * accumulated echo samples; per output sample the DSP averages the
 * two-most-recent ring entries, scales them by the feedback gain, adds
 * the current accumulated voice contribution, writes back to the
 * ring, and mixes the averaged value into the output.  Without the
 * FIR filter (the wet path is filtered, the dry path is not) we mix
 * the averaged echo into both L and R equally; this loses the
 * subtle L-dry/R-wet asymmetry of the real DSP but keeps the
 * stereo image balanced.
 *
 * Register layout:
 *   0x93 -> qsound_echo_feedback   (feedback gain, 16-bit)
 *   0xd9 -> qsound_echo_end_pos    (delay-line end, length = end - 0x554)
 *   0xba+ch -> qsound_channel[ch].echo  (per-voice send level) */
#define QSOUND_DELAY_BASE_OFFSET 0x554
#define QSOUND_DELAY_BUFFER_LEN  1024
static int32_t qsound_echo_buffer[QSOUND_DELAY_BUFFER_LEN];
static int qsound_echo_pos;        /* current ring read/write position */
static int qsound_echo_length;     /* current ring length (samples) */
static int qsound_echo_end_pos;    /* raw value written to reg 0xd9 */
static int qsound_echo_feedback;   /* feedback gain (16-bit signed) */
static int qsound_echo_last;       /* previous ring entry (for 2-tap avg) */

/* Apply the echo to a single accumulated voice-input sample, return
 * the echoed output to be mixed into the L/R outputs.  This matches
 * the algorithm in the disassembled DSP program: average the two
 * most recent ring entries, scale the average by the feedback gain
 * and add to the new input, write the result back to the ring, and
 * return the (unfiltered) average. */
static int32_t qsound_echo_apply(int32_t input)
{
	int32_t old_sample = qsound_echo_buffer[qsound_echo_pos];
	int32_t last       = qsound_echo_last;
	int32_t new_sample;

	qsound_echo_last = old_sample;
	/* 2-tap moving average over delay-line output */
	old_sample = (old_sample + last) >> 1;

	/* Add the feedback-attenuated average back to the current
	 * accumulated voice input, write the result to the delay line
	 * (scaled down to fit the int32 ring slot range we use). */
	new_sample = input + ((old_sample * qsound_echo_feedback) >> 14);
	qsound_echo_buffer[qsound_echo_pos] = new_sample;

	qsound_echo_pos++;
	if (qsound_echo_pos >= qsound_echo_length)
		qsound_echo_pos = 0;

	return old_sample;
}

static void qsound_update( int num, int16_t **buffer, int length )
{
	/* Per-sample outer loop: precompute per-voice setup once, then for
	 * each output sample step through every active voice, accumulating
	 * its contribution into the L/R output samples.  This structure is
	 * the natural shape for any "post-process every voice's
	 * contribution" effect (echo, FIR); the old per-voice-outer
	 * structure made those harder to add. */
	int i,j;
	int16_t *pOutL;
	int16_t *pOutR;
	/* Per-voice state cached for the whole buffer */
	int32_t lvol[QSOUND_CHANNELS];
	int32_t rvol[QSOUND_CHANNELS];
	int8_t *pST[QSOUND_CHANNELS];

	if (Machine->sample_rate == 0) return;

	pOutL = buffer[0];
	pOutR = buffer[1];
	memset(pOutL, 0x00, length * sizeof(int16_t));
	memset(pOutR, 0x00, length * sizeof(int16_t));

	/* Set up per-voice constants: combined L/R volume scaling and the
	 * sample-ROM base pointer for this voice's bank.  Inactive voices
	 * are skipped via the .key flag inside the loop body. */
	for (i = 0; i < QSOUND_CHANNELS; i++)
	{
		if (qsound_channel[i].key)
		{
			lvol[i] = (qsound_channel[i].lvol * qsound_channel[i].vol) >> 8;
			rvol[i] = (qsound_channel[i].rvol * qsound_channel[i].vol) >> 8;
			pST[i]  = qsound_sample_rom + qsound_channel[i].bank;
		}
	}

	for (j = 0; j < length; j++)
	{
		struct qsound_channel *pC = &qsound_channel[0];
		int32_t lacc = 0;
		int32_t racc = 0;
		int32_t echo_in = 0;
		int32_t echo_out;
		for (i = 0; i < QSOUND_CHANNELS; i++, pC++)
		{
			int count, v;
			if (!pC->key)
				continue;
			count = (pC->offset) >> 16;
			pC->offset &= 0xffff;
			if (count)
			{
				pC->address += count;
				if (pC->address >= pC->end)
				{
					if (!pC->loop)
					{
						/* Reached the end of a non-looped sample */
						pC->key = 0;
						continue;
					}
					/* Reached the end, restart the loop */
					pC->address = (pC->end - pC->loop) & 0xffff;
				}
				pC->lastdt = pST[i][pC->address];
			}
			v = pC->lastdt;
			lacc += (v * lvol[i]) >> 6;
			racc += (v * rvol[i]) >> 6;
			/* Per-voice echo send: pre-pan, volume-scaled sample
			 * multiplied by the echo send level (register 0xba+ch).
			 * Voices with .echo == 0 contribute nothing, so the
			 * branch around this is left to the compiler. */
			if (pC->echo)
				echo_in += ((v * pC->vol) >> 8) * pC->echo;
			pC->offset += pC->pitch;
		}
		/* Run the echo state machine for this output sample.  Skip
		 * entirely if the delay line is disabled (length 0) or there
		 * is no feedback and no voice contributed input -- this is
		 * the common case for non-Q-Sound games (the channel struct
		 * is zero-initialized) and the QSound boot window before
		 * the game first programs the echo registers. */
		if (qsound_echo_length > 0)
		{
			echo_out = qsound_echo_apply(echo_in);
			lacc += echo_out;
			racc += echo_out;
		}
		pOutL[j] = lacc;
		pOutR[j] = racc;
	}
}

int qsound_sh_start(const struct MachineSound *msound)
{
	int i;

	if (Machine->sample_rate == 0) return 0;

	intf = (struct QSound_interface*)msound->sound_interface;

	qsound_sample_rom = (int8_t *)memory_region(intf->region);

	memset(qsound_channel, 0, sizeof(qsound_channel));

	/* Initialize global echo state.  Matches the DSP program's boot-
	 * time values: the delay-line ring is empty, the end-position
	 * register is set to BASE_OFFSET + 6 (so the initial length is 6
	 * samples until the game programs a real value via register
	 * 0xd9), feedback is zero (no echo), and the moving-average
	 * carry slot starts at zero.  Per-voice .echo defaults to 0 from
	 * the channel-struct memset above. */
	memset(qsound_echo_buffer, 0, sizeof(qsound_echo_buffer));
	qsound_echo_pos       = 0;
	qsound_echo_end_pos   = QSOUND_DELAY_BASE_OFFSET + 6;
	qsound_echo_length    = 6;
	qsound_echo_feedback  = 0;
	qsound_echo_last      = 0;

	qsound_frq_ratio = ((float)intf->clock / (float)QSOUND_CLOCKDIV) /
						(float) Machine->sample_rate;
	qsound_frq_ratio *= 16.0;

	/* Create pan table */
	for (i=0; i<33; i++)
		qsound_pan_table[i]=(int)((256/sqrt(32)) * sqrt(i));
	{
		/* Allocate stream */
		char buf[2][40];
		const char *name[2];
		int  vol[2];
		name[0] = buf[0];
		name[1] = buf[1];
		sprintf( buf[0], "%s L", sound_name(msound) );
		sprintf( buf[1], "%s R", sound_name(msound) );
		vol[0]=MIXER(intf->mixing_level[0], MIXER_PAN_LEFT);
		vol[1]=MIXER(intf->mixing_level[1], MIXER_PAN_RIGHT);
		qsound_stream = stream_init_multi(2,
			name,
			vol,
			Machine->sample_rate,
			0,
			qsound_update );
	}

	return 0;
}

void qsound_sh_stop (void)
{
	if (Machine->sample_rate == 0) return;
}

WRITE_HANDLER( qsound_data_h_w )
{
	qsound_data=(qsound_data&0xff)|(data<<8);
}

WRITE_HANDLER( qsound_data_l_w )
{
	qsound_data=(qsound_data&0xff00)|data;
}

static void qsound_set_command(int data, int value)
{
	int ch=0,reg=0;
	if (data < 0x80)
	{
		ch=data>>3;
		reg=data & 0x07;
	}
	else
	{
		if (data < 0x90)
		{
			ch=data-0x80;
			reg=8;
		}
		else if (data == 0x93)
		{
			/* Global echo feedback gain */
			reg = 10;
		}
		else if (data == 0xd9)
		{
			/* Global echo delay-line end position; length is
			 * derived as (end_pos - QSOUND_DELAY_BASE_OFFSET) and
			 * is clamped into the local ring buffer below. */
			reg = 11;
		}
		else
		{
			if (data >= 0xba && data < 0xca)
			{
				ch=data-0xba;
				reg=9;
			}
			else
			{
				/* Unknown registers */
				ch=99;
				reg=99;
			}
		}
	}

	switch (reg)
	{
		case 0: /* Bank */
			ch=(ch+1)&0x0f;	/* strange ... */
			qsound_channel[ch].bank=(value&0x7f)<<16;
			break;
		case 1: /* start */
			qsound_channel[ch].address=value;
			break;
		case 2: /* pitch */
			qsound_channel[ch].pitch=(long)
					((float)value * qsound_frq_ratio );
			if (!value)
			{
				/* Key off */
				qsound_channel[ch].key=0;
			}
			break;
		case 3: /* unknown */
			qsound_channel[ch].reg3=value;
			break;
		case 4: /* loop offset */
			qsound_channel[ch].loop=value;
			break;
		case 5: /* end */
			qsound_channel[ch].end=value;
			break;
		case 6: /* master volume */
			if (value==0)
			{
				/* Key off */
				qsound_channel[ch].key=0;
			}
			else if (qsound_channel[ch].key==0)
			{
				/* Key on */
				qsound_channel[ch].key=1;
				qsound_channel[ch].offset=0;
				qsound_channel[ch].lastdt=0;
			}
			qsound_channel[ch].vol=value;
			break;

		case 7:  /* unused */
			break;
		case 8:
			{
			   int pandata=(value-0x10)&0x3f;
			   if (pandata > 32)
					pandata=32;
			   qsound_channel[ch].lvol=qsound_pan_table[pandata];
			   qsound_channel[ch].rvol=qsound_pan_table[32-pandata];
			   qsound_channel[ch].pan = value;
			}
			break;
		 case 9: /* per-voice echo send (register 0xba+ch) */
			qsound_channel[ch].echo=value;
			break;
		 case 10: /* global echo feedback (register 0x93) */
			qsound_echo_feedback = (int16_t)value;
			break;
		 case 11: /* global echo end-position (register 0xd9) */
			qsound_echo_end_pos = value;
			{
				int len = value - QSOUND_DELAY_BASE_OFFSET;
				if (len < 0) len = 0;
				if (len > QSOUND_DELAY_BUFFER_LEN)
					len = QSOUND_DELAY_BUFFER_LEN;
				qsound_echo_length = len;
				/* Reset the ring position so the new delay window
				 * starts coherently rather than mid-tap. */
				if (qsound_echo_pos >= len)
					qsound_echo_pos = 0;
			}
			break;
	}
}



WRITE_HANDLER( qsound_cmd_w )
{
	qsound_set_command(data, qsound_data);
}

READ_HANDLER( qsound_status_r )
{
	/* Port ready bit (0x80 if ready) */
	return 0x80;
}
