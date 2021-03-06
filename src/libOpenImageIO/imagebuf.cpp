/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


/// \file
/// Implementation of ImageBuf class.


#include <iostream>

#include <OpenEXR/ImathFun.h>
#include <OpenEXR/half.h>
#include <boost/scoped_ptr.hpp>

#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "imagecache.h"
#include "dassert.h"
#include "strutil.h"
#include "fmath.h"

OIIO_NAMESPACE_ENTER
{

ImageBuf::ImageBuf (const std::string &filename,
                    ImageCache *imagecache)
    : m_name(filename), m_nsubimages(0),
      m_current_subimage(-1), m_current_miplevel(-1),
      m_localpixels(NULL), m_clientpixels(false),
      m_spec_valid(false), m_pixels_valid(false),
      m_badfile(false), m_orientation(1), m_pixelaspect(1), 
      m_imagecache(imagecache)
{
}



ImageBuf::ImageBuf (const std::string &filename, const ImageSpec &spec)
    : m_name(filename), m_nsubimages(0),
      m_current_subimage(-1), m_current_miplevel(-1),
      m_localpixels(NULL), m_clientpixels(false),
      m_spec_valid(false), m_pixels_valid(false),
      m_badfile(false), m_orientation(1), m_pixelaspect(1),
      m_imagecache(NULL)
{
    alloc (spec);
}



ImageBuf::ImageBuf (const std::string &filename, const ImageSpec &spec,
                    void *buffer)
    : m_name(filename), m_nsubimages(0),
      m_current_subimage(-1), m_current_miplevel(-1),
      m_localpixels(NULL), m_clientpixels(false),
      m_spec_valid(false), m_pixels_valid(false),
      m_badfile(false), m_orientation(1), m_pixelaspect(1),
      m_imagecache(NULL)
{
    m_spec = spec;
    m_nativespec = spec;
    m_spec_valid = true;
    m_localpixels = (char *)buffer;
    m_clientpixels = true;
}



ImageBuf::ImageBuf (const ImageBuf &src)
    : m_name(src.m_name), m_fileformat(src.m_fileformat),
      m_nsubimages(src.m_nsubimages),
      m_current_subimage(src.m_current_subimage),
      m_current_miplevel(src.m_current_miplevel),
      m_nmiplevels(src.m_nmiplevels),
      m_spec(src.m_spec), m_nativespec(src.m_nativespec),
      m_pixels(src.m_pixels),
      m_localpixels(src.m_localpixels),
      m_clientpixels(src.m_clientpixels),
      m_spec_valid(src.m_spec_valid), m_pixels_valid(src.m_pixels_valid),
      m_badfile(src.m_badfile),
      m_orientation(src.m_orientation),
      m_pixelaspect(src.m_pixelaspect),
      m_imagecache(src.m_imagecache),
      m_cachedpixeltype(src.m_cachedpixeltype)
{
    if (src.localpixels()) {
        // Source had the image fully in memory (no cache)
        if (src.m_clientpixels) {
            // Source just wrapped the client app's pixels
            ASSERT (0 && "ImageBuf wrapping client buffer not yet supported");
        } else {
            // We own our pixels
            // Make sure our localpixels points to our own owned memory.
            m_localpixels = &m_pixels[0];
        }
    } else {
        // Source was cache-based
        // nothing else to do
    }
}



ImageBuf::~ImageBuf ()
{
    // Do NOT destroy m_imagecache here -- either it was created
    // externally and passed to the ImageBuf ctr or reset() method, or
    // else init_spec requested the system-wide shared cache, which
    // does not need to be destroyed.
}



#if 0
const ImageBuf &
ImageBuf::operator= (const ImageBuf &src)
{
    if (&src != this) {
        m_name = src.m_name;
        m_fileformat = src.m_fileformat;
        m_nsubimages = src.m_nsubimages;
        m_current_subimage = src.m_current_subimage;
        m_current_miplevel = src.m_current_miplevel;
        m_nmiplevels = src.m_nmiplevels;
        m_spec = src.m_spec;
        m_nativespec = src.m_nativespec;
        m_pixels = src.m_pixels;
        m_localpixels = src.m_localpixels;
        m_clientpixels = src.m_clientpixels;
        m_spec_valid = src.m_spec_valid;
        m_pixels_valid = src.m_pixels_valid;
        m_badfile = src.m_badfile;
        m_err.clear();
        m_orientation = src.m_orientation;
        m_pixelaspect = src.m_pixelaspect;
        m_imagecache = src.m_imagecache;
        m_cachedpixeltype = src.m_cachedpixeltype;
        if (src.localpixels()) {
            // Source had the image fully in memory (no cache)
            if (src.m_clientpixels) {
                // Source just wrapped the client app's pixels
                ASSERT (0 && "ImageBuf wrapping client buffer not yet supported");
                std::vector<char> tmp;
                std::swap (m_pixels, tmp);  // delete it with prejudice
            } else {
                // We own our pixels
                // Make sure our localpixels points to our own owned memory.
                m_localpixels = &m_pixels[0];
            }
        } else {
            // Source was cache-based
            std::vector<char> tmp;
            std::swap (m_pixels, tmp);  // delete it with prejudice
        }
    }
    return *this;
}
#endif



void
ImageBuf::clear ()
{
    m_name.clear ();
    m_fileformat.clear ();
    m_nsubimages = 0;
    m_current_subimage = -1;
    m_current_miplevel = -1;
    m_spec = ImageSpec ();
    m_nativespec = ImageSpec ();
    {
        std::vector<char> tmp;
        std::swap (m_pixels, tmp);  // clear it with deallocation
    }
    m_localpixels = NULL;
    m_clientpixels = false;
    m_spec_valid = false;
    m_pixels_valid = false;
    m_badfile = false;
    m_orientation = 1;
    m_pixelaspect = 1;
}



void
ImageBuf::reset (const std::string &filename, ImageCache *imagecache)
{
    clear ();
    m_name = ustring (filename);
    if (imagecache)
        m_imagecache = imagecache;
}



void
ImageBuf::reset (const std::string &filename, const ImageSpec &spec)
{
    clear ();
    m_name = ustring (filename);
    m_current_subimage = 0;
    m_current_miplevel = 0;
    alloc (spec);
}



void
ImageBuf::realloc ()
{
    size_t newsize = spec().image_bytes ();
    if (((int)m_pixels.size() - (int)newsize) > 4*1024*1024) {
        // If we are substantially shrinking, try to actually free
        // memory, which std::vector::resize does not do!
        std::vector<char> tmp;      // vector with 0 memory
        std::swap (tmp, m_pixels);  // Now tmp holds the mem, not m_pixels
        // As tmp leaves scope, it frees m_pixels's old memory
    }
    m_pixels.resize (newsize);
    m_localpixels = &m_pixels[0];
    m_clientpixels = false;
#if 0
    std::cerr << "ImageBuf " << m_name << " local allocation: " << newsize << "\n";
#endif
}



void
ImageBuf::alloc (const ImageSpec &spec)
{
    m_spec = spec;
    m_nativespec = spec;
    m_spec_valid = true;
    realloc ();
}



void
ImageBuf::copy_from (const ImageBuf &src)
{
    if (this == &src)
        return;
    ASSERT (m_spec.width == src.m_spec.width &&
            m_spec.height == src.m_spec.height &&
            m_spec.depth == src.m_spec.depth &&
            m_spec.nchannels == src.m_spec.nchannels);
    realloc ();
    src.get_pixels (src.xbegin(), src.xend(), src.ybegin(), src.yend(),
                    src.zbegin(), src.zend(), m_spec.format, m_localpixels);
}



bool
ImageBuf::init_spec (const std::string &filename, int subimage, int miplevel)
{
    if (m_current_subimage >= 0 && m_current_miplevel >= 0
            && m_name == filename && m_current_subimage == subimage
            && m_current_miplevel == miplevel)
        return true;   // Already done

    if (! m_imagecache) {
        m_imagecache = ImageCache::create (true /* shared cache */);
    }

    m_name = filename;
    m_nsubimages = 0;
    m_nmiplevels = 0;
    static ustring s_subimages("subimages"), s_miplevels("miplevels");
    m_imagecache->get_image_info (m_name, subimage, miplevel, s_subimages,
                                  TypeDesc::TypeInt, &m_nsubimages);
    m_imagecache->get_image_info (m_name, subimage, miplevel, s_miplevels,
                                  TypeDesc::TypeInt, &m_nmiplevels);
    m_imagecache->get_imagespec (m_name, m_spec, subimage, miplevel);
    m_imagecache->get_imagespec (m_name, m_nativespec, subimage, miplevel, true);
    if (m_nsubimages) {
        m_badfile = false;
        m_spec_valid = true;
        m_orientation = m_spec.get_int_attribute ("orientation", 1);
        m_pixelaspect = m_spec.get_float_attribute ("pixelaspectratio", 1.0f);
        m_current_subimage = subimage;
        m_current_miplevel = miplevel;
    } else {
        m_badfile = true;
        m_spec_valid = false;
        m_current_subimage = -1;
        m_current_miplevel = -1;
        m_err = m_imagecache->geterror ();
        // std::cerr << "ImageBuf ERROR: " << m_err << "\n";
    }

    return !m_badfile;
}



bool
ImageBuf::read (int subimage, int miplevel, bool force, TypeDesc convert,
               ProgressCallback progress_callback,
               void *progress_callback_data)
{
    if (pixels_valid() && !force &&
            subimage == this->subimage() && miplevel == this->miplevel())
        return true;

    if (! init_spec (m_name.string(), subimage, miplevel)) {
        m_badfile = true;
        m_spec_valid = false;
        return false;
    }

    // Set our current spec to the requested subimage
    if (! m_imagecache->get_imagespec (m_name, m_spec, subimage, miplevel) ||
        ! m_imagecache->get_imagespec (m_name, m_nativespec, subimage, miplevel, true)) {
        m_err = m_imagecache->geterror ();
        return false;
    }
    m_current_subimage = subimage;
    m_current_miplevel = miplevel;

#if 1
    // If we don't already have "local" pixels, and we aren't asking to
    // convert the pixels to a specific (and different) type, then take an
    // early out by relying on the cache.
    int peltype = TypeDesc::UNKNOWN;
    m_imagecache->get_image_info (m_name, subimage, miplevel,
                                  ustring("cachedpixeltype"),
                                  TypeDesc::TypeInt, &peltype);
    m_cachedpixeltype = TypeDesc ((TypeDesc::BASETYPE)peltype);
    if (! m_localpixels && ! force &&
        (convert == m_cachedpixeltype || convert == TypeDesc::UNKNOWN)) {
        m_spec.format = m_cachedpixeltype;
#ifdef DEBUG
        std::cerr << "read was not necessary -- using cache\n";
#endif
        return true;
    } else {
#ifdef DEBUG
        std::cerr << "going to have to read " << m_name << ": "
                  << m_spec.format.c_str() << " vs " << convert.c_str() << "\n";
#endif
    }        
#endif

    if (convert != TypeDesc::UNKNOWN)
        m_spec.format = convert;
    m_orientation = m_spec.get_int_attribute ("orientation", 1);
    m_pixelaspect = m_spec.get_float_attribute ("pixelaspectratio", 1.0f);

    realloc ();
    if (m_imagecache->get_pixels (m_name, subimage, miplevel,
                                  m_spec.x, m_spec.x+m_spec.width,
                                  m_spec.y, m_spec.y+m_spec.height,
                                  m_spec.z, m_spec.z+m_spec.depth,
                                  m_spec.format, m_localpixels)) {
        m_pixels_valid = true;
    } else {
        m_pixels_valid = false;
        m_err = m_imagecache->geterror ();
    }

    return m_pixels_valid;
}



bool
ImageBuf::write (ImageOutput *out,
                 ProgressCallback progress_callback,
                 void *progress_callback_data) const
{
    stride_t as = AutoStride;
    bool ok = true;
    if (m_localpixels) {
        ok = out->write_image (m_spec.format, m_localpixels, as, as, as,
                               progress_callback, progress_callback_data);
    } else {
        std::vector<char> tmp (m_spec.image_bytes());
        get_pixels (xbegin(), xend(), ybegin(), yend(), zbegin(), zend(),
                    m_spec.format, &tmp[0]);
        ok = out->write_image (m_spec.format, &tmp[0], as, as, as,
                               progress_callback, progress_callback_data);
        // FIXME -- not good for huge images.  Instead, we should read
        // little bits at a time (scanline or tile blocks).
    }
    if (! ok)
        m_err = out->geterror ();
    return ok;
}



bool
ImageBuf::save (const std::string &_filename, const std::string &_fileformat,
                ProgressCallback progress_callback,
                void *progress_callback_data) const
{
    std::string filename = _filename.size() ? _filename : name();
    std::string fileformat = _fileformat.size() ? _fileformat : filename;
    boost::scoped_ptr<ImageOutput> out (ImageOutput::create (fileformat.c_str(), "" /* searchpath */));
    if (! out) {
        m_err = geterror();
        return false;
    }
    if (! out->open (filename.c_str(), m_spec)) {
        m_err = out->geterror ();
        return false;
    }
    if (! write (out.get(), progress_callback, progress_callback_data))
        return false;
    out->close ();
    if (progress_callback)
        progress_callback (progress_callback_data, 0);
    return true;
}



bool
ImageBuf::reres (const ImageBuf &src)
{
    if (localpixels() && ! m_clientpixels) {
        m_spec.width = src.m_spec.width;
        m_spec.height = src.m_spec.height;
        m_spec.depth = src.m_spec.depth;
        m_spec.nchannels = src.m_spec.nchannels;
        m_spec.format = src.m_spec.format;
        m_spec.channelformats = src.m_spec.channelformats;
        m_spec.channelnames = src.m_spec.channelnames;
        m_spec.alpha_channel = src.m_spec.alpha_channel;
        m_spec.z_channel = src.m_spec.z_channel;
        return true;
    }
    return false;  // not the kind of ImageBuf that can be resized
}



void
ImageBuf::copy_metadata (const ImageBuf &src)
{
    m_spec.full_x = src.m_spec.full_x;
    m_spec.full_y = src.m_spec.full_y;
    m_spec.full_z = src.m_spec.full_z;
    m_spec.full_width = src.m_spec.full_width;
    m_spec.full_height = src.m_spec.full_height;
    m_spec.full_depth = src.m_spec.full_depth;
    m_spec.tile_width = src.m_spec.tile_width;
    m_spec.tile_height = src.m_spec.tile_height;
    m_spec.tile_depth = src.m_spec.tile_depth;
    m_spec.extra_attribs = src.m_spec.extra_attribs;
}



namespace {

// Pixel-by-pixel copy fully templated by both data types.
template<class D, class S>
void copy_pixels_2 (ImageBuf &dst, const ImageBuf &src, int xbegin, int xend,
                    int ybegin, int yend, int zbegin, int zend, int nchannels)
{
    if (is_same<D,S>::value) {
        // If both bufs are the same type, just directly copy the values
        ImageBuf::Iterator<D,D> d (dst, xbegin, xend, ybegin, yend, zbegin, zend);
        ImageBuf::ConstIterator<D,D> s (src, xbegin, xend, ybegin, yend, zbegin, zend);
        for ( ; ! d.done();  ++d, ++s) {
            if (s.exists() && d.exists()) {
                for (int c = 0;  c < nchannels;  ++c)
                    d[c] = s[c];
            }
        }
    } else {
        // If the two bufs are different types, convert through float
        ImageBuf::Iterator<D,float> d (dst, xbegin, xend, ybegin, yend, zbegin, zend);
        ImageBuf::ConstIterator<S,float> s (dst, xbegin, xend, ybegin, yend, zbegin, zend);
        for ( ; ! d.done();  ++d, ++s) {
            if (s.exists() && d.exists()) {
                for (int c = 0;  c < nchannels;  ++c)
                    d[c] = s[c];
            }
        }
    }        
}


// Call two-type template copy_pixels_2 based on src AND dst data type
template<class S>
void copy_pixels_ (ImageBuf &dst, const ImageBuf &src, int xbegin, int xend,
                   int ybegin, int yend, int zbegin, int zend, int nchannels)
{
    switch (dst.spec().format.basetype) {
    case TypeDesc::FLOAT :
        copy_pixels_2<float,S> (dst, src, xbegin, xend, ybegin, yend,
                                zbegin, zend, nchannels);
        break;
    case TypeDesc::UINT8 :
        copy_pixels_2<unsigned char,S> (dst, src, xbegin, xend, ybegin, yend,
                                        zbegin, zend, nchannels);
        break;
    case TypeDesc::INT8  :
        copy_pixels_2<char,S> (dst, src, xbegin, xend, ybegin, yend,
                               zbegin, zend, nchannels);
        break;
    case TypeDesc::UINT16:
        copy_pixels_2<unsigned short,S> (dst, src, xbegin, xend, ybegin, yend,
                                         zbegin, zend, nchannels);
        break;
    case TypeDesc::INT16 :
        copy_pixels_2<short,S> (dst, src, xbegin, xend, ybegin, yend,
                                zbegin, zend, nchannels);
        break;
    case TypeDesc::UINT  :
        copy_pixels_2<unsigned int,S> (dst, src, xbegin, xend, ybegin, yend,
                                       zbegin, zend, nchannels);
        break;
    case TypeDesc::INT   :
        copy_pixels_2<int,S> (dst, src, xbegin, xend, ybegin, yend,
                              zbegin, zend, nchannels);
        break;
    case TypeDesc::HALF  :
        copy_pixels_2<half,S> (dst, src, xbegin, xend, ybegin, yend,
                               zbegin, zend, nchannels);
        break;
    case TypeDesc::DOUBLE:
        copy_pixels_2<double,S> (dst, src, xbegin, xend, ybegin, yend,
                                 zbegin, zend, nchannels);
        break;
    case TypeDesc::UINT64:
        copy_pixels_2<unsigned long long,S> (dst, src, xbegin, xend, ybegin, yend,
                                             zbegin, zend, nchannels);
        break;
    case TypeDesc::INT64 :
        copy_pixels_2<long long,S> (dst, src, xbegin, xend, ybegin, yend,
                                    zbegin, zend, nchannels);
        break;
    default:
        ASSERT (0);
    }
}

}

bool
ImageBuf::copy_pixels (const ImageBuf &src)
{
    // compute overlap
    int xbegin = std::max (this->xbegin(), src.xbegin());
    int xend = std::min (this->xend(), src.xend());
    int ybegin = std::max (this->ybegin(), src.ybegin());
    int yend = std::min (this->yend(), src.yend());
    int zbegin = std::max (this->zbegin(), src.zbegin());
    int zend = std::min (this->zend(), src.zend());
    int nchannels = std::min (this->nchannels(), src.nchannels());

    // If we aren't copying over all our pixels, zero out the pixels
    if (xbegin != this->xbegin() || xend != this->xend() ||
        ybegin != this->ybegin() || yend != this->yend() ||
        zbegin != this->zbegin() || zend != this->zend() ||
        nchannels != this->nchannels())
        ImageBufAlgo::zero (*this);

    // Call template copy_pixels_ based on src data type
    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT :
        copy_pixels_<float> (*this, src, xbegin, xend, ybegin, yend,
                             zbegin, zend, nchannels);
        break;
    case TypeDesc::UINT8 :
        copy_pixels_<unsigned char> (*this, src, xbegin, xend, ybegin, yend,
                                     zbegin, zend, nchannels);
        break;
    case TypeDesc::INT8  :
        copy_pixels_<char> (*this, src, xbegin, xend, ybegin, yend,
                            zbegin, zend, nchannels);
        break;
    case TypeDesc::UINT16:
        copy_pixels_<unsigned short> (*this, src, xbegin, xend, ybegin, yend,
                                      zbegin, zend, nchannels);
        break;
    case TypeDesc::INT16 :
        copy_pixels_<short> (*this, src, xbegin, xend, ybegin, yend,
                             zbegin, zend, nchannels);
        break;
    case TypeDesc::UINT  :
        copy_pixels_<unsigned int> (*this, src, xbegin, xend, ybegin, yend,
                                    zbegin, zend, nchannels);
        break;
    case TypeDesc::INT   :
        copy_pixels_<int> (*this, src, xbegin, xend, ybegin, yend,
                           zbegin, zend, nchannels);
        break;
    case TypeDesc::HALF  :
        copy_pixels_<half> (*this, src, xbegin, xend, ybegin, yend,
                            zbegin, zend, nchannels);
        break;
    case TypeDesc::DOUBLE:
        copy_pixels_<double> (*this, src, xbegin, xend, ybegin, yend,
                              zbegin, zend, nchannels);
        break;
    case TypeDesc::UINT64:
        copy_pixels_<unsigned long long> (*this, src, xbegin, xend, ybegin, yend,
                                          zbegin, zend, nchannels);
        break;
    case TypeDesc::INT64 :
        copy_pixels_<long long> (*this, src, xbegin, xend, ybegin, yend,
                                 zbegin, zend, nchannels);
        break;
    default:
        ASSERT (0);
    }
    return true;
}



