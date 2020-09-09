#include <stdint.h>
#include "wave_lib.h"
/*
*    main ....
*
*   // Header per il file wave salvato. 
*   WAVE_FormatTypeDef wf;
*   uint8_t pHeaderBuff[WAVE_HEADER_LEN];
*   uint32_t FreqSampling = 16000;  // Freq. di campionamento
*   uint32_t RecordTime = 3;        // secondi di registrazione
*   
*	// Apertura del file 
*	if ((res = f_open(&FileRec, fname, FA_OPEN_ALWAYS | FA_WRITE)) == FR_OK)
*	{
*		// Azzero la struttura dati della Wave 
*		memset( (WAVE_FormatTypeDef*)&wf, 0, sizeof(wf));
*		memset( (uint8_t*)&pHeaderBuff, 0, sizeof(pHeaderBuff));
*		// Preparazione e scrittura dell'header wave. 
*		WavProcess_EncInit( FreqSampling, pHeaderBuff, &wf);
*		res = f_write(&FileRec, pHeaderBuff, sizeof(pHeaderBuff), (void *)&byteswritten);
*   // Aggiorno la dimensione del file.
*   wf.FileSize = (FreqSampling*sizeof(int16_t)*RecordTime);
*   WavProcess_HeaderUpdate( pHeaderBuff, &wf);
*   res = f_lseek(&FileRec, 0);
*   // Scrivo la nuova header con la dimensione finale. 
*   res = f_write(&FileRec, pHeaderBuff, sizeof(pHeaderBuff), (void *)&byteswritten);
*
*/

/**
  * @brief  Encoder initialization.
  * @param  Freq: Sampling frequency.
  * @param  pHeader: Pointer to the WAV file header to be written.  
  * @retval 0 if success, !0 else.
  */
