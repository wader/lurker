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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <libgen.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <getopt.h>
#include <signal.h>
#include <curses.h>
#include <term.h>
#include <limits.h>

#include "riff.h"
#include "wav.h"


char *current_dir;
char *input;
char *output;
char *recording_append;
double threshold;
double runlength;
double short_filter;
time_t time_start;
double slice_divisor;

int terminate_signal;
char *clear_line;


/* mkdir, full path edition */
int mkdirp(char *path)
{
    int result = 0;
    char *d, *s;
    
    s = strdup(path);
    if(s == NULL)
        return -1;

    d = dirname(s);
    if(strcmp(d, ".") == 0 || strcmp(d, "/") == 0)
        result = 0;
    else
    {
        result = mkdirp(d);
        if(result == 0)
        {
            /* ok if mkdir succeeds or directory already exists */
            if(mkdir(path, 0777) == 0 || errno == EEXIST)
                result = 0;
            else
                result = -1;
        }
    }
    
    free(s);
    
    return result;
}

/* discrete root mean square algorithm */
/* http://en.wikipedia.org/wiki/Root_mean_square */
double root_mean_square(int16_t *buffer, int length)
{
    int i;
    double sum = 0.0;

    for(i = 0; i < length; i++)
        sum += (buffer[i] * buffer[i]) / length;

    return sqrt(sum) / INT16_MAX;
}

/* use terminfo database to generate a string that clears current line and move
 * cursor to left side of screen */
char *generate_clear_line_string()
{
    char *s, *m, *c;
    
    /* init terminal type */
    if(setupterm(NULL, STDOUT_FILENO, NULL) == ERR)
        return NULL;
    /* carriage return, cursor to left of screen */
    m = tigetstr("cr");
    if(m == NULL)
        return NULL;
    /* clear from cursor to end of line */
    c = tigetstr("el");
    if(c == NULL)
        return NULL;
    /* concatinate */
    if(asprintf(&s, "%s%s", m, c) == -1)
        return NULL;

    return s;
}

void message(char *format, ...)
{
    char *s;
    char b[256];
    va_list args;
    time_t t;
   
    t = time(NULL);
    strftime(b, sizeof(b), "%H:%M:%S", localtime(&t));
    
    va_start(args, format);
    if(vasprintf(&s, format, args) == -1)
        s = NULL;
    va_end(args);

    if(s == NULL)
        fprintf(stderr, "message: vasprintf failed\n");
    else
    {
        printf("%s%s %s", clear_line, b, s);
	free(s);
    }
}

void signal_handler(int number)
{
    terminate_signal = 1;
}

