#ifndef __WAV_FILE_H__
#define __WAV_FILE_H__

#include <stdio.h>
#include <unistd.h>

#include "riff.h"

struct wav_file
{
    FILE *stream;
    riff_chunk riff;
    riff_sub_chunk_header format_header;
    riff_sub_chunk_body_wave_format format;
    riff_sub_chunk_header data_header;
};

typedef struct wav_file wav_file;


int wav_open_write(char *file, wav_file *w);
int wav_open_read(char *file, wav_file *w);
int wav_close_write(wav_file *w);
int wav_close_read(wav_file *w);
void wav_truncate(wav_file *w, off_t size);

#endif

