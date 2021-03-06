/*
 * X11DRV bitmap objects
 *
 * Copyright 1993 Alexandre Julliard
 *           1999 Noel Borthwick
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include "windef.h"
#include "wingdi.h"
#include "wine/debug.h"
#include "x11drv.h"

WINE_DEFAULT_DEBUG_CHANNEL(x11drv);

  /* GCs used for B&W and color bitmap operations */
GC BITMAP_monoGC = 0, BITMAP_colorGC = 0;
X_PHYSBITMAP BITMAP_stock_phys_bitmap = { 0 };  /* phys bitmap for the default stock bitmap */

static XContext bitmap_context;  /* X context to associate a phys bitmap to a handle */

/***********************************************************************
 *           X11DRV_BITMAP_Init
 */
void X11DRV_BITMAP_Init(void)
{
    Pixmap tmpPixmap;

      /* Create the necessary GCs */

    wine_tsx11_lock();
    bitmap_context = XUniqueContext();
    BITMAP_stock_phys_bitmap.pixmap_depth = 1;
    BITMAP_stock_phys_bitmap.pixmap = XCreatePixmap( gdi_display, root_window, 1, 1, 1 );
    BITMAP_monoGC = XCreateGC( gdi_display, BITMAP_stock_phys_bitmap.pixmap, 0, NULL );
    XSetGraphicsExposures( gdi_display, BITMAP_monoGC, False );
    XSetSubwindowMode( gdi_display, BITMAP_monoGC, IncludeInferiors );

    if (screen_depth != 1)
    {
        if ((tmpPixmap = XCreatePixmap( gdi_display, root_window, 1, 1, screen_depth )))
        {
            BITMAP_colorGC = XCreateGC( gdi_display, tmpPixmap, 0, NULL );
            XSetGraphicsExposures( gdi_display, BITMAP_colorGC, False );
            XSetSubwindowMode( gdi_display, BITMAP_colorGC, IncludeInferiors );
            XFreePixmap( gdi_display, tmpPixmap );
        }
    }
    wine_tsx11_unlock();
}

/***********************************************************************
 *           SelectBitmap   (X11DRV.@)
 */
HBITMAP X11DRV_SelectBitmap( X11DRV_PDEVICE *physDev, HBITMAP hbitmap )
{
    X_PHYSBITMAP *physBitmap;

    if(physDev->xrender)
        X11DRV_XRender_UpdateDrawable( physDev );

    if (hbitmap == BITMAP_stock_phys_bitmap.hbitmap) physBitmap = &BITMAP_stock_phys_bitmap;
    else if (!(physBitmap = X11DRV_get_phys_bitmap( hbitmap ))) return 0;

    physDev->bitmap = physBitmap;
    physDev->drawable = physBitmap->pixmap;

      /* Change GC depth if needed */

    if (physDev->depth != physBitmap->pixmap_depth)
    {
        physDev->depth = physBitmap->pixmap_depth;
        wine_tsx11_lock();
        XFreeGC( gdi_display, physDev->gc );
        physDev->gc = XCreateGC( gdi_display, physDev->drawable, 0, NULL );
        XSetGraphicsExposures( gdi_display, physDev->gc, False );
        XSetSubwindowMode( gdi_display, physDev->gc, IncludeInferiors );
        XFlush( gdi_display );
        wine_tsx11_unlock();
    }
    return hbitmap;
}


/****************************************************************************
 *	  CreateBitmap   (X11DRV.@)
 *
 * Create a device dependent X11 bitmap
 *
 * Returns TRUE on success else FALSE
 */