int lurk()
{
    wav_file in, out;
    int quit;
    int16_t *buffer;
    int buffer_length, buffer_bytes, read_length;
    uint64_t total_length, cut_length, peak_length;
    int recording;
    char *output_path, *output_temp_path;
    double rms;
    const char progress[] = {'|', '/', '-', '\\'};
    uint64_t progress_temp, progress_position;

    quit = 0;
    terminate_signal = 0;
    output_path = NULL;
    output_temp_path = NULL;
    progress_position = 0;
    
    printf("Reading header from %s\n", (input == NULL ? "stdin" : input));

    if(wav_open_read(input, &in) == -1)
    {
        fprintf(stderr, "Failed to open %s for input\n", input);

        return -1;
    }
    
    if(!(in.format.audio_format == 1 && /* 1 = PCM */
         in.format.bits_per_sample == 16 &&
         in.format.num_channels == 1))
    {
        fprintf(stderr, "Wrong audio format, i want 16 bit mono PCM audio\n");

        return -1;
    }
    
    printf("Output: %s\n", output);
    printf("Recording append: %s\n", recording_append);
    printf("Threshold: %g\n", threshold);
    printf("Runlength: %g seconds\n", runlength);
    if(short_filter != 0)
        printf("Short filter: %g seconds\n", short_filter);
   
    buffer_length = in.format.sample_rate / slice_divisor;
    buffer_bytes = buffer_length * in.format.block_align;
    buffer = malloc(buffer_bytes);
    if(buffer == NULL)
    {
        fprintf(stderr, "lurk: malloc audio buffer failed\n");

        return -1;
    }
    total_length = 0;
    cut_length = 0;
    peak_length = 0;
    recording = 0;

    if(time_start != 0)
    {
        char s[256];
        
        strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", localtime(&time_start));
        printf("Start time: %s\n", s);
    }
    printf("Sample rate: %d Hz\n", in.format.sample_rate);
    printf("Slice divisor: %g\n", slice_divisor);
    printf("\n");
    printf("Starting to lurk...\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    while(quit == 0)
    {
        read_length = riff_read_wave_16(in.stream, buffer, buffer_length);
        if(read_length < 1)
        {
            if(read_length == 0)
            {
                if(recording == 1)
                    quit = 1; /* run loop one last time */
                else
                    break;
            }
            else
            {
                fprintf(stderr, "Error reading input file\n");

                if(recording == 1)
                {
                    message("Trying to close output file nicely\n");
                    wav_close_write(&out);
                    rename(output_temp_path, output_path);
                }

                break;
            }
        }

        if(terminate_signal == 1)
            quit = 1;
        
        rms = root_mean_square(buffer, read_length);
        total_length += read_length;
        
        if(recording == 1)
        {
            if((double)peak_length / in.format.sample_rate > runlength || quit == 1)
            {
                /* stop recording to file */
                recording = 0;

                /* remove runlength seconds of silence at end of recording */
                cut_length -= runlength * in.format.sample_rate;
                /* adjust file */
                wav_truncate(&out, cut_length * out.format.block_align);
                
                if(wav_close_write(&out) == -1)
                    fprintf(stderr, "lurk: failed to close output file %s\n", output_temp_path);
                
                if(short_filter != 0 &&
                   (double)cut_length / in.format.sample_rate < short_filter)
                {
                    if(unlink(output_temp_path) == -1)
                        fprintf(stderr, "lurk: faild to unlink %s\n", output_temp_path);
                    
                    message("Recording removed, short filter\n");
                }
                else
                {
                    if(rename(output_temp_path, output_path) == -1)
                        fprintf(stderr, "lurk: faild to rename %s to %s\n", output_temp_path, output_path);
                    
                    message("Recording stopped, %d minutes %d seconds recorded\n",
                            (int)(cut_length / in.format.sample_rate) / 60,
                            /* (int)(ceil(cut_length / in.format.sample_rate)) % 60 */
                            (int)(cut_length / in.format.sample_rate) % 60
                            );
                }
                
                free(output_path);
                free(output_temp_path);
                output_path = NULL;
                output_temp_path = NULL;
                cut_length = 0;
                peak_length = 0;
            }
            else
            {
                cut_length += read_length;
                peak_length += read_length;

                if(rms > threshold)
                    peak_length = 0;
            }
        }
        else
        {
            if(rms > threshold)
            {
                time_t t;
                char *s, *d;
                char expanded[PATH_MAX];
                
                /* start recording to file */
                recording = 1;

                /* fancy print output path (non-absolute path etc) */
                if(time_start != 0)
                    t = time_start + total_length / in.format.sample_rate;
                else
                    time(&t);
                strftime(expanded, sizeof(expanded), output, localtime(&t));
                message("Recording started to %s\n", expanded);

                if(asprintf(&output_path, "%s%s",
                            (output[0] == '/' ? "" : current_dir), /* make absolute if relative */
                            expanded
                            ) == -1)
                {
                    fprintf(stderr, "lurk: asprintf failed: output_path\n");

                    return -1;
                }
                if(asprintf(&output_temp_path, "%s%s",
                            output_path,
                            recording_append
                            ) == -1)
                {
                    fprintf(stderr, "lurk: asprintf failed: output_temp_path\n");

                    return -1;
                }
                
                s = strdup(output_path);
                if(s == NULL)
                {
                    fprintf(stderr, "lurk: strdup failed: output_path\n");

                    return -1;
                }
                d = dirname(s);
                if(mkdirp(d) == -1)
                {
                    fprintf(stderr, "lurk: mkdirp failed: %s\n", d);

                    return -1;
                }
                free(s);
                
                out.riff.size = INT32_MAX; /* as big as possible, wav_close_write will fix them */
                out.data.size = INT32_MAX;
                out.format.audio_format = 1; /* PCM */
                out.format.num_channels = 1; /* mono */
                out.format.sample_rate = in.format.sample_rate;
                out.format.byte_rate = in.format.byte_rate;
                out.format.block_align = in.format.block_align;
                out.format.bits_per_sample = in.format.bits_per_sample;

                if(wav_open_write(output_temp_path, &out) == -1)
                {
                    fprintf(stderr, "lurk: failed to open temp output file %s\n", output_temp_path);

                    return -1;
                }
            }
        }
/*
        progress_temp = total_length / in.format.sample_rate;
        if(progress_temp > progress_position)
        {
            progress_position = progress_temp;
            message("%s %c",
                   (recording == 1 ? "Recording" : "Lurking"),
                   progress[progress_position % sizeof(progress)]
                   );

            fflush(stdout);
        }
*/        

        /* bloated fancy status featuring cut length, volume-meter and more! */
        {
            int p, l;
            char b[21];
            
            progress_position = total_length / in.format.sample_rate;
            
            l = sizeof(b) * rms;
            for(p = 0; p < sizeof(b) - 1; p++)
                b[p] = (p < l ? '=' : ' ');
            b[sizeof(b) - 1] = '\0';
            
            message("%s %c [t:%.1f c:%.1f p:%.1f] [%s]",
                   (recording == 1 ? "Recording" : "Lurking"),
                   progress[progress_position % sizeof(progress)],
                   (double)total_length / in.format.sample_rate,
                   (double)cut_length / in.format.sample_rate,
                   (double)peak_length / in.format.sample_rate,
                   b
                   );
            
            fflush(stdout);
        }
        
        if(recording == 1)
        {
            /* write audio to file */
            if(riff_write_wave_16(out.stream, buffer, read_length) == -1)
            {
                fprintf(stderr, "lurk: riff_write_wave_16 failed\n");

                return -1;
            }
        }
    }

    wav_close_read(&in);
    free(buffer);
    if(output_path != NULL)
        free(output_path);
    if(output_temp_path != NULL)
        free(output_temp_path);
    
    message("Stopped\n");

    return 0;
}

int main(int argc, char **argv)
{
    int r;
    int option;
    struct option getopt_options[] =
    {
        {"help", 0, 0, 'h'},
        {"input", 1, 0, 'i'},
        {"output", 1, 0, 'o'},
        {"append", 1, 0, 'a'},
        {"threshold", 1, 0, 't'},
        {"runlength", 1, 0, 'r'},
        {"filter", 1, 0, 'f'},
        {"start", 1, 0, 's'},
        {"divisor", 1, 0, 'd'},
        {NULL, 0, 0, 0}
    };

    /* defaults */
    input = NULL; /* stdin */
    output = "clip_%F_%H:%M:%S.wav";
    /* use .wav to be nice to people who like to listen while recording */
    recording_append = ".recording.wav";
    threshold = 0.1;
    runlength = 4;
    short_filter = 0; /* dont filter */
    time_start = 0; /* 0 = use system time */
    slice_divisor = 60;

    /* shameless plug */
    printf("lurker 0.4, (C)2004 Mattias Wadman <mattias.wadman@softdays.se>\n");

    while(1)
    {
        option = getopt_long(argc, argv, "hi:o:a:t:r:f:s:d:", getopt_options, NULL);

        if(option == -1)
            break;
        else if(option == 'h')
        {
            printf("Usage: %s [OPTION]...\n"
                   "    -i, --input PATH       Input file (stdin)\n"
                   "    -o, --output PATH      Output path, strftime formated (%s)\n"
                   "    -a, --append STRING    Append to filename while recording (%s)\n"
                   "    -t, --threshold NUMBER Sound level threshold to trigger (%g)\n"
                   "    -r, --runlength NUMBER Seconds of silence to untrigger (%g)\n"
                   "    -f, --filter NUMBER    Remove clip if length is less then NUMBER seconds (%g)\n"
                   "    -s, --start DATETIME   Use a given start time and offset with audio time\n"
                   "                           Eg: \"2000-01-02 03:04:05\"\n"
                   "                           Eg: now (use system clock as start)\n"
                   "    -d, --divisor NUMBER   Slice sample rate into NUMBER parts internally (%g)\n"
                   "",
                   argv[0], output, recording_append, threshold, runlength,
                   short_filter, slice_divisor
                   );

            return EXIT_SUCCESS;
        }
        else if(option == 'i')
            input = optarg;
        else if(option == 'o')
            output = optarg;
        else if(option == 'a')
            recording_append = optarg;
        else if(option == 't')
            threshold = atof(optarg);
        else if(option == 'r')
            runlength = atof(optarg);
        else if(option == 'f')
            short_filter = atof(optarg);
        else if(option == 's')
        {
            struct tm t;
            char *s;

            if(strcmp(optarg, "now") == 0)
                time_start = time(NULL);
            else
            {
                s = strptime(optarg, "%Y-%m-%d %H:%M:%S", &t);
                if(s == NULL || *s != '\0')
                {
                    fprintf(stderr, "Invalid time format\n");

                    return EXIT_FAILURE;
                }
                
                time_start = mktime(&t);
            }
        }
        else if(option == 'd')
            slice_divisor = atof(optarg);
        else
        {
            fprintf(stderr, "Error in argument: %c\n", option);

            return EXIT_FAILURE;
        }
    }
   
    /* string used to clear current line */
    clear_line = generate_clear_line_string();
    if(clear_line == NULL)
    {
        fprintf(stderr, "generate_clear_line_string failed, fallback to \"\\r\"\n");
        clear_line = strdup("\r");
    }
   
    /* get current working path and make sure it end with a slash */
    current_dir = get_current_dir_name();
    if(current_dir == NULL)
    {
        fprintf(stderr, "get_current_dir_name failed\n");

        return EXIT_FAILURE;
    }
    /* it is possible that it already ends with a slash, see man page about $PWD */
    else if(current_dir[strlen(current_dir) - 1] != '/')
    {
        char *t;

        if(asprintf(&t, "%s/", current_dir) == -1)
        {
            fprintf(stderr, "asprintf failed\n");

            return EXIT_FAILURE;
        }

        free(current_dir);
        current_dir = t;
    }

    r = lurk();
    free(current_dir);
    free(clear_line);

    return (r == -1 ? EXIT_FAILURE : EXIT_SUCCESS);
}

