/*###################################################################################################
**
**	TMS34010: Portable Texas Instruments TMS34010 emulator
**
**	Copyright (C) Alex Pasadyn/Zsolt Vasvari 1998
**	 Parts based on code by Aaron Giles
**
**#################################################################################################*/

#include <stdio.h>
#include "driver.h"
#include "osd_cpu.h"
#include "tms34010.h"
#include "34010ops.h"


/*###################################################################################################
**	FIELD WRITE FUNCTIONS
**#################################################################################################*/

WRITE_HANDLER( wfield_01 )
{
	WFIELDMAC(0x01,16);
}

WRITE_HANDLER( wfield_02 )
{
	WFIELDMAC(0x03,15);
}

WRITE_HANDLER( wfield_03 )
{
	WFIELDMAC(0x07,14);
}

WRITE_HANDLER( wfield_04 )
{
	WFIELDMAC(0x0f,13);
}

WRITE_HANDLER( wfield_05 )
{
	WFIELDMAC(0x1f,12);
}

WRITE_HANDLER( wfield_06 )
{
	WFIELDMAC(0x3f,11);
}

WRITE_HANDLER( wfield_07 )
{
	WFIELDMAC(0x7f,10);
}

WRITE_HANDLER( wfield_08 )
{
	WFIELDMAC_8;
}

WRITE_HANDLER( wfield_09 )
{
	WFIELDMAC(0x1ff,8);
}

WRITE_HANDLER( wfield_10 )
{
	WFIELDMAC(0x3ff,7);
}

WRITE_HANDLER( wfield_11 )
{
	WFIELDMAC(0x7ff,6);
}

WRITE_HANDLER( wfield_12 )
{
	WFIELDMAC(0xfff,5);
}

WRITE_HANDLER( wfield_13 )
{
	WFIELDMAC(0x1fff,4);
}

WRITE_HANDLER( wfield_14 )
{
	WFIELDMAC(0x3fff,3);
}

WRITE_HANDLER( wfield_15 )
{
	WFIELDMAC(0x7fff,2);
}

WRITE_HANDLER( wfield_16 )
{
	if (offset & 0x0f)
	{
		WFIELDMAC(0xffff,1);
	}
	else
	{
		TMS34010_WRMEM_WORD(TOBYTE(offset),data);
	}
}

WRITE_HANDLER( wfield_17 )
{
	WFIELDMAC(0x1ffff,0);
}

WRITE_HANDLER( wfield_18 )
{
	WFIELDMAC_BIG(0x3ffff,15);
}

WRITE_HANDLER( wfield_19 )
{
	WFIELDMAC_BIG(0x7ffff,14);
}

WRITE_HANDLER( wfield_20 )
{
	WFIELDMAC_BIG(0xfffff,13);
}

WRITE_HANDLER( wfield_21 )
{
	WFIELDMAC_BIG(0x1fffff,12);
}

WRITE_HANDLER( wfield_22 )
{
	WFIELDMAC_BIG(0x3fffff,11);
}

WRITE_HANDLER( wfield_23 )
{
	WFIELDMAC_BIG(0x7fffff,10);
}

WRITE_HANDLER( wfield_24 )
{
	WFIELDMAC_BIG(0xffffff,9);
}

WRITE_HANDLER( wfield_25 )
{
	WFIELDMAC_BIG(0x1ffffff,8);
}

WRITE_HANDLER( wfield_26 )
{
	WFIELDMAC_BIG(0x3ffffff,7);
}

WRITE_HANDLER( wfield_27 )
{
	WFIELDMAC_BIG(0x7ffffff,6);
}

WRITE_HANDLER( wfield_28 )
{
	WFIELDMAC_BIG(0xfffffff,5);
}

WRITE_HANDLER( wfield_29 )
{
	WFIELDMAC_BIG(0x1fffffff,4);
}

WRITE_HANDLER( wfield_30 )
{
	WFIELDMAC_BIG(0x3fffffff,3);
}

WRITE_HANDLER( wfield_31 )
{
	WFIELDMAC_BIG(0x7fffffff,2);
}

WRITE_HANDLER( wfield_32 )
{
	WFIELDMAC_32;
}



/*###################################################################################################
**	FIELD READ FUNCTIONS (ZERO-EXTEND)
**#################################################################################################*/

READ_HANDLER( rfield_z_01 )
{
	uint32_t ret;
	RFIELDMAC(0x01,16);
	return ret;
}

READ_HANDLER( rfield_z_02 )
{
	uint32_t ret;
	RFIELDMAC(0x03,15);
	return ret;
}

