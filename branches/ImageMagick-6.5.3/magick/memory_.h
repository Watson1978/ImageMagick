/*
  Copyright 1999-2009 ImageMagick Studio LLC, a non-profit organization
  dedicated to making software imaging solutions freely available.
  
  You may not use this file except in compliance with the License.
  obtain a copy of the License at
  
    http://www.imagemagick.org/script/license.php
  
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  MagickCore memory methods.
*/
#ifndef _MAGICKCORE_MEMORY_H
#define _MAGICKCORE_MEMORY_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef void
  *(*AcquireMemoryHandler)(size_t),
  (*DestroyMemoryHandler)(void *),
  *(*ResizeMemoryHandler)(void *,size_t);

extern MagickExport void
  *AcquireCachelineMemory(const size_t),
  *AcquireMagickMemory(const size_t),
  *AcquireQuantumMemory(const size_t,const size_t),
  *CopyMagickMemory(void *,const void *,const size_t),
  DestroyMagickMemory(void),
  GetMagickMemoryMethods(AcquireMemoryHandler *,ResizeMemoryHandler *,
    DestroyMemoryHandler *),
  *RelinquishMagickMemory(void *),
  *ResetMagickMemory(void *,int,const size_t),
  *ResizeMagickMemory(void *,const size_t),
  *ResizeQuantumMemory(void *,const size_t,const size_t),
  SetMagickMemoryMethods(AcquireMemoryHandler,ResizeMemoryHandler,
    DestroyMemoryHandler);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