bool
ImageBuf::copy (const ImageBuf &src)
{
    if (! m_spec_valid && ! m_pixels_valid) {
        // uninitialized
        if (! src.m_spec_valid && ! src.m_pixels_valid)
            return true;   // uninitialized=uninitialized is a nop
        // uninitialized = initialized : set up *this with local storage
        reset (src.name(), src.spec());
    }

    bool selfcopy = (&src == this);

    if (cachedpixels()) {
        if (selfcopy) {  // special case: self copy of ImageCache loads locally
            return read (subimage(), miplevel(), true /*force*/);
        }
        reset (src.name(), src.spec());
        // Now it has local pixels
    }

    if (selfcopy)
        return true;

    if (localpixels()) {
        if (m_clientpixels) {
            // app-owned memory
            if (spec().width != src.spec().width ||
                spec().height != src.spec().height ||
                spec().depth != src.spec().depth ||
                spec().nchannels != src.spec().nchannels) {
                // size doesn't match, fail
                return false;
            }
            this->copy_metadata (src);
        } else {
            // locally owned memory -- we can fully resize it
            reset (src.name(), src.spec());
        }
        return this->copy_pixels (src);
    }

    return false;   // all other cases fail
}



template<typename T>
static inline float getchannel_ (const ImageBuf &buf, int x, int y, int c)
{
    ImageBuf::ConstIterator<T> pixel (buf, x, y);
    return pixel[c];
}



