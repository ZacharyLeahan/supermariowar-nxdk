/* Standalone SDL2 regression: an unkeyed RGB565 black pixel must be opaque.
 * No video/audio initialization is needed. Run against a selected SDL library;
 * host results are separate from Xbox observations. */
#include <SDL.h>
#include <stdio.h>
static int check(int rle) {
    SDL_Surface *src=SDL_CreateRGBSurfaceWithFormat(0, 2, 1, 16, SDL_PIXELFORMAT_RGB565);
    SDL_Surface *dst=SDL_CreateRGBSurfaceWithFormat(0, 2, 1, 16, SDL_PIXELFORMAT_RGB565);
    if (!src || !dst) return -1;
    SDL_FillRect(src, NULL, 0);
    SDL_FillRect(dst, NULL, SDL_MapRGB(dst->format, 0, 0, 255));
    if (SDL_SetSurfaceBlendMode(src,SDL_BLENDMODE_BLEND)<0 ||
        SDL_SetSurfaceAlphaMod(src,255)<0 || SDL_SetSurfaceRLE(src,rle)<0 ||
        SDL_BlitSurface(src,NULL,dst,NULL)<0) return -1;
    Uint16 first=*(Uint16*)dst->pixels;
    printf("rle=%d expected_black=0 actual=%u opaque=%d\n",rle,(unsigned)first,first==0);
    SDL_FreeSurface(src);SDL_FreeSurface(dst);
    return first==0;
}
int main(void) {
    SDL_version v;SDL_GetVersion(&v);printf("SDL %d.%d.%d\n",v.major,v.minor,v.patch);
    const int plain=check(0), accelerated=check(1);
    if (plain!=1 || accelerated<0) {fprintf(stderr,"setup/plain failure: %s\n",SDL_GetError());return 2;}
    return accelerated==1 ? 0 : 1;
}
