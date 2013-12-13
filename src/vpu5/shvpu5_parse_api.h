/**
   src/vpu5/shvpu5_parse_api.h

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

typedef struct {
	void*			base_addr;
	size_t			size;
	int 			n_sbufs;
	size_t			buf_sizes[16];
	void*			buf_offsets[16];
} phys_input_buf_t;

typedef	struct {
	phys_input_buf_t*	pBufs[16];
	int			n_bufs;
	int			n_sbufs;
	size_t			size;
	OMX_BOOL		hasSlice;
	buffer_avcdec_metainfo_t buffer_meta;
	int			has_meta;
} pic_t;

struct input_parse_ops {
	OMX_BOOL (*parseBuffer) (shvpu_decode_PrivateType *shvpu_decode_Private,
					    OMX_BUFFERHEADERTYPE *pBuffer,
					    OMX_BOOL		 eos,
					    pic_t		 *pActivePic,
					    OMX_BOOL 		 *pIsInBufferNeeded);
	void (*parserFlush) (shvpu_decode_PrivateType *shvpu_decode_Private);
	void (*parserDeinit) (shvpu_decode_PrivateType *shvpu_decode_Private);
	const unsigned char *EOSCode;
	size_t EOSCodeLen;
};

/* Parser initialization function headers */
int initAvcParser(shvpu_decode_PrivateType *priv);

#ifdef MPEG4_DECODER
int initMpegParser(shvpu_decode_PrivateType *priv);
#else
static inline int initMpegParser(shvpu_decode_PrivateType *priv) {
	return -1;
}
#endif
