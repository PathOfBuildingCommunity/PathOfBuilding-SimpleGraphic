/******************************************************************************
 
gif_lib.h - service library for decoding and encoding GIF images
                                                                             
*****************************************************************************/

#ifndef _GIF_LIB_H_
#define _GIF_LIB_H_ 1





#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GIFLIB_MAJOR 5
#define GIFLIB_MINOR 1
#define GIFLIB_RELEASE 2

#define GIF_ERROR   0
#define GIF_OK      1

#include <stddef.h>

#include <stdbool.h>
#include <inttypes.h>
#define MODULE_API
//#include "winhlpr.h"



#define GIF_STAMP "GIFVER"          /* First chars in file - GIF stamp.  */
#define GIF_STAMP_LEN sizeof(GIF_STAMP) - 1
#define GIF_VERSION_POS 3           /* Version first character in stamp. */
#define GIF87_STAMP "GIF87a"        /* First chars in file - GIF stamp.  */
#define GIF89_STAMP "GIF89a"        /* First chars in file - GIF stamp.  */

typedef unsigned char GifPixelType;
typedef unsigned char *GifRowType;
typedef unsigned char GifByteType;
typedef unsigned int GifPrefixType;
typedef int GifWord;

typedef struct GifColorType {
    GifByteType Red, Green, Blue;
} GifColorType;

typedef struct ColorMapObject {
    int ColorCount;
    int BitsPerPixel;
    bool SortFlag;
    GifColorType *Colors;    /* on malloc(3) heap */
} ColorMapObject;

typedef struct GifImageDesc {
    GifWord Left, Top, Width, Height;   /* Current image dimensions. */
    bool Interlace;                     /* Sequential/Interlaced lines. */
    ColorMapObject *ColorMap;           /* The local color map */
} GifImageDesc;

typedef struct ExtensionBlock {
    int ByteCount;
    GifByteType *Bytes; /* on malloc(3) heap */
    int Function;       /* The block function code */
#define CONTINUE_EXT_FUNC_CODE    0x00    /* continuation subblock */
#define COMMENT_EXT_FUNC_CODE     0xfe    /* comment */
#define GRAPHICS_EXT_FUNC_CODE    0xf9    /* graphics control (GIF89) */
#define PLAINTEXT_EXT_FUNC_CODE   0x01    /* plaintext */
#define APPLICATION_EXT_FUNC_CODE 0xff    /* application block */
} ExtensionBlock;

typedef struct SavedImage {
    GifImageDesc ImageDesc;
    GifByteType *RasterBits;         /* on malloc(3) heap */
    int ExtensionBlockCount;         /* Count of extensions before image */    
    ExtensionBlock *ExtensionBlocks; /* Extensions before image */    
} SavedImage;

typedef struct GifFileType {
    GifWord SWidth, SHeight;         /* Size of virtual canvas */
    GifWord SColorResolution;        /* How many colors can we generate? */
    GifWord SBackGroundColor;        /* Background color for virtual canvas */
    GifByteType AspectByte;	     /* Used to compute pixel aspect ratio */
    ColorMapObject *SColorMap;       /* Global colormap, NULL if nonexistent. */
    int ImageCount;                  /* Number of current image (both APIs) */
    GifImageDesc Image;              /* Current image (low-level API) */
    SavedImage *SavedImages;         /* Image sequence (high-level API) */
    int ExtensionBlockCount;         /* Count extensions past last image */
    ExtensionBlock *ExtensionBlocks; /* Extensions past last image */    
    int Error;			     /* Last error condition reported */
    void *UserData;                  /* hook to attach user data (TVT) */
    void *Private;                   /* Don't mess with this! */
} GifFileType;

#define GIF_ASPECT_RATIO(n)	((n)+15.0/64.0)

typedef enum {
    UNDEFINED_RECORD_TYPE,
    SCREEN_DESC_RECORD_TYPE,
    IMAGE_DESC_RECORD_TYPE, /* Begin with ',' */
    EXTENSION_RECORD_TYPE,  /* Begin with '!' */
    TERMINATE_RECORD_TYPE   /* Begin with ';' */
} GifRecordType;

/* func type to read gif data from arbitrary sources (TVT) */
typedef int (*InputFunc) (GifFileType *, GifByteType *, int);

/* func type to write gif data to arbitrary targets.
 * Returns count of bytes written. (MRB)
 */
typedef int (*OutputFunc) (GifFileType *, const GifByteType *, int);

/******************************************************************************
 GIF89 structures
******************************************************************************/

typedef struct GraphicsControlBlock {
    int DisposalMode;
#define DISPOSAL_UNSPECIFIED      0       /* No disposal specified. */
#define DISPOSE_DO_NOT            1       /* Leave image in place */
#define DISPOSE_BACKGROUND        2       /* Set area too background color */
#define DISPOSE_PREVIOUS          3       /* Restore to previous content */
    bool UserInputFlag;      /* User confirmation required before disposal */
    int DelayTime;           /* pre-display delay in 0.01sec units */
    int TransparentColor;    /* Palette index for transparency, -1 if none */
#define NO_TRANSPARENT_COLOR	-1
} GraphicsControlBlock;

