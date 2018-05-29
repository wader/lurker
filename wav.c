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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "riff.h"
#include "wav.h"


int wav_open_write(char *file, wav_file *w)
{
    /* fill in header constants */
    strncpy(w->riff.id, "RIFF", 4);
    strncpy(w->riff.format, "WAVE", 4);
    strncpy(w->format_header.id, "fmt ", 4);
    strncpy(w->data_header.id, "data", 4);

    /* PCM format header size */
    w->format_header.size = sizeof(w->format);
    
    w->stream = fopen(file, "w");
    if(w->stream == NULL)
    {
        fprintf(stderr, "wav_open_write: fopen for write failed\n");
        
        return -1;
    }

    if(riff_write_chunk(w->stream, &w->riff) == -1 ||
       riff_write_sub_chunk_header(w->stream, &w->format_header) == -1 ||
       riff_write_sub_chunk_body_wave_format(w->stream, &w->format) == -1 ||
       riff_write_sub_chunk_header(w->stream, &w->data_header) == -1)
    {
        fprintf(stderr, "wav_open_write: Failed to write header\n");
        fclose(w->stream);

        return -1;
    }

    return 0;
}

int wav_open_read(char *file, wav_file *w)
{
    if(file == NULL)
        w->stream = stdin;
    else
    {
        w->stream = fopen(file, "r");
        if(w->stream == NULL)
        {
            fprintf(stderr, "wav_open_read: fopen for read failed\n");
            
            return -1;
        }
    }

    if(riff_read_chunk(w->stream, &w->riff) == -1)
    {
        fprintf(stderr, "wave_open_read: Failed to read RIFF header\n");
        fclose(w->stream);

        return -1;
    }

    if(strncmp(w->riff.id, "RIFF", 4) != 0)
    {
        fprintf(stderr, "wave_open_read: Wrong RIFF id, not a RIFF file?\n");
        fclose(w->stream);

        return -1;
    }

    if(strncmp(w->riff.format, "WAVE", 4) != 0)
    {
        fprintf(stderr, "wav_open_read: RIFF format is not WAVE\n");
        fclose(w->stream);

        return -1;
    }

    for (;;) {
        riff_sub_chunk_header header;
        if(riff_read_sub_chunk(w->stream, &header) == -1)
        {
            fprintf(stderr, "wave_open_read: Failed to read RIFF sub chunk header\n");
            fclose(w->stream);

            return -1;
        }

        printf("'%.*s' %ld\n", 4, &header.id, header.size);

        if(strncmp(&header.id, "fmt ", 4) == 0)
        {
                    printf("fmt ASDASDSAD\n");


            if(riff_read_sub_chunk_body_wave_format(w->stream, &w->format) == -1)
            {
                fprintf(stderr, "wave_open_read: Failed to read RIFF fmt header\n");
                fclose(w->stream);

                return -1;
            }
        }
        else if(strncmp(header.id, "data", 4) == 0)
        {
            break;
        }
        else
        {
            // skip sub chunk
            char *b = malloc(header.size);
            fread(b, header.size, 1, w->stream);
            free(b);
        }
    }

    return 0;
}

int wav_close_write(wav_file *w)
{
    /* seek to end of file */
    if(fseek(w->stream, 0, SEEK_END) == -1)
    {
        fprintf(stderr, "wav_close_write: fseek end failed\n");

        return -1;
    }
    
    w->riff.size = ftell(w->stream) - 8; /* total size - parts of riff header */
    w->data_header.size = ftell(w->stream) - (
        sizeof(w->riff) +
        sizeof(w->format_header) +
        sizeof(w->format) +
        sizeof(w->data_header)
    );
    
    /* seek to beginning of file */
    if(fseek(w->stream, 0, SEEK_SET) == -1)
    {
        fprintf(stderr, "wav_close_write: fseek beginning failed\n");

        return -1;
    }
    
    /* rewrite header */
    if(riff_write_chunk(w->stream, &w->riff) == -1 ||
       riff_write_sub_chunk_header(w->stream, &w->format_header) == -1 ||
       riff_write_sub_chunk_body_wave_format(w->stream, &w->format) == -1 ||
       riff_write_sub_chunk_header(w->stream, &w->data_header) == -1)
    {
        fprintf(stderr, "wav_close_write: Failed to rewrite header\n");

        return -1;
    }

    fclose(w->stream);

    return 0;
}

int wav_close_read(wav_file *w)
{
    fclose(w->stream);

    return 0;
}

void wav_truncate(wav_file *w, off_t size)
{
    fflush(w->stream);
    ftruncate(fileno(w->stream),
              sizeof(riff_chunk) +
              sizeof(riff_sub_chunk_header) +
              sizeof(riff_sub_chunk_body_wave_format) +
              sizeof(riff_sub_chunk_header) +
              /* align size to whole blocks */
              size - (size % w->format.block_align)
              );
}