BOOL X11DRV_CreateBitmap( X11DRV_PDEVICE *physDev, HBITMAP hbitmap, LPVOID bmBits )
{
    X_PHYSBITMAP *physBitmap;
    BITMAP bitmap;

    if (!GetObjectW( hbitmap, sizeof(bitmap), &bitmap )) return FALSE;

      /* Check parameters */
    if (bitmap.bmPlanes != 1) return FALSE;

    if ((bitmap.bmBitsPixel != 1) && (bitmap.bmBitsPixel != screen_depth))
    {
        ERR("Trying to make bitmap with planes=%d, bpp=%d\n",
            bitmap.bmPlanes, bitmap.bmBitsPixel);
        return FALSE;
    }
    if (hbitmap == BITMAP_stock_phys_bitmap.hbitmap)
    {
        ERR( "called for stock bitmap, please report\n" );
        return FALSE;
    }

    TRACE("(%p) %dx%d %d bpp\n", hbitmap, bitmap.bmWidth, bitmap.bmHeight, bitmap.bmBitsPixel);

    if (!(physBitmap = X11DRV_init_phys_bitmap( hbitmap ))) return FALSE;

      /* Create the pixmap */
    wine_tsx11_lock();
    physBitmap->pixmap_depth = bitmap.bmBitsPixel;
    physBitmap->pixmap = XCreatePixmap(gdi_display, root_window,
                                       bitmap.bmWidth, bitmap.bmHeight, bitmap.bmBitsPixel);
    wine_tsx11_unlock();
    if (!physBitmap->pixmap)
    {
        WARN("Can't create Pixmap\n");
        HeapFree( GetProcessHeap(), 0, physBitmap );
        return FALSE;
    }

    if (bmBits) /* Set bitmap bits */
    {
        X11DRV_SetBitmapBits( hbitmap, bmBits, bitmap.bmHeight * bitmap.bmWidthBytes );
    }
    else  /* else clear the bitmap */
    {
        wine_tsx11_lock();
        XSetFunction( gdi_display, BITMAP_GC(physBitmap), GXclear );
        XFillRectangle( gdi_display, physBitmap->pixmap, BITMAP_GC(physBitmap), 0, 0,
                        bitmap.bmWidth, bitmap.bmHeight );
        XSetFunction( gdi_display, BITMAP_GC(physBitmap), GXcopy );
        wine_tsx11_unlock();
    }
    return TRUE;
}


/***********************************************************************
 *           GetBitmapBits   (X11DRV.@)
 *
 * RETURNS
 *    Success: Number of bytes copied
 *    Failure: 0
 */
LONG X11DRV_GetBitmapBits( HBITMAP hbitmap, void *buffer, LONG count )
{
    BITMAP bitmap;
    X_PHYSBITMAP *physBitmap = X11DRV_get_phys_bitmap( hbitmap );
    LONG old_height, height;
    XImage *image;
    LPBYTE tbuf, startline;
    int	h, w;

    if (!physBitmap || !GetObjectW( hbitmap, sizeof(bitmap), &bitmap )) return 0;

    TRACE("(bmp=%p, buffer=%p, count=0x%lx)\n", hbitmap, buffer, count);

    wine_tsx11_lock();

    /* Hack: change the bitmap height temporarily to avoid */
    /*       getting unnecessary bitmap rows. */

    old_height = bitmap.bmHeight;
    height = bitmap.bmHeight = count / bitmap.bmWidthBytes;

    image = XGetImage( gdi_display, physBitmap->pixmap, 0, 0,
                       bitmap.bmWidth, bitmap.bmHeight, AllPlanes, ZPixmap );
    bitmap.bmHeight = old_height;

    /* copy XImage to 16 bit padded image buffer with real bitsperpixel */

    startline = buffer;
    switch (physBitmap->pixmap_depth)
    {
    case 1:
        for (h=0;h<height;h++)
        {
	    tbuf = startline;
            *tbuf = 0;
            for (w=0;w<bitmap.bmWidth;w++)
            {
                if ((w%8) == 0)
                    *tbuf = 0;
                *tbuf |= XGetPixel(image,w,h)<<(7-(w&7));
                if ((w&7) == 7) ++tbuf;
            }
            startline += bitmap.bmWidthBytes;
        }
        break;
    case 4:
        for (h=0;h<height;h++)
        {
	    tbuf = startline;
            for (w=0;w<bitmap.bmWidth;w++)
            {
                if (!(w & 1)) *tbuf = XGetPixel( image, w, h) << 4;
	    	else *tbuf++ |= XGetPixel( image, w, h) & 0x0f;
            }
            startline += bitmap.bmWidthBytes;
        }
        break;
    case 8:
        for (h=0;h<height;h++)
        {
	    tbuf = startline;
            for (w=0;w<bitmap.bmWidth;w++)
                *tbuf++ = XGetPixel(image,w,h);
            startline += bitmap.bmWidthBytes;
        }
        break;
    case 15:
    case 16:
        for (h=0;h<height;h++)
        {
	    tbuf = startline;
            for (w=0;w<bitmap.bmWidth;w++)
            {
	    	long pixel = XGetPixel(image,w,h);

		*tbuf++ = pixel & 0xff;
		*tbuf++ = (pixel>>8) & 0xff;
            }
            startline += bitmap.bmWidthBytes;
        }
        break;
    case 24:
        for (h=0;h<height;h++)
        {
	    tbuf = startline;
            for (w=0;w<bitmap.bmWidth;w++)
            {
	    	long pixel = XGetPixel(image,w,h);

		*tbuf++ = pixel & 0xff;
		*tbuf++ = (pixel>> 8) & 0xff;
		*tbuf++ = (pixel>>16) & 0xff;
	    }
            startline += bitmap.bmWidthBytes;
	}
        break;

    case 32:
        for (h=0;h<height;h++)
        {
	    tbuf = startline;
            for (w=0;w<bitmap.bmWidth;w++)
            {
	    	long pixel = XGetPixel(image,w,h);

		*tbuf++ = pixel & 0xff;
		*tbuf++ = (pixel>> 8) & 0xff;
		*tbuf++ = (pixel>>16) & 0xff;
		*tbuf++ = (pixel>>24) & 0xff;
	    }
            startline += bitmap.bmWidthBytes;
	}
        break;
    default:
        FIXME("Unhandled bits:%d\n", physBitmap->pixmap_depth);
    }
    XDestroyImage( image );
    wine_tsx11_unlock();
    return count;
}