float
ImageBuf::getchannel (int x, int y, int c) const
{
    if (c < 0 || c >= spec().nchannels)
        return 0.0f;
    switch (spec().format.basetype) {
    case TypeDesc::FLOAT : return getchannel_<float> (*this, x, y, c);
    case TypeDesc::UINT8 : return getchannel_<unsigned char> (*this, x, y, c);
    case TypeDesc::INT8  : return getchannel_<char> (*this, x, y, c);
    case TypeDesc::UINT16: return getchannel_<unsigned short> (*this, x, y, c);
    case TypeDesc::INT16 : return getchannel_<short> (*this, x, y, c);
    case TypeDesc::UINT  : return getchannel_<unsigned int> (*this, x, y, c);
    case TypeDesc::INT   : return getchannel_<int> (*this, x, y, c);
    case TypeDesc::HALF  : return getchannel_<half> (*this, x, y, c);
    case TypeDesc::DOUBLE: return getchannel_<double> (*this, x, y, c);
    case TypeDesc::UINT64: return getchannel_<unsigned long long> (*this, x, y, c);
    case TypeDesc::INT64 : return getchannel_<long long> (*this, x, y, c);
    default:
        ASSERT (0);
        return 0.0f;
    }
}



template<typename T>
static inline void
getpixel_ (const ImageBuf &buf, int x, int y, int z, float *result, int chans)
{
    ImageBuf::ConstIterator<T> pixel (buf, x, y, z);
    if (pixel.valid()) {
        for (int i = 0;  i < chans;  ++i)
            result[i] = pixel[i];
    } else {
        for (int i = 0;  i < chans;  ++i)
            result[i] = 0.0f;
    }
}



