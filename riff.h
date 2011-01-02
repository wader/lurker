#ifndef __RIFF_H__
#define __RIFF_H__

#include <stdio.h>
#include <stdint.h>

struct riff_chunk
{
    int8_t id[4];
    int32_t size;
    int8_t format[4];
};

typedef struct riff_chunk riff_chunk;

struct riff_sub_chunk_wave_format
{
    int8_t id[4];
    int32_t size;
    int16_t audio_format;
    int16_t num_channels;
    int32_t sample_rate;
    int32_t byte_rate;
    int16_t block_align;
    int16_t bits_per_sample;
};

typedef struct riff_sub_chunk_wave_format riff_sub_chunk_wave_format;

struct riff_sub_chunk_wave_data
{
    int8_t id[4];
    int32_t size;
};

typedef struct riff_sub_chunk_wave_data riff_sub_chunk_wave_data;


int riff_read_chunk(FILE *stream, riff_chunk *r);
int riff_read_sub_chunk_wave_format(FILE *stream, riff_sub_chunk_wave_format *r);
int riff_read_sub_chunk_wave_data(FILE *stream, riff_sub_chunk_wave_data *r);
int riff_read_wave_16(FILE *stream, int16_t *buffer, int length);
int riff_write_chunk(FILE *stream, riff_chunk *r);
int riff_write_sub_chunk_wave_format(FILE *stream, riff_sub_chunk_wave_format *r);
int riff_write_sub_chunk_wave_data(FILE *stream, riff_sub_chunk_wave_data *r);
int riff_write_wave_16(FILE *stream, int16_t *buffer, int length);

#endif