/******************************************************************************
 GIF encoding routines
******************************************************************************/

/* Main entry points */
MODULE_API GifFileType *EGifOpenFileName(const char *GifFileName,
                              const bool GifTestExistence, int *Error);
MODULE_API GifFileType *EGifOpenFileHandle(const int GifFileHandle, int *Error);
MODULE_API GifFileType *EGifOpen(void *userPtr, OutputFunc writeFunc, int *Error);
MODULE_API int EGifSpew(GifFileType * GifFile);
MODULE_API const char *EGifGetGifVersion(GifFileType *GifFile); /* new in 5.x */
MODULE_API int EGifCloseFile(GifFileType *GifFile, int *ErrorCode);

#define E_GIF_SUCCEEDED          0
#define E_GIF_ERR_OPEN_FAILED    1    /* And EGif possible errors. */
#define E_GIF_ERR_WRITE_FAILED   2
#define E_GIF_ERR_HAS_SCRN_DSCR  3
#define E_GIF_ERR_HAS_IMAG_DSCR  4
#define E_GIF_ERR_NO_COLOR_MAP   5
#define E_GIF_ERR_DATA_TOO_BIG   6
#define E_GIF_ERR_NOT_ENOUGH_MEM 7
#define E_GIF_ERR_DISK_IS_FULL   8
#define E_GIF_ERR_CLOSE_FAILED   9
#define E_GIF_ERR_NOT_WRITEABLE  10

/* These are legacy.  You probably do not want to call them directly */
MODULE_API int EGifPutScreenDesc(GifFileType *GifFile,
                      const int GifWidth, const int GifHeight, 
		      const int GifColorRes,
                      const int GifBackGround,
                      const ColorMapObject *GifColorMap);
MODULE_API int EGifPutImageDesc(GifFileType *GifFile, 
		     const int GifLeft, const int GifTop,
                     const int GifWidth, const int GifHeight, 
		     const bool GifInterlace,
                     const ColorMapObject *GifColorMap);
MODULE_API void EGifSetGifVersion(GifFileType *GifFile, const bool gif89);
MODULE_API int EGifPutLine(GifFileType *GifFile, GifPixelType *GifLine,
                int GifLineLen);
MODULE_API int EGifPutPixel(GifFileType *GifFile, const GifPixelType GifPixel);
MODULE_API int EGifPutComment(GifFileType *GifFile, const char *GifComment);
MODULE_API int EGifPutExtensionLeader(GifFileType *GifFile, const int GifExtCode);
MODULE_API int EGifPutExtensionBlock(GifFileType *GifFile,
                         const int GifExtLen, const void *GifExtension);
MODULE_API int EGifPutExtensionTrailer(GifFileType *GifFile);
MODULE_API int EGifPutExtension(GifFileType *GifFile, const int GifExtCode, 
		     const int GifExtLen,
                     const void *GifExtension);
MODULE_API int EGifPutCode(GifFileType *GifFile, int GifCodeSize,
                const GifByteType *GifCodeBlock);
MODULE_API int EGifPutCodeNext(GifFileType *GifFile,
                    const GifByteType *GifCodeBlock);

/******************************************************************************
 GIF decoding routines
******************************************************************************/

/* Main entry points */
MODULE_API GifFileType *DGifOpenFileName(const char *GifFileName, int *Error);
MODULE_API GifFileType *DGifOpenFileHandle(int GifFileHandle, int *Error);
MODULE_API int DGifSlurp(GifFileType * GifFile);
MODULE_API GifFileType *DGifOpen(void *userPtr, InputFunc readFunc, int *Error);    /* new one (TVT) */
MODULE_API    int DGifCloseFile(GifFileType * GifFile, int *ErrorCode);

#define D_GIF_SUCCEEDED          0
#define D_GIF_ERR_OPEN_FAILED    101    /* And DGif possible errors. */
#define D_GIF_ERR_READ_FAILED    102
#define D_GIF_ERR_NOT_GIF_FILE   103
#define D_GIF_ERR_NO_SCRN_DSCR   104
#define D_GIF_ERR_NO_IMAG_DSCR   105
#define D_GIF_ERR_NO_COLOR_MAP   106
#define D_GIF_ERR_WRONG_RECORD   107
#define D_GIF_ERR_DATA_TOO_BIG   108
#define D_GIF_ERR_NOT_ENOUGH_MEM 109
#define D_GIF_ERR_CLOSE_FAILED   110
#define D_GIF_ERR_NOT_READABLE   111
#define D_GIF_ERR_IMAGE_DEFECT   112
#define D_GIF_ERR_EOF_TOO_SOON   113