void
ImageBuf::getpixel (int x, int y, int z, float *pixel, int maxchannels) const
{
    int n = std::min (spec().nchannels, maxchannels);
    switch (spec().format.basetype) {
    case TypeDesc::FLOAT : getpixel_<float> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT8 : getpixel_<unsigned char> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT8  : getpixel_<char> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT16: getpixel_<unsigned short> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT16 : getpixel_<short> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT  : getpixel_<unsigned int> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT   : getpixel_<int> (*this, x, y, z, pixel, n); break;
    case TypeDesc::HALF  : getpixel_<half> (*this, x, y, z, pixel, n); break;
    case TypeDesc::DOUBLE: getpixel_<double> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT64: getpixel_<unsigned long long> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT64 : getpixel_<long long> (*this, x, y, z, pixel, n); break;
    default:
        ASSERT (0);
    }
}



void
ImageBuf::interppixel (float x, float y, float *pixel) const
{
    const int maxchannels = 64;  // Reasonable guess
    float p[4][maxchannels];
    DASSERT (spec().nchannels <= maxchannels && 
             "You need to increase maxchannels in ImageBuf::interppixel");
    int n = std::min (spec().nchannels, maxchannels);
    x -= 0.5f;
    y -= 0.5f;
    int xtexel, ytexel;
    float xfrac, yfrac;
    xfrac = floorfrac (x, &xtexel);
    yfrac = floorfrac (y, &ytexel);
    getpixel (xtexel, ytexel, p[0], n);
    getpixel (xtexel+1, ytexel, p[1], n);
    getpixel (xtexel, ytexel+1, p[2], n);
    getpixel (xtexel+1, ytexel+1, p[3], n);
    bilerp (p[0], p[1], p[2], p[3], xfrac, yfrac, n, pixel);
}