/******************************************************************************
 *             SetBitmapBits   (X11DRV.@)
 *
 * RETURNS
 *    Success: Number of bytes used in setting the bitmap bits
 *    Failure: 0
 */
LONG X11DRV_SetBitmapBits( HBITMAP hbitmap, const void *bits, LONG count )
{
    BITMAP bitmap;
    X_PHYSBITMAP *physBitmap = X11DRV_get_phys_bitmap( hbitmap );
    LONG height;
    XImage *image;
    const BYTE *sbuf, *startline;
    int	w, h;

    if (!physBitmap || !GetObjectW( hbitmap, sizeof(bitmap), &bitmap )) return 0;

    TRACE("(bmp=%p, bits=%p, count=0x%lx)\n", hbitmap, bits, count);

    height = count / bitmap.bmWidthBytes;

    wine_tsx11_lock();
    image = XCreateImage( gdi_display, visual, physBitmap->pixmap_depth, ZPixmap, 0, NULL,
                          bitmap.bmWidth, height, 32, 0 );
    if (!(image->data = malloc(image->bytes_per_line * height)))
    {
        WARN("No memory to create image data.\n");
        XDestroyImage( image );
        wine_tsx11_unlock();
        return 0;
    }

    /* copy 16 bit padded image buffer with real bitsperpixel to XImage */

    startline = bits;

    switch (physBitmap->pixmap_depth)
    {
    case 1:
        for (h=0;h<height;h++)
        {
	    sbuf = startline;
            for (w=0;w<bitmap.bmWidth;w++)
            {
                XPutPixel(image,w,h,(sbuf[0]>>(7-(w&7))) & 1);
                if ((w&7) == 7)
                    sbuf++;
            }
            startline += bitmap.bmWidthBytes;
        }
        break;
    case 4:
        for (h=0;h<height;h++)
        {
	    sbuf = startline;
            for (w=0;w<bitmap.bmWidth;w++)
            {
                if (!(w & 1)) XPutPixel( image, w, h, *sbuf >> 4 );
                else XPutPixel( image, w, h, *sbuf++ & 0xf );
            }
            startline += bitmap.bmWidthBytes;
        }
        break;
    case 8:
        for (h=0;h<height;h++)
        {
	    sbuf = startline;
            for (w=0;w<bitmap.bmWidth;w++)
                XPutPixel(image,w,h,*sbuf++);
            startline += bitmap.bmWidthBytes;
        }
        break;
    case 15:
    case 16:
        for (h=0;h<height;h++)
        {
	    sbuf = startline;
            for (w=0;w<bitmap.bmWidth;w++)
            {
                XPutPixel(image,w,h,sbuf[1]*256+sbuf[0]);
                sbuf+=2;
            }
	    startline += bitmap.bmWidthBytes;
        }
        break;
    case 24:
        for (h=0;h<height;h++)
        {
	    sbuf = startline;
            for (w=0;w<bitmap.bmWidth;w++)
            {
                XPutPixel(image,w,h,(sbuf[2]<<16)+(sbuf[1]<<8)+sbuf[0]);
                sbuf += 3;
            }
            startline += bitmap.bmWidthBytes;
        }
        break;
    case 32:
        for (h=0;h<height;h++)
        {
	    sbuf = startline;
            for (w=0;w<bitmap.bmWidth;w++)
            {
                XPutPixel(image,w,h,(sbuf[3]<<24)+(sbuf[2]<<16)+(sbuf[1]<<8)+sbuf[0]);
                sbuf += 4;
            }
	    startline += bitmap.bmWidthBytes;
        }
        break;
    default:
      FIXME("Unhandled bits:%d\n", physBitmap->pixmap_depth);

    }
    XPutImage( gdi_display, physBitmap->pixmap, BITMAP_GC(physBitmap),
               image, 0, 0, 0, 0, bitmap.bmWidth, height );
    XDestroyImage( image ); /* frees image->data too */
    wine_tsx11_unlock();
    return count;
}

