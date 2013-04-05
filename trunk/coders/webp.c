/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%                         W   W  EEEEE  BBBB   PPPP                           %
%                         W   W  E      B   B  P   P                          %
%                         W W W  EEE    BBBB   PPPP                           %
%                         WW WW  E      B   B  P                              %
%                         W   W  EEEEE  BBBB   P                              %
%                                                                             %
%                                                                             %
%                         Read/Write WebP Image Format                        %
%                                                                             %
%                              Software Design                                %
%                                John Cristy                                  %
%                                 March 2011                                  %
%                                                                             %
%                                                                             %
%  Copyright 1999-2013 ImageMagick Studio LLC, a non-profit organization      %
%  dedicated to making software imaging solutions freely available.           %
%                                                                             %
%  You may not use this file except in compliance with the License.  You may  %
%  obtain a copy of the License at                                            %
%                                                                             %
%    http://www.imagemagick.org/script/license.php                            %
%                                                                             %
%  Unless required by applicable law or agreed to in writing, software        %
%  distributed under the License is distributed on an "AS IS" BASIS,          %
%  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   %
%  See the License for the specific language governing permissions and        %
%  limitations under the License.                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%
*/

/*
  Include declarations.
*/
#include "MagickCore/studio.h"
#include "MagickCore/artifact.h"
#include "MagickCore/blob.h"
#include "MagickCore/blob-private.h"
#include "MagickCore/client.h"
#include "MagickCore/display.h"
#include "MagickCore/exception.h"
#include "MagickCore/exception-private.h"
#include "MagickCore/image.h"
#include "MagickCore/image-private.h"
#include "MagickCore/list.h"
#include "MagickCore/magick.h"
#include "MagickCore/monitor.h"
#include "MagickCore/monitor-private.h"
#include "MagickCore/memory_.h"
#include "MagickCore/option.h"
#include "MagickCore/pixel-accessor.h"
#include "MagickCore/quantum-private.h"
#include "MagickCore/static.h"
#include "MagickCore/string_.h"
#include "MagickCore/string-private.h"
#include "MagickCore/module.h"
#include "MagickCore/utility.h"
#include "MagickCore/xwindow.h"
#include "MagickCore/xwindow-private.h"
#if defined(MAGICKCORE_WEBP_DELEGATE)
#include <webp/decode.h>
#include <webp/encode.h>
#endif

/*
  Forward declarations.
*/
#if defined(MAGICKCORE_WEBP_DELEGATE)
static MagickBooleanType
  WriteWEBPImage(const ImageInfo *,Image *,ExceptionInfo *);
#endif

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   I s W E B P                                                               %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  IsWEBP() returns MagickTrue if the image format type, identified by the
%  magick string, is WebP.
%
%  The format of the IsWEBP method is:
%
%      MagickBooleanType IsWEBP(const unsigned char *magick,const size_t length)
%
%  A description of each parameter follows:
%
%    o magick: compare image format pattern against these bytes.
%
%    o length: Specifies the length of the magick string.
%
*/
static MagickBooleanType IsWEBP(const unsigned char *magick,const size_t length)
{
  if (length < 12)
    return(MagickFalse);
  if (LocaleNCompare((const char *) magick+8,"WEBP",4) == 0)
    return(MagickTrue);
  return(MagickFalse);
}