template<typename T>
static inline void
setpixel_ (ImageBuf &buf, int x, int y, int z, const float *data, int chans)
{
    ImageBuf::Iterator<T> pixel (buf, x, y, z);
    if (pixel.valid()) {
        for (int i = 0;  i < chans;  ++i)
            pixel[i] = data[i];
    }
}



void
ImageBuf::setpixel (int x, int y, int z, const float *pixel, int maxchannels)
{
    int n = std::min (spec().nchannels, maxchannels);
    switch (spec().format.basetype) {
    case TypeDesc::FLOAT : setpixel_<float> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT8 : setpixel_<unsigned char> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT8  : setpixel_<char> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT16: setpixel_<unsigned short> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT16 : setpixel_<short> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT  : setpixel_<unsigned int> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT   : setpixel_<int> (*this, x, y, z, pixel, n); break;
    case TypeDesc::HALF  : setpixel_<half> (*this, x, y, z, pixel, n); break;
    case TypeDesc::DOUBLE: setpixel_<double> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT64: setpixel_<unsigned long long> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT64 : setpixel_<long long> (*this, x, y, z, pixel, n); break;
    default:
        ASSERT (0);
    }
}



void
ImageBuf::setpixel (int i, const float *pixel, int maxchannels)
{
    setpixel (spec().x + (i % spec().width), spec().y + (i / spec().width),
              pixel, maxchannels);
}



