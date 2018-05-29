/*
 * lurker, an audio silence splitter
 * Copyright (C)2004 Mattias Wadman <mattias.wadman@softdays.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>

#include "riff.h"


int riff_read_chunk(FILE *stream, riff_chunk *r)
{
    if(fread(r, sizeof(*r), 1, stream) != 1)
        return -1;

#if __BYTE_ORDER != __LITTLE_ENDIAN
    r->size = bswap_32(r->size);
#endif

    return 0;
}

int riff_read_sub_chunk(FILE *stream, riff_sub_chunk_header *r)
{
    if(fread(r, sizeof(*r), 1, stream) != 1)
        return -1;

    #if __BYTE_ORDER != __LITTLE_ENDIAN
        r->size = bswap_32(r->size);
    #endif

    return 0;
}

int riff_read_sub_chunk_body_wave_format(FILE *stream, riff_sub_chunk_body_wave_format *r)
{
    if(fread(r, sizeof(*r), 1, stream) != 1)
        return -1;

    #if __BYTE_ORDER != __LITTLE_ENDIAN
        r->audio_format = bswap_16(r->audio_format);
        r->num_channels = bswap_16(r->num_channels);
        r->sample_rate = bswap_32(r->sample_rate);
        r->byte_rate = bswap_32(r->byte_rate);
        r->block_align = bswap_16(r->block_align);
        r->bits_per_sample = bswap_16(r->bits_per_sample);
    #endif
    
    return 0;
}

int riff_read_wave_16(FILE *stream, int16_t *buffer, int length)
{
    int n;
#if __BYTE_ORDER != __LITTLE_ENDIAN
    int i;
#endif
    
    n = fread(buffer, sizeof(int16_t), length, stream);
    if(n < length && ferror(stream) != 0)
        return -1;

#if __BYTE_ORDER != __LITTLE_ENDIAN
    for(i = 0; i < n; i++)
        buffer[i] = bswap_16(buffer[i]);
#endif
    
    return n;
}

int riff_write_chunk(FILE *stream, riff_chunk *r)
{
#if __BYTE_ORDER != __LITTLE_ENDIAN
    r->size = bswap_32(r->size);
#endif

    if(fwrite(r, sizeof(*r), 1, stream) != 1)
        return -1;

#if __BYTE_ORDER != __LITTLE_ENDIAN
    r->size = bswap_32(r->size);
#endif

    return 0;
}

int riff_write_sub_chunk_header(FILE *stream, riff_sub_chunk_header *r)
{
#if __BYTE_ORDER != __LITTLE_ENDIAN
    r->size = bswap_32(r->size);
#endif

    if(fwrite(r, sizeof(*r), 1, stream) != 1)
        return -1;

#if __BYTE_ORDER != __LITTLE_ENDIAN
    r->size = bswap_32(r->size);
#endif

    return 0;
}

int riff_write_sub_chunk_body_wave_format(FILE *stream, riff_sub_chunk_body_wave_format *r)
{
    riff_sub_chunk_body_wave_format c = *r;
    
#if __BYTE_ORDER != __LITTLE_ENDIAN
    c.audio_format = bswap_16(c.audio_format);
    c.num_channels = bswap_16(c.num_channels);
    c.sample_rate = bswap_32(c.sample_rate);
    c.byte_rate = bswap_32(c.byte_rate);
    c.block_align = bswap_16(c.block_align);
    c.bits_per_sample = bswap_16(c.bits_per_sample);
#endif
    
    if(fwrite(&c, sizeof(c), 1, stream) != 1)
        return -1;
    
    return 0;
}

int riff_write_wave_16(FILE *stream, int16_t *buffer, int length)
{
#if __BYTE_ORDER != __LITTLE_ENDIAN
    int i;

    /* swap to LE */
    for(i = 0; i < length; i++)
        buffer[i] = bswap_16(buffer[i]);
#endif
    
    if(fwrite(buffer, sizeof(int16_t) *  length, 1, stream) != 1)
        return -1;
    
#if __BYTE_ORDER != __LITTLE_ENDIAN
    /* swap back */
    for(i = 0; i < length; i++)
        buffer[i] = bswap_16(buffer[i]);
#endif
    
    return 0;
}