uint32_t WavProcess_EncInit(uint32_t Freq, uint8_t*pHeader, WAVE_FormatTypeDef*pWaveFormatStruct)
{  
  /* Initialize the encoder structure */
  pWaveFormatStruct->SampleRate = Freq;        /* Audio sampling frequency */
  pWaveFormatStruct->NbrChannels = 1;          /* Number of channels: 1:Mono or 2:Stereo */
  pWaveFormatStruct->BitPerSample = 16;        /* Number of bits per sample (16, 24 or 32) */
  pWaveFormatStruct->FileSize = 0x00000000;    /* Total length of useful audio data (payload) */
  pWaveFormatStruct->SubChunk1Size = 44;       /* The file header chunk size */
  pWaveFormatStruct->ByteRate = (pWaveFormatStruct->SampleRate * \
                                (pWaveFormatStruct->BitPerSample/8) * \
                                 pWaveFormatStruct->NbrChannels);     /* Number of bytes per second  (sample rate * block align)  */
  pWaveFormatStruct->BlockAlign = pWaveFormatStruct->NbrChannels * \
                                 (pWaveFormatStruct->BitPerSample/8); /* channels * bits/sample / 8 */
  
  /* Parse the wav file header and extract required information */
  /* Write chunkID, must be 'RIFF'  ------------------------------------------*/
  pHeader[0] = 'R';
  pHeader[1] = 'I';
  pHeader[2] = 'F';
  pHeader[3] = 'F';
  
  /* Write the file length ---------------------------------------------------*/
  /* The sampling time: this value will be written back at the end of the 
     recording operation.  Example: 661500 Btyes = 0x000A17FC, byte[7]=0x00, byte[4]=0xFC */
  pHeader[4] = 0x00;
  pHeader[5] = 0x00;
  pHeader[6] = 0x00;
  pHeader[7] = 0x00;
  /* Write the file format, must be 'WAVE' -----------------------------------*/
  pHeader[8]  = 'W';
  pHeader[9]  = 'A';
  pHeader[10] = 'V';
  pHeader[11] = 'E';
  
  /* Write the format chunk, must be'fmt ' -----------------------------------*/
  pHeader[12]  = 'f';
  pHeader[13]  = 'm';
  pHeader[14]  = 't';
  pHeader[15]  = ' ';
  
  /* Write the length of the 'fmt' data, must be 0x10 ------------------------*/
  pHeader[16]  = 0x10;
  pHeader[17]  = 0x00;
  pHeader[18]  = 0x00;
  pHeader[19]  = 0x00;
  
  /* Write the audio format, must be 0x01 (PCM) ------------------------------*/
  pHeader[20]  = 0x01;
  pHeader[21]  = 0x00;
  
  /* Write the number of channels, ie. 0x01 (Mono) ---------------------------*/
  pHeader[22]  = pWaveFormatStruct->NbrChannels;
  pHeader[23]  = 0x00;
  
  /* Write the Sample Rate in Hz ---------------------------------------------*/
  /* Write Little Endian ie. 8000 = 0x00001F40 => byte[24]=0x40, byte[27]=0x00*/
  pHeader[24]  = (uint8_t)((pWaveFormatStruct->SampleRate & 0xFF));
  pHeader[25]  = (uint8_t)((pWaveFormatStruct->SampleRate >> 8) & 0xFF);
  pHeader[26]  = (uint8_t)((pWaveFormatStruct->SampleRate >> 16) & 0xFF);
  pHeader[27]  = (uint8_t)((pWaveFormatStruct->SampleRate >> 24) & 0xFF);
  
  /* Write the Byte Rate -----------------------------------------------------*/
  pHeader[28]  = (uint8_t)((pWaveFormatStruct->ByteRate & 0xFF));
  pHeader[29]  = (uint8_t)((pWaveFormatStruct->ByteRate >> 8) & 0xFF);
  pHeader[30]  = (uint8_t)((pWaveFormatStruct->ByteRate >> 16) & 0xFF);
  pHeader[31]  = (uint8_t)((pWaveFormatStruct->ByteRate >> 24) & 0xFF);
  
  /* Write the block alignment -----------------------------------------------*/
  pHeader[32]  = pWaveFormatStruct->BlockAlign;
  pHeader[33]  = 0x00;
  
  /* Write the number of bits per sample -------------------------------------*/
  pHeader[34]  = pWaveFormatStruct->BitPerSample;
  pHeader[35]  = 0x00;
  
  /* Write the Data chunk, must be 'data' ------------------------------------*/
  pHeader[36]  = 'd';
  pHeader[37]  = 'a';
  pHeader[38]  = 't';
  pHeader[39]  = 'a';
  
  /* Write the number of sample data -----------------------------------------*/
  /* This variable will be written back at the end of the recording operation */
  pHeader[40]  = 0x00;
  pHeader[41]  = 0x00;
  pHeader[42]  = 0x00;
  pHeader[43]  = 0x00;
  
  /* Return 0 if all operations are OK */
  return 0;
}

/**
  * @brief  Initialize the wave header file
  * @param  pHeader: Header Buffer to be filled
  * @param  pWaveFormatStruct: Pointer to the wave structure to be filled.
  * @retval 0 if passed, !0 if failed.
  */
uint32_t WavProcess_HeaderUpdate(uint8_t* pHeader, WAVE_FormatTypeDef*pWaveFormatStruct)
{
	uint32_t len;
	
  /* Write the file length ---------------------------------------------------*/
  /* The sampling time: this value will be written back at the end of the 
     recording operation.  Example: 661500 Btyes = 0x000A17FC, byte[7]=0x00, byte[4]=0xFC */
	len=pWaveFormatStruct->FileSize+36;
  pHeader[4] = (uint8_t)( len & 0xFF);
  pHeader[5] = (uint8_t)((len >> 8)  & 0xFF);
  pHeader[6] = (uint8_t)((len >> 16) & 0xFF);
  pHeader[7] = (uint8_t)((len >> 24) & 0xFF);
  /* Write the number of sample data -----------------------------------------*/
  /* This variable will be written back at the end of the recording operation */
  pHeader[40] = (uint8_t) (pWaveFormatStruct->FileSize & 0xFF); 
  pHeader[41] = (uint8_t)((pWaveFormatStruct->FileSize >> 8)  & 0xFF);
  pHeader[42] = (uint8_t)((pWaveFormatStruct->FileSize >> 16) & 0xFF);
  pHeader[43] = (uint8_t)((pWaveFormatStruct->FileSize >> 24) & 0xFF); 
  
  /* Return 0 if all operations are OK */
  return 0;
}
