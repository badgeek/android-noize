/* Copyright (C) 2008 The Android Open Source Project
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/ioctl.h>

#define AUDIO_IOCTL_MAGIC 'a'

#define AUDIO_START        _IOW(AUDIO_IOCTL_MAGIC, 0, unsigned)
#define AUDIO_STOP         _IOW(AUDIO_IOCTL_MAGIC, 1, unsigned)
#define AUDIO_FLUSH        _IOW(AUDIO_IOCTL_MAGIC, 2, unsigned)
#define AUDIO_GET_CONFIG   _IOR(AUDIO_IOCTL_MAGIC, 3, unsigned)
#define AUDIO_SET_CONFIG   _IOW(AUDIO_IOCTL_MAGIC, 4, unsigned)
#define AUDIO_GET_STATS    _IOR(AUDIO_IOCTL_MAGIC, 5, unsigned)

struct msm_audio_config {
    uint32_t buffer_size;
    uint32_t buffer_count;
    uint32_t channel_count;
    uint32_t sample_rate;
    uint32_t codec_type;
    uint32_t unused[3];
};

struct msm_audio_stats {
    uint32_t out_bytes;
    uint32_t unused[3];
};
    
/* http://ccrma.stanford.edu/courses/422/projects/WaveFormat/ */

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1

struct wav_header {
	uint32_t riff_id;
	uint32_t riff_sz;
	uint32_t riff_fmt;
	uint32_t fmt_id;
	uint32_t fmt_sz;
	uint16_t audio_format;
	uint16_t num_channels;
	uint32_t sample_rate;
	uint32_t byte_rate;       /* sample_rate * num_channels * bps / 8 */
	uint16_t block_align;     /* num_channels * bps / 8 */
	uint16_t bits_per_sample;
	uint32_t data_id;
	uint32_t data_sz;
};


static char *next;
static unsigned avail;

int fill_buffer(void *buf, unsigned sz, void *cookie)
{
    char mychar[2];
    char mybuf[sz];
    int t;
    int i;

    i = 0;

    for(t=0+avail;t<avail+sz;t++)
    {
        //sprintf(&mybuf[i], "%c", t * ((t>>12|t>>8)&63&t>>4));
        sprintf(&mybuf[i], "%c",t * ((t>>2|t>>4)&90&t>>5&t>>9&t>>3&t>>5)  );
        i++;    
    } 

    memcpy(buf, mybuf, sz);
    
    avail += sz;

    return 0;
}

int pcm_play(unsigned rate, unsigned channels,
             int (*fill)(void *buf, unsigned sz, void *cookie),
             void *cookie)
{
    struct msm_audio_config config;
    struct msm_audio_stats stats;
    unsigned sz, n;
    char buf[8192];
    int afd;
    
    afd = open("/dev/msm_pcm_out", O_RDWR);
    
    if (afd < 0) {
        perror("pcm_play: cannot open audio device");
        return -1;
    }

    if(ioctl(afd, AUDIO_GET_CONFIG, &config)) {
        perror("could not get config");
        return -1;
    }

    config.channel_count = channels;
    config.sample_rate = rate;
    config.buffer_size = 2400;
    
    if (ioctl(afd, AUDIO_SET_CONFIG, &config)) {
        perror("could not set config");
        return -1;
    }

    sz = config.buffer_size;

    fprintf(stderr,"+--- prefill  ---+\n");
    fprintf(stderr,"+--- buffer_size = %i  ---+\n", config.buffer_size);
    fprintf(stderr,"+--- buffer_count = %i ---+\n", config.buffer_count);

    for (n = 0; n < config.buffer_count; n++) {
        if (fill(buf, sz, next)) break;
        if (write(afd, buf, sz) != sz) break;
    }

    fprintf(stderr,"+--- start-noize ---+\n");

    ioctl(afd, AUDIO_START, 0);

    for (;;) {
        if (fill(buf, sz, next)) break;
        if (write(afd, buf, sz) != sz) break;
    }

done:
    close(afd);
    return 0;
}

int start_sound(unsigned rate, unsigned channels)
{
    fprintf(stderr, "+--- init available: %i  ---+\n", avail);
    fprintf(stderr, "+--- starting pcm_play ---+\n");
    pcm_play(rate, channels, fill_buffer, 0);
    return 0;
}

int main(int argc, char **argv)
{
    avail = 0;
    fprintf(stderr, "+--- android-noize 0.1  ---+\n");
    fprintf(stderr, "+--- http://manticore.deadmediafm.org  ---+\n");
    return start_sound(atoi(argv[1]),1);
}


