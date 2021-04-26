#ifndef __LOCATE_H__
#define __LOCATE_H__

#define DEFAULT_IMAGE_ALIGN 0x20ULL

UINT64 get_rip();
extern UINT64 OriginalEntryPoint_offset;

void* get_image_base();
void reloc_image(void* image, void* new_image_base);

struct OEP_t {
    UINT64 oep;
    UINT64 ep;
};

#endif