#if defined(MAGICKCORE_WEBP_DELEGATE)
/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   R e a d W E B P I m a g e                                                 %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  ReadWEBPImage() reads an image in the WebP image format.
%
%  The format of the ReadWEBPImage method is:
%
%      Image *ReadWEBPImage(const ImageInfo *image_info,
%        ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image_info: the image info.
%
%    o exception: return any errors or warnings in this structure.
%
*/

static inline uint32_t ReadWebPLSBWord(const unsigned char *restrict data)
{
  register const unsigned char
    *p;

  register uint32_t
    value;

  p=data;
  value=(uint32_t) (*p++);
  value|=((uint32_t) (*p++)) << 8;
  value|=((uint32_t) (*p++)) << 16;
  value|=((uint32_t) (*p++)) << 24;
  return(value);
}

static MagickBooleanType IsWEBPImageLossless(const unsigned char *stream,
  const size_t length)
{
#define VP8_CHUNK_INDEX  15
#define LOSSLESS_FLAG  'L'
#define EXTENDED_HEADER  'X'
#define VP8_CHUNK_HEADER  "VP8"
#define VP8_CHUNK_HEADER_SIZE  3
#define RIFF_HEADER_SIZE  12
#define VP8X_CHUNK_SIZE  10
#define TAG_SIZE  4
#define CHUNK_SIZE_BYTES  4
#define CHUNK_HEADER_SIZE  8
#define MAX_CHUNK_PAYLOAD  (~0U-CHUNK_HEADER_SIZE-1)

  ssize_t
    offset;

  /*
    Read simple header.
  */
  if (stream[VP8_CHUNK_INDEX] != EXTENDED_HEADER)
   return(stream[VP8_CHUNK_INDEX] == LOSSLESS_FLAG ? MagickTrue : MagickFalse);
  /*
    Read extended header.
  */
  offset=RIFF_HEADER_SIZE+TAG_SIZE+CHUNK_SIZE_BYTES+VP8X_CHUNK_SIZE;
  while (offset <= length)
  {
    uint32_t
      chunk_size,
      chunk_size_pad;

    chunk_size=ReadWebPLSBWord(stream+offset+TAG_SIZE);
    if (chunk_size > MAX_CHUNK_PAYLOAD)
      break;
    chunk_size_pad=(CHUNK_HEADER_SIZE+chunk_size+1) & ~1;
    if (memcmp( stream+offset,VP8_CHUNK_HEADER,VP8_CHUNK_HEADER_SIZE) == 0)
      return(*(stream+offset+VP8_CHUNK_HEADER_SIZE) == LOSSLESS_FLAG ?
        MagickTrue : MagickFalse);
    offset+=chunk_size_pad;
  }
  return(MagickFalse);
}

static Image *ReadWEBPImage(const ImageInfo *image_info,
  ExceptionInfo *exception)
{
  Image
    *image;

  MagickBooleanType
    status;

  register unsigned char
    *p;

  size_t
    length;

  ssize_t
    count,
    y;

  unsigned char
    *stream;

  WebPDecoderConfig
    configure;

  WebPDecBuffer
    *restrict webp_image = &configure.output;

  WebPBitstreamFeatures
    *restrict features = &configure.input;

  /*
    Open image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickSignature);
  if (image_info->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",
      image_info->filename);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickSignature);
  image=AcquireImage(image_info,exception);
  status=OpenBlob(image_info,image,ReadBinaryBlobMode,exception);
  if (status == MagickFalse)
    {
      image=DestroyImageList(image);
      return((Image *) NULL);
    }
  if (WebPInitDecoderConfig(&configure) == 0)
    ThrowReaderException(ResourceLimitError,"UnableToDecodeImageFile");
  length=(size_t) GetBlobSize(image);
  stream=(unsigned char *) AcquireQuantumMemory(length,sizeof(*stream));
  if (stream == (unsigned char *) NULL)
    ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
  count=ReadBlob(image,length,stream);
  if (count != (ssize_t) length)
    ThrowReaderException(CorruptImageError,"InsufficientImageDataInFile");
  if (WebPGetFeatures(stream,length,features) != 0)
    {
      stream=(unsigned char*) RelinquishMagickMemory(stream);
      ThrowReaderException(ResourceLimitError,"UnableToDecodeImageFile");
    }
  webp_image->colorspace=MODE_RGBA;
  if (WebPDecode(stream,length,&configure) != 0)
    {
      stream=(unsigned char*) RelinquishMagickMemory(stream);
      ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
    }
  image->columns=(size_t) webp_image->width;
  image->rows=(size_t) webp_image->height;
  image->alpha_trait=features->has_alpha != 0 ? BlendPixelTrait :
    UndefinedPixelTrait;
  if (IsWEBPImageLossless(stream,length) != MagickFalse)
    image->quality=100;
  p=webp_image->u.RGBA.rgba;
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    register Quantum
      *q;

    register ssize_t
      x;

    q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
    if (q == (Quantum *) NULL)
      break;
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      SetPixelRed(image,ScaleCharToQuantum(*p++),q);
      SetPixelGreen(image,ScaleCharToQuantum(*p++),q);
      SetPixelBlue(image,ScaleCharToQuantum(*p++),q);
      SetPixelAlpha(image,ScaleCharToQuantum(*p++),q);
      q+=GetPixelChannels(image);
    }
    if (SyncAuthenticPixels(image,exception) == MagickFalse)
      break;
    status=SetImageProgress(image,LoadImageTag,(MagickOffsetType) y,
      image->rows);
    if (status == MagickFalse)
      break;
  }
  WebPFreeDecBuffer(webp_image);
  stream=(unsigned char*) RelinquishMagickMemory(stream);
  return(image);
}
#endif

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   R e g i s t e r W E B P I m a g e                                         %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  RegisterWEBPImage() adds attributes for the WebP image format to
%  the list of supported formats.  The attributes include the image format
%  tag, a method to read and/or write the format, whether the format
%  supports the saving of more than one frame to the same file or blob,
%  whether the format supports native in-memory I/O, and a brief
%  description of the format.
%
%  The format of the RegisterWEBPImage method is:
%
%      size_t RegisterWEBPImage(void)
%
*/
ModuleExport size_t RegisterWEBPImage(void)
{
  char
    version[MaxTextExtent];

  MagickInfo
    *entry;

  *version='\0';
  entry=SetMagickInfo("WEBP");
#if defined(MAGICKCORE_WEBP_DELEGATE)
  entry->decoder=(DecodeImageHandler *) ReadWEBPImage;
  entry->encoder=(EncodeImageHandler *) WriteWEBPImage;
  (void) FormatLocaleString(version,MaxTextExtent,"libwebp %d.%d.%d",
    (WebPGetDecoderVersion() >> 16) & 0xff,
    (WebPGetDecoderVersion() >> 8) & 0xff,
    (WebPGetDecoderVersion() >> 0) & 0xff);
#endif
  entry->description=ConstantString("WebP Image Format");
  entry->adjoin=MagickFalse;
  entry->module=ConstantString("WEBP");
  entry->magick=(IsImageFormatHandler *) IsWEBP;
  if (*version != '\0')
    entry->version=ConstantString(version);
  (void) RegisterMagickInfo(entry);
  return(MagickImageCoderSignature);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   U n r e g i s t e r W E B P I m a g e                                     %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  UnregisterWEBPImage() removes format registrations made by the WebP module
%  from the list of supported formats.
%
%  The format of the UnregisterWEBPImage method is:
%
%      UnregisterWEBPImage(void)
%
*/
ModuleExport void UnregisterWEBPImage(void)
{
  (void) UnregisterMagickInfo("WEBP");
}
#if defined(MAGICKCORE_WEBP_DELEGATE)

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   W r i t e W E B P I m a g e                                               %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  WriteWEBPImage() writes an image in the WebP image format.
%
%  The format of the WriteWEBPImage method is:
%
%      MagickBooleanType WriteWEBPImage(const ImageInfo *image_info,
%        Image *image)
%
%  A description of each parameter follows.
%
%    o image_info: the image info.
%
%    o image:  The image.
%
*/

static int WebPWriter(const unsigned char *stream,size_t length,
  const WebPPicture *const picture)
{
  Image
    *image;

  image=(Image *) picture->custom_ptr;
  return(length != 0 ? (int) WriteBlob(image,length,stream) : 1);
}

static MagickBooleanType WriteWEBPImage(const ImageInfo *image_info,
  Image *image,ExceptionInfo *exception)
{
  const char
    *value;

  int
    webp_status;

  MagickBooleanType
    status;

  register uint32_t
    *restrict q;

  ssize_t
    y;

  WebPConfig
    configure;

  WebPPicture
    picture;

  WebPAuxStats
    statistics;

  /*
    Open output image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickSignature);
  assert(image != (Image *) NULL);
  assert(image->signature == MagickSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  if ((image->columns > 16383UL) || (image->rows > 16383UL))
    ThrowWriterException(ImageError,"WidthOrHeightExceedsLimit");
  status=OpenBlob(image_info,image,WriteBinaryBlobMode,exception);
  if (status == MagickFalse)
    return(status);
  if ((WebPPictureInit(&picture) == 0) || (WebPConfigInit(&configure) == 0))
    ThrowWriterException(ResourceLimitError,"UnableToEncodeImageFile");
  picture.writer=WebPWriter;
  picture.custom_ptr=(void *) image;
  picture.stats=(&statistics);
  picture.width=(int) image->columns;
  picture.height=(int) image->rows;
  picture.argb_stride=(int) image->columns;
  picture.use_argb=1;
  if (image->quality != UndefinedCompressionQuality)
    configure.quality=(float) image->quality;
  else
    if (image->quality >= 100)
      configure.lossless=1;
  value=GetImageArtifact(image,"webp:lossless");
  if (value != (char *) NULL)
    configure.lossless=ParseCommandOption(MagickBooleanOptions,MagickFalse,
      value);
  value=GetImageArtifact(image,"webp:method");
  if (value != (char *) NULL)
    configure.method=StringToInteger(value);
  value=GetImageArtifact(image,"webp:image-hint");
  if (value != (char *) NULL)
    {
      if (LocaleCompare(value,"graph") == 0)
        configure.image_hint=WEBP_HINT_GRAPH;
      if (LocaleCompare(value,"photo") == 0)
        configure.image_hint=WEBP_HINT_PHOTO;
      if (LocaleCompare(value,"picture") == 0)
        configure.image_hint=WEBP_HINT_PICTURE;
    }
  value=GetImageArtifact(image,"webp:target-size");
  if (value != (char *) NULL)
    configure.target_size=StringToInteger(value);
  value=GetImageArtifact(image,"webp:target-psnr");
  if (value != (char *) NULL)
    configure.target_PSNR=(float) StringToDouble(value,(char **) NULL);
  value=GetImageArtifact(image,"webp:segments");
  if (value != (char *) NULL)
    configure.segments=StringToInteger(value);
  value=GetImageArtifact(image,"webp:sns-strength");
  if (value != (char *) NULL)
    configure.sns_strength=StringToInteger(value);
  value=GetImageArtifact(image,"webp:filter-strength");
  if (value != (char *) NULL)
    configure.filter_strength=StringToInteger(value);
  value=GetImageArtifact(image,"webp:filter-sharpness");
  if (value != (char *) NULL)
    configure.filter_sharpness=StringToInteger(value);
  value=GetImageArtifact(image,"webp:filter-type");
  if (value != (char *) NULL)
    configure.filter_type=StringToInteger(value);
  value=GetImageArtifact(image,"webp:auto-filter");
  if (value != (char *) NULL)
    configure.autofilter=ParseCommandOption(MagickBooleanOptions,MagickFalse,
      value);
  value=GetImageArtifact(image,"webp:alpha-compression");
  if (value != (char *) NULL)
    configure.alpha_compression=StringToInteger(value);
  value=GetImageArtifact(image,"webp:alpha-filtering");
  if (value != (char *) NULL)
    configure.alpha_filtering=StringToInteger(value);
  value=GetImageArtifact(image,"webp:alpha-quality");
  if (value != (char *) NULL)
    configure.alpha_quality=StringToInteger(value);
  value=GetImageArtifact(image,"webp:pass");
  if (value != (char *) NULL)
    configure.pass=StringToInteger(value);
  value=GetImageArtifact(image,"webp:show-compressed");
  if (value != (char *) NULL)
    configure.show_compressed=StringToInteger(value);
  value=GetImageArtifact(image,"webp:preprocessing");
  if (value != (char *) NULL)
    configure.preprocessing=StringToInteger(value);
  value=GetImageArtifact(image,"webp:partitions");
  if (value != (char *) NULL)
    configure.partitions=StringToInteger(value);
  value=GetImageArtifact(image,"webp:partition-limit");
  if (value != (char *) NULL)
    configure.partition_limit=StringToInteger(value);
  if (WebPValidateConfig(&configure) == 0)
    ThrowWriterException(ResourceLimitError,"UnableToEncodeImageFile");
  /*
    Allocate memory for pixels.
  */
  picture.argb=(uint32_t *) AcquireQuantumMemory(image->columns,image->rows*
    sizeof(*picture.argb));
  if (picture.argb == (uint32_t *) NULL)
    ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
  /*
    Convert image to WebP raster pixels.
  */
  q=picture.argb;
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    register const Quantum
      *restrict p;

    register ssize_t
      x;

    p=GetVirtualPixels(image,0,y,image->columns,1,exception);
    if (p == (const Quantum *) NULL)
      break;
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      *q++=(uint32_t) (image->alpha_trait == BlendPixelTrait ?
        ScaleQuantumToChar(GetPixelAlpha(image,p)) << 24 : 0xff000000u) |
        (ScaleQuantumToChar(GetPixelRed(image,p)) << 16) |
        (ScaleQuantumToChar(GetPixelGreen(image,p)) << 8) |
        (ScaleQuantumToChar(GetPixelBlue(image,p)));
      p+=GetPixelChannels(image);
    }
    status=SetImageProgress(image,SaveImageTag,(MagickOffsetType) y,
      image->rows);
    if (status == MagickFalse)
      break;
  }
  webp_status=WebPEncode(&configure,&picture);
  WebPPictureFree(&picture);
  picture.argb=(uint32_t *) RelinquishMagickMemory(picture.argb);
  (void) CloseBlob(image);
  return(webp_status == 0 ? MagickFalse : MagickTrue);
}
#endif