READ_HANDLER( rfield_z_03 )
{
	uint32_t ret;
	RFIELDMAC(0x07,14);
	return ret;
}

READ_HANDLER( rfield_z_04 )
{
	uint32_t ret;
	RFIELDMAC(0x0f,13);
	return ret;
}

READ_HANDLER( rfield_z_05 )
{
	uint32_t ret;
	RFIELDMAC(0x1f,12);
	return ret;
}

READ_HANDLER( rfield_z_06 )
{
	uint32_t ret;
	RFIELDMAC(0x3f,11);
	return ret;
}

READ_HANDLER( rfield_z_07 )
{
	uint32_t ret;
	RFIELDMAC(0x7f,10);
	return ret;
}

READ_HANDLER( rfield_z_08 )
{
	uint32_t ret;
	RFIELDMAC_8;
	return ret;
}

READ_HANDLER( rfield_z_09 )
{
	uint32_t ret;
	RFIELDMAC(0x1ff,8);
	return ret;
}

READ_HANDLER( rfield_z_10 )
{
	uint32_t ret;
	RFIELDMAC(0x3ff,7);
	return ret;
}

READ_HANDLER( rfield_z_11 )
{
	uint32_t ret;
	RFIELDMAC(0x7ff,6);
	return ret;
}

READ_HANDLER( rfield_z_12 )
{
	uint32_t ret;
	RFIELDMAC(0xfff,5);
	return ret;
}

READ_HANDLER( rfield_z_13 )
{
	uint32_t ret;
	RFIELDMAC(0x1fff,4);
	return ret;
}

READ_HANDLER( rfield_z_14 )
{
	uint32_t ret;
	RFIELDMAC(0x3fff,3);
	return ret;
}

READ_HANDLER( rfield_z_15 )
{
	uint32_t ret;
	RFIELDMAC(0x7fff,2);
	return ret;
}

READ_HANDLER( rfield_z_16 )
{
	uint32_t ret;
	if (offset & 0x0f)
	{
		RFIELDMAC(0xffff,1);
	}

	else
		ret = TMS34010_RDMEM_WORD(TOBYTE(offset));
	return ret;
}

READ_HANDLER( rfield_z_17 )
{
	uint32_t ret;
	RFIELDMAC(0x1ffff,0);
	return ret;
}

READ_HANDLER( rfield_z_18 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x3ffff,15);
	return ret;
}

READ_HANDLER( rfield_z_19 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x7ffff,14);
	return ret;
}

READ_HANDLER( rfield_z_20 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0xfffff,13);
	return ret;
}

READ_HANDLER( rfield_z_21 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x1fffff,12);
	return ret;
}

READ_HANDLER( rfield_z_22 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x3fffff,11);
	return ret;
}

READ_HANDLER( rfield_z_23 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x7fffff,10);
	return ret;
}

READ_HANDLER( rfield_z_24 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0xffffff,9);
	return ret;
}

READ_HANDLER( rfield_z_25 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x1ffffff,8);
	return ret;
}

READ_HANDLER( rfield_z_26 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x3ffffff,7);
	return ret;
}

READ_HANDLER( rfield_z_27 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x7ffffff,6);
	return ret;
}

READ_HANDLER( rfield_z_28 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0xfffffff,5);
	return ret;
}

READ_HANDLER( rfield_z_29 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x1fffffff,4);
	return ret;
}

READ_HANDLER( rfield_z_30 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x3fffffff,3);
	return ret;
}

READ_HANDLER( rfield_z_31 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x7fffffff,2);
	return ret;
}

READ_HANDLER( rfield_32 )
{
	RFIELDMAC_32;
}



/*###################################################################################################
**	FIELD READ FUNCTIONS (SIGN-EXTEND)
**#################################################################################################*/

READ_HANDLER( rfield_s_01 )
{
	uint32_t ret;
	RFIELDMAC(0x01,16);
	return ((int32_t)(ret << 31)) >> 31;
}

READ_HANDLER( rfield_s_02 )
{
	uint32_t ret;
	RFIELDMAC(0x03,15);
	return ((int32_t)(ret << 30)) >> 30;
}

READ_HANDLER( rfield_s_03 )
{
	uint32_t ret;
	RFIELDMAC(0x07,14);
	return ((int32_t)(ret << 29)) >> 29;
}

READ_HANDLER( rfield_s_04 )
{
	uint32_t ret;
	RFIELDMAC(0x0f,13);
	return ((int32_t)(ret << 28)) >> 28;
}