template<typename S, typename D>
static inline void 
get_pixels_ (const ImageBuf &buf, int xbegin, int xend,
             int ybegin, int yend, int zbegin, int zend, D *r)
{
    int w = (xend-xbegin), h = (yend-ybegin);
    imagesize_t wh = imagesize_t(w) * imagesize_t(h);
    for (ImageBuf::ConstIterator<S,D> p (buf, xbegin, xend, ybegin, yend, zbegin, zend);
         p.valid(); ++p) { 
        imagesize_t offset = ((p.z()-zbegin)*wh + (p.y()-ybegin)*w + (p.x()-xbegin)) * buf.nchannels();
        for (int c = 0;  c < buf.nchannels();  ++c)
            r[offset+c] = p[c];
    }
}



template<typename D>
bool
ImageBuf::get_pixels (int xbegin, int xend, int ybegin, int yend,
                      int zbegin, int zend, D *r) const
{
    // Caveat: serious hack here.  To avoid duplicating code, use a
    // #define.  Furthermore, exploit the CType<> template to construct
    // the right C data type for the given BASETYPE.
#define TYPECASE(B)                                                     \
    case B : get_pixels_<CType<B>::type,D>(*this, xbegin, xend, ybegin, yend, zbegin, zend, (D *)r); return true
    
    switch (spec().format.basetype) {
        TYPECASE (TypeDesc::UINT8);
        TYPECASE (TypeDesc::INT8);
        TYPECASE (TypeDesc::UINT16);
        TYPECASE (TypeDesc::INT16);
        TYPECASE (TypeDesc::UINT);
        TYPECASE (TypeDesc::INT);
        TYPECASE (TypeDesc::HALF);
        TYPECASE (TypeDesc::FLOAT);
        TYPECASE (TypeDesc::DOUBLE);
        TYPECASE (TypeDesc::UINT64);
        TYPECASE (TypeDesc::INT64);
    }
    return false;
#undef TYPECASE
}



