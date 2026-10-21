/* Verify the public SDL semantics used by the opaque RGB565 fast path. */
#include <SDL.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    SDL_version version;SDL_GetVersion(&version);
    printf("SDL %d.%d.%d\n",version.major,version.minor,version.patch);
    for (int keyed=0;keyed<2;++keyed) {
        SDL_Surface *src=SDL_CreateRGBSurfaceWithFormat(0,3,1,16,SDL_PIXELFORMAT_RGB565);
        SDL_Surface *dst=SDL_CreateRGBSurfaceWithFormat(0,3,1,16,SDL_PIXELFORMAT_RGB565);
        if (!src || !dst) return 2;
        const Uint16 pixels[3]={0,0xf800,0xf81f};
        Uint16 blended[3],copied[3];
        memcpy(src->pixels,pixels,sizeof(pixels));
        if (SDL_SetSurfaceAlphaMod(src,255)<0 || SDL_SetColorKey(src,keyed,0xf81f)<0) return 2;
        SDL_FillRect(dst,NULL,31);
        if (SDL_SetSurfaceBlendMode(src,SDL_BLENDMODE_BLEND)<0 || SDL_BlitSurface(src,NULL,dst,NULL)<0) return 2;
        memcpy(blended,dst->pixels,sizeof(blended));
        SDL_FillRect(dst,NULL,31);
        if (SDL_SetSurfaceBlendMode(src,SDL_BLENDMODE_NONE)<0 || SDL_BlitSurface(src,NULL,dst,NULL)<0) return 2;
        memcpy(copied,dst->pixels,sizeof(copied));
        const int equal=memcmp(blended,copied,sizeof(copied))==0;
        const int expected=copied[0]==0 && copied[1]==0xf800 && copied[2]==(keyed?31:0xf81f);
        printf("keyed=%d opaque_blend_equals_copy=%d expected_pixels=%d\n",keyed,equal,expected);
        SDL_FreeSurface(src);SDL_FreeSurface(dst);
        if (!equal || !expected) return 1;
    }
    return 0;
}
