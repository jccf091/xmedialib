/*
 * Copyright (c) 2008-2012 Peter Lemenkov <lemenkov@gmail.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * * Neither the name of the authors nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "erl_driver.h"
#include <opus.h>

typedef struct {
	OpusEncoder *encoder;
	OpusDecoder *decoder;
	int sampling_rate;
	int number_of_channels;
	ErlDrvPort port;
} codec_data;

enum {
	CMD_SETUP = 0,
	CMD_ENCODE = 1,
	CMD_DECODE = 2
};

static ErlDrvData codec_drv_start(ErlDrvPort port, char *buff)
{
	codec_data* d = (codec_data*)driver_alloc(sizeof(codec_data));
	d->encoder = NULL;
	d->decoder = NULL;
	d->sampling_rate = 0;
	d->number_of_channels = 0;

	d->port = port;
	set_port_control_flags(port, PORT_CONTROL_FLAG_BINARY);

	return (ErlDrvData)d;
}

static void codec_drv_stop(ErlDrvData handle)
{
	codec_data *d = (codec_data *) handle;
	if (d->decoder)
		opus_decoder_destroy(d->decoder);
	if (d->encoder)
		opus_encoder_destroy(d->encoder);
	driver_free((char*)handle);
}

static ErlDrvSSizeT codec_drv_control(
		ErlDrvData handle,
		unsigned int command,
		char *buf, ErlDrvSizeT len,
		char **rbuf, ErlDrvSizeT rlen)
{
#define MAX_PACKET 1500
#define STEREO 2

	codec_data* d = (codec_data*)handle;

	int ret = 0;
	ErlDrvBinary *out;
	*rbuf = NULL;
	opus_int16 pcm[960*6*STEREO];
	unsigned char opus[MAX_PACKET];

	// required for the SETUP
	int err = 0;

	switch(command) {
		case CMD_ENCODE:
			ret = opus_encode(d->encoder, (const opus_int16 *)buf, len >> 1, opus, MAX_PACKET);
			out = driver_alloc_binary(ret);
			memcpy(out->orig_bytes, opus, ret);
			*rbuf = (char *) out;
			break;
		 case CMD_DECODE:
			ret = d->number_of_channels * 2 * opus_decode(d->decoder, (const unsigned char *)buf, len, pcm, 960*6*STEREO, 0);
			out = driver_alloc_binary(ret);
			memcpy(out->orig_bytes, pcm, ret);
			*rbuf = (char *) out;
			break;
		case CMD_SETUP:
			d->sampling_rate = ((uint32_t*)buf)[0];
			d->number_of_channels = ((uint32_t*)buf)[1];

			/* come up with a way to specify these */
//			int bitrate_bps = bits_per_second;
//			int mode = MODE_HYBRID;
//			int use_vbr = 1;
//			int complexity = 10;
//			int use_inbandfec = 1;
//			int use_dtx = 1;
//			int bandwidth = BANDWIDTH_FULLBAND;

			if (!d->encoder && !d->decoder){
				d->encoder = opus_encoder_create(d->sampling_rate, d->number_of_channels, OPUS_APPLICATION_VOIP, &err);
				d->decoder = opus_decoder_create(d->sampling_rate, d->number_of_channels, &err);
			}
//			opus_encoder_ctl(d->encoder, OPUS_SET_MODE(mode));
//			opus_encoder_ctl(d->encoder, OPUS_SET_BITRATE(bitrate_bps));
//			opus_encoder_ctl(d->encoder, OPUS_SET_BANDWIDTH(bandwidth));
//			opus_encoder_ctl(d->encoder, OPUS_SET_VBR_FLAG(use_vbr));
//			opus_encoder_ctl(d->encoder, OPUS_SET_COMPLEXITY(complexity));
//			opus_encoder_ctl(d->encoder, OPUS_SET_INBAND_FEC_FLAG(use_inbandfec));
//			opus_encoder_ctl(d->encoder, OPUS_SET_DTX_FLAG(use_dtx));
			break;
		 default:
			break;
	}
	return ret;
}

ErlDrvEntry codec_driver_entry = {
	NULL,			/* F_PTR init, N/A */
	codec_drv_start,	/* L_PTR start, called when port is opened */
	codec_drv_stop,		/* F_PTR stop, called when port is closed */
	NULL,			/* F_PTR output, called when erlang has sent */
	NULL,			/* F_PTR ready_input, called when input descriptor ready */
	NULL,			/* F_PTR ready_output, called when output descriptor ready */
	(char*) "opus_codec_drv",	/* char *driver_name, the argument to open_port */
	NULL,			/* F_PTR finish, called when unloaded */
	NULL,			/* handle */
	codec_drv_control,	/* F_PTR control, port_command callback */
	NULL,			/* F_PTR timeout, reserved */
	NULL,			/* F_PTR outputv, reserved */
	NULL,
	NULL,
	NULL,
	NULL,
	(int) ERL_DRV_EXTENDED_MARKER,
	(int) ERL_DRV_EXTENDED_MAJOR_VERSION,
	(int) ERL_DRV_EXTENDED_MINOR_VERSION,
	0,
	NULL,
	NULL,
	NULL
};

DRIVER_INIT(codec_drv) /* must match name in driver_entry */
{
	return &codec_driver_entry;
}