/* These are legacy.  You probably do not want to call them directly */
MODULE_API int DGifGetScreenDesc(GifFileType *GifFile);
MODULE_API int DGifGetRecordType(GifFileType *GifFile, GifRecordType *GifType);
MODULE_API int DGifGetImageDesc(GifFileType *GifFile);
MODULE_API int DGifGetLine(GifFileType *GifFile, GifPixelType *GifLine, int GifLineLen);
MODULE_API int DGifGetPixel(GifFileType *GifFile, GifPixelType GifPixel);
MODULE_API int DGifGetComment(GifFileType *GifFile, char *GifComment);
MODULE_API int DGifGetExtension(GifFileType *GifFile, int *GifExtCode,
                     GifByteType **GifExtension);
MODULE_API int DGifGetExtensionNext(GifFileType *GifFile, GifByteType **GifExtension);
MODULE_API int DGifGetCode(GifFileType *GifFile, int *GifCodeSize,
                GifByteType **GifCodeBlock);
MODULE_API int DGifGetCodeNext(GifFileType *GifFile, GifByteType **GifCodeBlock);
MODULE_API int DGifGetLZCodes(GifFileType *GifFile, int *GifCode);


/******************************************************************************
 Color table quantization (deprecated)
******************************************************************************/
MODULE_API int GifQuantizeBuffer(unsigned int Width, unsigned int Height,
                   int *ColorMapSize, GifByteType * RedInput,
                   GifByteType * GreenInput, GifByteType * BlueInput,
                   GifByteType * OutputBuffer,
                   GifColorType * OutputColorMap);

/******************************************************************************
 Error handling and reporting.
******************************************************************************/
extern MODULE_API const char *GifErrorString(int ErrorCode);     /* new in 2012 - ESR */

/*****************************************************************************
 Everything below this point is new after version 1.2, supporting `slurp
 mode' for doing I/O in two big belts with all the image-bashing in core.
******************************************************************************/

/******************************************************************************
 Color map handling from gif_alloc.c
******************************************************************************/

extern MODULE_API ColorMapObject *GifMakeMapObject(int ColorCount,
                                     const GifColorType *ColorMap);
extern MODULE_API void GifFreeMapObject(ColorMapObject *Object);
extern MODULE_API ColorMapObject *GifUnionColorMap(const ColorMapObject *ColorIn1,
                                     const ColorMapObject *ColorIn2,
                                     GifPixelType ColorTransIn2[]);
extern MODULE_API int GifBitSize(int n);

extern MODULE_API void *
reallocarray(void *optr, size_t nmemb, size_t size);

/******************************************************************************
 Support for the in-core structures allocation (slurp mode).              
******************************************************************************/

extern MODULE_API void GifApplyTranslation(SavedImage *Image, GifPixelType Translation[]);
extern MODULE_API int GifAddExtensionBlock(int *ExtensionBlock_Count,
				ExtensionBlock **ExtensionBlocks, 
				int Function, 
				unsigned int Len, unsigned char ExtData[]);
extern MODULE_API void GifFreeExtensions(int *ExtensionBlock_Count,
			      ExtensionBlock **ExtensionBlocks);
extern MODULE_API SavedImage *GifMakeSavedImage(GifFileType *GifFile,
                                  const SavedImage *CopyFrom);
extern MODULE_API void GifFreeSavedImages(GifFileType *GifFile);

/******************************************************************************
 5.x functions for GIF89 graphics control blocks
******************************************************************************/

int MODULE_API DGifExtensionToGCB(const size_t GifExtensionLength,
		       const GifByteType *GifExtension,
		       GraphicsControlBlock *GCB);
size_t MODULE_API EGifGCBToExtension(const GraphicsControlBlock *GCB,
		       GifByteType *GifExtension);

int MODULE_API DGifSavedExtensionToGCB(GifFileType *GifFile, 
			    int ImageIndex, 
			    GraphicsControlBlock *GCB);
int MODULE_API EGifGCBToSavedExtension(const GraphicsControlBlock *GCB, 
			    GifFileType *GifFile, 
			    int ImageIndex);

/******************************************************************************
 The library's internal utility font                          
******************************************************************************/

#define GIF_FONT_WIDTH  8
#define GIF_FONT_HEIGHT 8
extern MODULE_API const unsigned char GifAsciiTable8x8[][GIF_FONT_WIDTH];

extern MODULE_API void GifDrawText8x8(SavedImage *Image,
                     const int x, const int y,
                     const char *legend, const int color);

extern void MODULE_API GifDrawBox(SavedImage *Image,
                    const int x, const int y,
                    const int w, const int d, const int color);

extern void MODULE_API GifDrawRectangle(SavedImage *Image,
                   const int x, const int y,
                   const int w, const int d, const int color);

extern void MODULE_API GifDrawBoxedText8x8(SavedImage *Image,
                          const int x, const int y,
                          const char *legend,
                          const int border, const int bg, const int fg);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _GIF_LIB_H */

/* end */
