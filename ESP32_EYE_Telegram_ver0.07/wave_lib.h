/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __WAVLIB_H
#define __WAVLIB_H

#include <stdint.h>

typedef struct {
  uint32_t ChunkID;       /* 0 */ 
  uint32_t FileSize;      /* 4 */
  uint32_t FileFormat;    /* 8 */
  uint32_t SubChunk1ID;   /* 12 */
  uint32_t SubChunk1Size; /* 16*/  
  uint16_t AudioFormat;   /* 20 */ 
  uint16_t NbrChannels;   /* 22 */   
  uint32_t SampleRate;    /* 24 */
  
  uint32_t ByteRate;      /* 28 */
  uint16_t BlockAlign;    /* 32 */  
  uint16_t BitPerSample;  /* 34 */  
  uint32_t SubChunk2ID;   /* 36 */   
  uint32_t SubChunk2Size; /* 40 */    
} WAVE_FormatTypeDef;

#define WAVE_HEADER_LEN		(44)

uint32_t WavProcess_HeaderUpdate(uint8_t* pHeader, WAVE_FormatTypeDef* pWaveFormatStruct);
uint32_t WavProcess_EncInit(uint32_t Freq, uint8_t *pHeader, WAVE_FormatTypeDef* pWaveFormatStruct);

#endif