/***********************************************************************
 *           DeleteBitmap   (X11DRV.@)
 */
BOOL X11DRV_DeleteBitmap( HBITMAP hbitmap )
{
    X_PHYSBITMAP *physBitmap = X11DRV_get_phys_bitmap( hbitmap );

    if (physBitmap)
    {
        DIBSECTION dib;

        if (GetObjectW( hbitmap, sizeof(dib), &dib ) == sizeof(dib))
            X11DRV_DIB_DeleteDIBSection( physBitmap, &dib );

        if (physBitmap->glxpixmap) destroy_glxpixmap(physBitmap->glxpixmap); 
        wine_tsx11_lock();
        if (physBitmap->pixmap) XFreePixmap( gdi_display, physBitmap->pixmap );
        XDeleteContext( gdi_display, (XID)hbitmap, bitmap_context );
        wine_tsx11_unlock();
        HeapFree( GetProcessHeap(), 0, physBitmap );
    }
    return TRUE;
}


/***********************************************************************
 *           X11DRV_get_phys_bitmap
 *
 * Retrieve the X physical bitmap info.
 */
X_PHYSBITMAP *X11DRV_get_phys_bitmap( HBITMAP hbitmap )
{
    X_PHYSBITMAP *ret;

    wine_tsx11_lock();
    if (XFindContext( gdi_display, (XID)hbitmap, bitmap_context, (char **)&ret )) ret = NULL;
    wine_tsx11_unlock();
    return ret;
}


/***********************************************************************
 *           X11DRV_init_phys_bitmap
 *
 * Initialize the X physical bitmap info.
 */
X_PHYSBITMAP *X11DRV_init_phys_bitmap( HBITMAP hbitmap )
{
    X_PHYSBITMAP *ret;

    if ((ret = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*ret) )) != NULL)
    {
        ret->hbitmap = hbitmap;
        wine_tsx11_lock();
        XSaveContext( gdi_display, (XID)hbitmap, bitmap_context, (char *)ret );
        wine_tsx11_unlock();
    }
    return ret;
}


/***********************************************************************
 *           X11DRV_get_pixmap
 *
 * Retrieve the pixmap associated to a bitmap.
 */
Pixmap X11DRV_get_pixmap( HBITMAP hbitmap )
{
    X_PHYSBITMAP *physBitmap = X11DRV_get_phys_bitmap( hbitmap );

    if (!physBitmap) return 0;
    return physBitmap->pixmap;
}
