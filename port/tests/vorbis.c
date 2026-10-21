#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define STB_VORBIS_NO_PUSHDATA_API
#include "stb_vorbis.c"
static unsigned long decode(stb_vorbis *v, unsigned long *hash)
{
    short pcm[8192];
    int n, any = 0;
    unsigned long frames = 0;
    *hash = 2166136261UL;
    while ((n = stb_vorbis_get_samples_short_interleaved(v, 2, pcm, 8192)) > 0) {
        frames += n;
        for (int k = 0; k < n * 2; k++) {
            any |= pcm[k] != 0;
            *hash = (*hash ^ (unsigned short)pcm[k]) * 16777619UL;
        }
    }
    assert(any);
    assert(stb_vorbis_seek_start(v));
    assert(stb_vorbis_get_samples_short_interleaved(v, 2, pcm, 8192) > 0);
    return frames;
}
int main(int argc, char **argv)
{
    unsigned long total = 0;
    for (int i = 1; i < argc; i++) {
        int error = 0;
        stb_vorbis *v = stb_vorbis_open_filename(argv[i], &error, 0);
        assert(v);
        unsigned long fileHash, memoryHash;
        unsigned long frames = decode(v, &fileHash);
        stb_vorbis_close(v);
        FILE *f = fopen(argv[i], "rb");
        assert(f);
        assert(!fseek(f, 0, SEEK_END));
        long bytes = ftell(f);
        assert(bytes > 0 && bytes <= 4 * 1024 * 1024);
        rewind(f);
        unsigned char *compressed = malloc(bytes);
        assert(compressed);
        assert(fread(compressed, 1, bytes, f) == (size_t)bytes);
        fclose(f);
        v = stb_vorbis_open_memory(compressed, (int)bytes, &error, 0);
        assert(v);
        const stb_vorbis_info info = stb_vorbis_get_info(v);
        assert(info.channels > 0 && info.sample_rate > 0);
        printf("ASSET compressed=%ld setup=%u setup_temp=%u decode_temp=%u rate=%u channels=%d %s\n",
               bytes, info.setup_memory_required, info.setup_temp_memory_required,
               info.temp_memory_required, info.sample_rate, info.channels, argv[i]);
        assert(decode(v, &memoryHash) == frames);
        assert(memoryHash == fileHash);
        stb_vorbis_close(v);
        free(compressed);
        total += frames;
    }
    printf("PASS: %d Ogg files decoded/rewound from files and memory with identical PCM (%lu frames)\n",
           argc - 1, total);
}
