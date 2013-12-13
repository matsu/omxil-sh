/**
   src/vpu5/shvpu5_avcenc.h

   This component implements H.264 / MPEG-4 AVC video codec.
   The H.264 / MPEG-4 AVC video encoder/decoder is implemented
   on the Renesas's VPU5HG middleware library.

   Copyright (C) 2010 IGEL Co., Ltd
   Copyright (C) 2010 Renesas Solutions Corp.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301 USA

*/
#ifndef __SIMPLE_AVCENC_H_
#define __SIMPLE_AVCENC_H_
#include <pthread.h>
#include <bellagio/tsemaphore.h>
#include "mciph.h"
#include "mcvenc.h"
#include "uiomux/uiomux.h"
#include "avcenc.h"
#include "shvpu5_driver.h"
#include "shvpu5_common_uio.h"

#define SHVPU_AVCENC_OUTBUF_SIZE (1280 * 720)
#define SHVPU_AVCENC_OUTBUF_NUM 3

#if defined(VPU_VERSION_5)
#define SPS_FLAG_COUNT 4
#elif defined(VPU5HA_SERIES)
#define SPS_FLAG_COUNT AVCENC_CNST_NOEL
#endif

typedef enum {
	SHVPU_BUFFER_STATUS_NONE = 0,
	SHVPU_BUFFER_STATUS_READY,
	SHVPU_BUFFER_STATUS_SET,
	SHVPU_BUFFER_STATUS_FILL
} shvpu_buffer_status_t;

typedef enum {
	SHVPU_PICTURE_TYPE_UNKNOWN = 0,
	SHVPU_PICTURE_TYPE_I,
	SHVPU_PICTURE_TYPE_P,
	SHVPU_PICTURE_TYPE_B
} shvpu_picture_type_t;

typedef struct {
	MCVENC_STRM_BUFF_INFO_T bufferInfo;
	shvpu_buffer_status_t	status;
	int			frameId;
	shvpu_picture_type_t	picType;
} shvpu_avcenc_outbuf_t;

typedef struct {
	MCVENC_WORK_INFO_T	wbuf_enc;
	MCVENC_IMD_INFO_T	imd_info;
	MCVENC_LDEC_INFO_T	ldec_info;
	MCVENC_IR_INFO_T	ir_info;
	MCVENC_MV_INFO_T	mv_info;
	MCVENC_FIRMWARE_INFO_T	fw;
	size_t  		mv_info_size;
	size_t			ce_fw_size;
	size_t			vlc_fw_size;
} shvpu_work_memory_t;

typedef struct {
	int			meta_input_buffers;
} shvpu_mode_settings_t;

typedef struct {
	unsigned long		paddr;
} shvpu_metabuffer_t;

typedef struct {
	shvpu_driver_t		*pDriver;

	/* only for encode */
	void*			pContext;
	int			nEncoded;
	shvpu_avcenc_outbuf_t	streamBuffer[SHVPU_AVCENC_OUTBUF_NUM];
        MCVENC_CMN_PROPERTY_T	cmnProp;
        AVCENC_OPTION_T		avcOpt;
	unsigned long		avcOptSet;
	long			sps_constraint_flags[SPS_FLAG_COUNT];
	shvpu_work_memory_t 	cmnWorkMem;
	shvpu_mode_settings_t	modeSettings;
} shvpu_avcenc_codec_t;

int logd(const char *format, ...);
int loge(const char *format, ...);

shvpu_avcenc_codec_t *
encode_new();
long
encode_init(shvpu_avcenc_codec_t *pCodec);
int
encode_set_profile(shvpu_avcenc_codec_t *pCodec, int profile_id);
int
encode_set_level(shvpu_avcenc_codec_t *pCodec, int level_id, int is1b);
int
encode_set_options(shvpu_avcenc_codec_t *pCodec, int num_ref_frames,
		   int max_GOP_length, int num_b_frames,
		   int isCABAC, int cabac_init_idc);
int
encode_set_bitrate(shvpu_avcenc_codec_t *pCodec, int bitrate, char mode);
int
encode_set_propaties(shvpu_avcenc_codec_t *pCodec, int width, int height,
		     unsigned int framerate, int bitrate, char ratecontrol);
int
encode_header(void *context, unsigned char *pBuffer, size_t nBufferLen);
int
encode_main(MCVENC_CONTEXT_T *pContext, int frameId,
	    unsigned char *pBuffer, int nWidth, int nHeight,
	    void **ppConsumed, int metabuffers);
int
encode_endcode(void *context, unsigned char *pBuffer, size_t nBufferLen);

int
encode_setqmatrix(MCVENC_CONTEXT_T *pContext);

void
encode_deinit(shvpu_avcenc_codec_t *pCodec);

int
encode_finalize(void *context);
#endif /* __SIMPLE_AVCENC_H_ */