READ_HANDLER( rfield_s_05 )
{
	uint32_t ret;
	RFIELDMAC(0x1f,12);
	return ((int32_t)(ret << 27)) >> 27;
}

READ_HANDLER( rfield_s_06 )
{
	uint32_t ret;
	RFIELDMAC(0x3f,11);
	return ((int32_t)(ret << 26)) >> 26;
}

READ_HANDLER( rfield_s_07 )
{
	uint32_t ret;
	RFIELDMAC(0x7f,10);
	return ((int32_t)(ret << 25)) >> 25;
}

READ_HANDLER( rfield_s_08 )
{
	uint32_t ret;
	if (offset & 0x07)											
	{															
		RFIELDMAC(0xff,9);
	}
															
	else														
		ret = TMS34010_RDMEM(TOBYTE(offset));					
	return (int32_t)(int8_t)ret;
}

READ_HANDLER( rfield_s_09 )
{
	uint32_t ret;
	RFIELDMAC(0x1ff,8);
	return ((int32_t)(ret << 23)) >> 23;
}

READ_HANDLER( rfield_s_10 )
{
	uint32_t ret;
	RFIELDMAC(0x3ff,7);
	return ((int32_t)(ret << 22)) >> 22;
}

READ_HANDLER( rfield_s_11 )
{
	uint32_t ret;
	RFIELDMAC(0x7ff,6);
	return ((int32_t)(ret << 21)) >> 21;
}

READ_HANDLER( rfield_s_12 )
{
	uint32_t ret;
	RFIELDMAC(0xfff,5);
	return ((int32_t)(ret << 20)) >> 20;
}

READ_HANDLER( rfield_s_13 )
{
	uint32_t ret;
	RFIELDMAC(0x1fff,4);
	return ((int32_t)(ret << 19)) >> 19;
}

READ_HANDLER( rfield_s_14 )
{
	uint32_t ret;
	RFIELDMAC(0x3fff,3);
	return ((int32_t)(ret << 18)) >> 18;
}

READ_HANDLER( rfield_s_15 )
{
	uint32_t ret;
	RFIELDMAC(0x7fff,2);
	return ((int32_t)(ret << 17)) >> 17;
}

READ_HANDLER( rfield_s_16 )
{
	uint32_t ret;
	if (offset & 0x0f)
	{
		RFIELDMAC(0xffff,1);
	}

	else
	{
		ret = TMS34010_RDMEM_WORD(TOBYTE(offset));
	}

	return (int32_t)(int16_t)ret;
}

READ_HANDLER( rfield_s_17 )
{
	uint32_t ret;
	RFIELDMAC(0x1ffff,0);
	return ((int32_t)(ret << 15)) >> 15;
}

READ_HANDLER( rfield_s_18 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x3ffff,15);
	return ((int32_t)(ret << 14)) >> 14;
}

READ_HANDLER( rfield_s_19 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x7ffff,14);
	return ((int32_t)(ret << 13)) >> 13;
}

READ_HANDLER( rfield_s_20 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0xfffff,13);
	return ((int32_t)(ret << 12)) >> 12;
}

READ_HANDLER( rfield_s_21 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x1fffff,12);
	return ((int32_t)(ret << 11)) >> 11;
}

READ_HANDLER( rfield_s_22 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x3fffff,11);
	return ((int32_t)(ret << 10)) >> 10;
}

READ_HANDLER( rfield_s_23 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x7fffff,10);
	return ((int32_t)(ret << 9)) >> 9;
}

READ_HANDLER( rfield_s_24 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0xffffff,9);
	return ((int32_t)(ret << 8)) >> 8;
}

READ_HANDLER( rfield_s_25 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x1ffffff,8);
	return ((int32_t)(ret << 7)) >> 7;
}

READ_HANDLER( rfield_s_26 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x3ffffff,7);
	return ((int32_t)(ret << 6)) >> 6;
}

READ_HANDLER( rfield_s_27 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x7ffffff,6);
	return ((int32_t)(ret << 5)) >> 5;
}

READ_HANDLER( rfield_s_28 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0xfffffff,5);
	return ((int32_t)(ret << 4)) >> 4;
}

READ_HANDLER( rfield_s_29 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x1fffffff,4);
	return ((int32_t)(ret << 3)) >> 3;
}

READ_HANDLER( rfield_s_30 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x3fffffff,3);
	return ((int32_t)(ret << 2)) >> 2;
}

READ_HANDLER( rfield_s_31 )
{
	uint32_t ret;
	RFIELDMAC_BIG(0x7fffffff,2);
	return ((int32_t)(ret << 1)) >> 1;
}