bool
ImageBuf::get_pixels (int xbegin, int xend, int ybegin, int yend,
                       int zbegin, int zend,
                       TypeDesc format, void *result) const
{
#if 1
    // Fancy method -- for each possible base type that the user
    // wants for a destination type, call a template specialization.
    switch (format.basetype) {
    case TypeDesc::UINT8 :
        get_pixels<unsigned char> (xbegin, xend, ybegin, yend, zbegin, zend, (unsigned char *)result);
        break;
    case TypeDesc::INT8:
        get_pixels<char> (xbegin, xend, ybegin, yend, zbegin, zend, (char *)result);
        break;
    case TypeDesc::UINT16 :
        get_pixels<unsigned short> (xbegin, xend, ybegin, yend, zbegin, zend, (unsigned short *)result);
        break;
    case TypeDesc::INT16 :
        get_pixels<short> (xbegin, xend, ybegin, yend, zbegin, zend, (short *)result);
        break;
    case TypeDesc::UINT :
        get_pixels<unsigned int> (xbegin, xend, ybegin, yend, zbegin, zend, (unsigned int *)result);
        break;
    case TypeDesc::INT :
        get_pixels<int> (xbegin, xend, ybegin, yend, zbegin, zend, (int *)result);
        break;
    case TypeDesc::HALF :
        get_pixels<half> (xbegin, xend, ybegin, yend, zbegin, zend, (half *)result);
        break;
    case TypeDesc::FLOAT :
        get_pixels<float> (xbegin, xend, ybegin, yend, zbegin, zend, (float *)result);
        break;
    case TypeDesc::DOUBLE :
        get_pixels<double> (xbegin, xend, ybegin, yend, zbegin, zend, (double *)result);
        break;
    case TypeDesc::UINT64 :
        get_pixels<unsigned long long> (xbegin, xend, ybegin, yend, zbegin, zend, (unsigned long long *)result);
        break;
    case TypeDesc::INT64 :
        get_pixels<long long> (xbegin, xend, ybegin, yend, zbegin, zend, (long long *)result);
        break;
    default:
        return false;
    }
#else
    // Naive method -- loop over pixels, calling getpixel()
    size_t usersize = format.size() * nchannels();
    float *pel = (float *) alloca (nchannels() * sizeof(float));
    for (int z = zbegin;  z < zend;  ++z)
        for (int y = ybegin;  y < yend;  ++y)
            for (int x = xbegin;  x < xend;  ++x) {
                getpixel (x, y, z, pel);
                convert_types (TypeDesc::TypeFloat, pel,
                               format, result, nchannels());
                result = (void *) ((char *)result + usersize);
            }
#endif
    return true;
}

