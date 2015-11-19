/**********************************************************************
 *
 * Filename:    crc.c
 * 
 * Description: Slow and fast implementations of the CRC standards.
 *
 * Notes:       The parameters for each supported CRC standard are
 *		defined in the header file crc.h.  The implementations
 *		here should stand up to further additions to that list.
 *
 * 
 * Copyright (c) 2000 by Michael Barr.  This software is placed into
 * the public domain and may be used for any purpose.  However, this
 * notice must not be changed or removed and no warranty is either
 * expressed or implied by its publication or distribution.
 **********************************************************************/
 
#include "crc.h"


/*
 * Derive parameters from the standard-specific parameters in crc.h.
 */
#define WIDTH    (8 * sizeof(crc))
#define TOPBIT   (1 << (WIDTH - 1))

#if (JSON_C_REFLECT_DATA == TRUE)
#undef  JSON_C_REFLECT_DATA
#define JSON_C_REFLECT_DATA(X)			((unsigned char) reflect((X), 8))
#else
#undef  JSON_C_REFLECT_DATA
#define JSON_C_REFLECT_DATA(X)			(X)
#endif

#if (JSON_C_REFLECT_REMAINDER == TRUE)
#undef  JSON_C_REFLECT_REMAINDER
#define JSON_C_REFLECT_REMAINDER(X)	((crc) reflect((X), WIDTH))
#else
#undef  JSON_C_REFLECT_REMAINDER
#define JSON_C_REFLECT_REMAINDER(X)	(X)
#endif


/*********************************************************************
 *
 * Function:    reflect()
 * 
 * Description: Reorder the bits of a binary sequence, by reflecting
 *		them about the middle position.
 *
 * Notes:	No checking is done that nBits <= 32.
 *
 * Returns:	The reflection of the original data.
 *
 *********************************************************************/
static unsigned long
reflect(unsigned long data, unsigned char nBits)
{
	unsigned long  reflection = 0x00000000;
	unsigned char  bit;

	/*
	 * Reflect the data about the center bit.
	 */
	for (bit = 0; bit < nBits; ++bit)
	{
		/*
		 * If the LSB bit is set, set the reflection of it.
		 */
		if (data & 0x01)
		{
			reflection |= (1 << ((nBits - 1) - bit));
		}

		data = (data >> 1);
	}

	return (reflection);

}	/* reflect() */


json_crc  crcTable[256];


/*********************************************************************
 *
 * Function:    crcInit()
 * 
 * Description: Populate the partial CRC lookup table.
 *
 * Notes:	This function must be rerun any time the CRC standard
 *		is changed.  If desired, it can be run "offline" and
 *		the table results stored in an embedded system's ROM.
 *
 * Returns:	None defined.
 *
 *********************************************************************/
void
crcInit(void)
{
    json_crc	  remainder;
    int		  dividend;
    unsigned char bit;


    /*
     * Compute the remainder of each possible dividend.
     */
    for (dividend = 0; dividend < 256; ++dividend)
    {
        /*
         * Start with the dividend followed by zeros.
         */
        remainder = dividend << (WIDTH - 8);

        /*
         * Perform modulo-2 division, a bit at a time.
         */
        for (bit = 8; bit > 0; --bit)
        {
            /*
             * Try to divide the current data bit.
             */			
            if (remainder & JSON_C_TOPBIT)
            {
                remainder = (remainder << 1) ^ JSON_C_POLYNOMIAL;
            }
            else
            {
                remainder = (remainder << 1);
            }
        }

        /*
         * Store the result into the table.
         */
        crcTable[dividend] = remainder;
    }

}   /* crcInit() */


/*********************************************************************
 *
 * Function:    crcFast()
 * 
 * Description: Compute the CRC of a given message.
 *
 * Notes:		crcInit() must be called first.
 *
 * Returns:		The CRC of the message.
 *
 *********************************************************************/
json_crc
crcFast(unsigned char const message[], int nBytes)
{
    json_crc	  remainder = JSON_C_INITIAL_REMAINDER;
    unsigned char data;
    int           byte;


    /*
     * Divide the message by the polynomial, a byte at a time.
     */
    for (byte = 0; byte < nBytes; ++byte)
    {
        data = JSON_C_REFLECT_DATA(message[byte]) ^ (remainder >> (WIDTH - 8));
  		remainder = crcTable[data] ^ (remainder << 8);
    }

    /*
     * The final remainder is the CRC.
     */
    return (JSON_C_REFLECT_REMAINDER(remainder) ^ JSON_C_FINAL_XOR_VALUE);

}   /* crcFast() */
