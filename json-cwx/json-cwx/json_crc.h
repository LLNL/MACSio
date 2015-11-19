/**********************************************************************
 *
 * Filename:    json_crc.h
 * 
 * Description: A header file describing the various CRC standards.
 *
 * Notes:       
 *
 * 
 * Copyright (c) 2000 by Michael Barr.  This software is placed into
 * the public domain and may be used for any purpose.  However, this
 * notice must not be changed or removed and no warranty is either
 * expressed or implied by its publication or distribution.
 **********************************************************************/
#ifndef _JSON_CRC_H
#define _JSON_CRC_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Select the CRC standard from the list that follows.
 */
#define JSON_C_CRC32

#if defined(JSON_C_CRC_CCITT)

typedef unsigned short  json_crc;

#define JSON_C_CRC_NAME			"CRC-CCITT"
#define JSON_C_POLYNOMIAL		0x1021
#define JSON_C_INITIAL_REMAINDER	0xFFFF
#define JSON_C_FINAL_XOR_VALUE		0x0000
#define JSON_C_REFLECT_DATA		JSON_C_FALSE
#define JSON_C_REFLECT_REMAINDER	JSON_C_FALSE
#define JSON_C_CHECK_VALUE		0x29B1

#elif defined(JSON_C_CRC16)

typedef unsigned short  json_crc;

#define JSON_C_CRC_NAME			"CRC-16"
#define JSON_C_POLYNOMIAL		0x8005
#define JSON_C_INITIAL_REMAINDER	0x0000
#define JSON_C_FINAL_XOR_VALUE		0x0000
#define JSON_C_REFLECT_DATA		JSON_C_TRUE
#define JSON_C_REFLECT_REMAINDER	JSON_C_TRUE
#define JSON_C_CHECK_VALUE		0xBB3D

#elif defined(JSON_C_CRC32)

typedef unsigned json_crc;

#define JSON_C_CRC_NAME			"CRC-32"
#define JSON_C_POLYNOMIAL		0x04C11DB7
#define JSON_C_INITIAL_REMAINDER	0xFFFFFFFF
#define JSON_C_FINAL_XOR_VALUE		0xFFFFFFFF
#define JSON_C_REFLECT_DATA		JSON_C_TRUE
#define JSON_C_REFLECT_REMAINDER	JSON_C_TRUE
#define JSON_C_CHECK_VALUE		0xCBF43926

#else

#error "One of JSON_C_CRC_CCITT, JSON_C_CRC16, or JSON_C_CRC32 must be #define'd."

#endif

extern void     json_crcInit(void);
extern json_crc json_crcFast(unsigned char const message[], int nBytes);

#ifdef __cplusplus
}
#endif

#endif