int
ImageBuf::oriented_width () const
{
    return m_orientation <= 4 ? m_spec.width : m_spec.height;
}



int
ImageBuf::oriented_height () const
{
    return m_orientation <= 4 ? m_spec.height : m_spec.width;
}



int
ImageBuf::oriented_x () const
{
    return m_orientation <= 4 ? m_spec.x : m_spec.y;
}



int
ImageBuf::oriented_y () const
{
    return m_orientation <= 4 ? m_spec.y : m_spec.x;
}



int
ImageBuf::oriented_full_width () const
{
    return m_orientation <= 4 ? m_spec.full_width : m_spec.full_height;
}



int
ImageBuf::oriented_full_height () const
{
    return m_orientation <= 4 ? m_spec.full_height : m_spec.full_width;
}



int
ImageBuf::oriented_full_x () const
{
    return m_orientation <= 4 ? m_spec.full_x : m_spec.full_y;
}



int
ImageBuf::oriented_full_y () const
{
    return m_orientation <= 4 ? m_spec.full_y : m_spec.full_x;
}



void
ImageBuf::set_full (int xbegin, int xend, int ybegin, int yend,
                    int zbegin, int zend, const float *bordercolor)
{
    m_spec.full_x = xbegin;
    m_spec.full_y = ybegin;
    m_spec.full_z = zbegin;
    m_spec.full_width  = xend - xbegin;
    m_spec.full_height = yend - ybegin;
    m_spec.full_depth  = zend - zbegin;
    if (bordercolor)
        m_spec.attribute ("oiio:bordercolor",
                          TypeDesc(TypeDesc::FLOAT,m_spec.nchannels),
                          bordercolor);
}



const void *
ImageBuf::pixeladdr (int x, int y, int z) const
{
    if (cachedpixels())
        return NULL;
    x -= spec().x;
    y -= spec().y;
    z -= spec().z;
    size_t p = y * m_spec.scanline_bytes() + x * m_spec.pixel_bytes();
    if (z)
        p += z * clamped_mult64 (m_spec.scanline_bytes(), (imagesize_t)spec().height);
    return &(m_localpixels[p]);
}



void *
ImageBuf::pixeladdr (int x, int y, int z)
{
    if (cachedpixels())
        return NULL;
    x -= spec().x;
    y -= spec().y;
    z -= spec().z;
    size_t p = y * m_spec.scanline_bytes() + x * m_spec.pixel_bytes();
    if (z)
        p += z * m_spec.scanline_bytes() * spec().height;
    return &(m_localpixels[p]);
}



const void *
ImageBuf::retile (int x, int y, int z, ImageCache::Tile* &tile,
                  int &tilexbegin, int &tileybegin, int &tilezbegin) const
{
    int tw = spec().tile_width, th = spec().tile_height;
    int td = std::max (1, spec().tile_depth);
    if (tile == NULL || x < tilexbegin || x >= (tilexbegin+tw) ||
                        y < tileybegin || y >= (tileybegin+th) ||
                        z < tilezbegin || z >= (tilezbegin+td)) {
        // not the same tile as before
        if (tile)
            m_imagecache->release_tile (tile);
        int xtile = (x-spec().x) / tw;
        int ytile = (y-spec().y) / th;
        int ztile = (z-spec().z) / td;
        tilexbegin = spec().x + xtile*tw;
        tileybegin = spec().y + ytile*th;
        tilezbegin = spec().z + ztile*td;
        tile = m_imagecache->get_tile (m_name, subimage(), miplevel(), x, y, z);
    }

    size_t offset = ((y - tileybegin) * tw) + (x - tilexbegin);
    offset += ((z - tilezbegin) * tw * th);
    offset *= spec().pixel_bytes();
    TypeDesc format;
    return (const char *)m_imagecache->tile_pixels (tile, format) + offset;
}

}
OIIO_NAMESPACE_EXIT
