/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/FrameBufferBeOS.cxx,v 1.2 2004/02/08 21:13:34 agmsmith Exp agmsmith $
 *
 * This is the frame buffer access module for the BeOS version of the VNC
 * server.  It implements an rfb::FrameBuffer object, which opens a
 * slave BDirectWindow to read the screen pixels.
 *
 * Copyright (C) 2004 by Alexander G. M. Smith.  All Rights Reserved.
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This software is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this software; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Log: FrameBufferBeOS.cxx,v $
 * Revision 1.2  2004/02/08 21:13:34  agmsmith
 * BDirectWindow stuff under construction.
 *
 * Revision 1.1  2004/02/08 19:44:17  agmsmith
 * Initial revision
 */

/* Posix headers. */

#include <errno.h>

/* VNC library headers. */

#include <rdr/Exception.h>
#include <rfb/FrameBuffer.h>
#include <rfb/LogWriter.h>

/* BeOS (Be Operating System) headers. */

#include <DirectWindow.h>
#include <Locker.h>
#include <Screen.h>
#include <View.h>

/* Our source code */

#include "FrameBufferBeOS.h"


/******************************************************************************
 * Global variables, and not-so-variable things too.  Grouped by functionality.
 */

static rfb::LogWriter vlog("FrameBufferBeOS");



/******************************************************************************
 * This BView fills the invisible window covering the screen.  It serves mainly
 * as a place to detect mouse clicks and movement.  Plus it doesn't have a draw
 * function and the background colour is set to transparent, so you can see the
 * desktop background behind it.
 */

class TransparentBView : public BView
{
public:
  TransparentBView (BRect ViewSize);

  virtual void AttachedToWindow (void);
};


TransparentBView::TransparentBView (BRect ViewSize)
  : BView (ViewSize, "TransparentBView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW)
{
}


void TransparentBView::AttachedToWindow (void)
{
  BRect  FrameRect;
  
  FrameRect = Frame ();
  vlog.debug ("TransparentBView::AttachedToWindow  "
    "Our frame is %f,%f,%f,%f.\n",
    FrameRect.left, FrameRect.top, FrameRect.right, FrameRect.bottom);

  SetViewColor (B_TRANSPARENT_COLOR);
}



/******************************************************************************
 * Our variation of a BDirectWindow.
 */

BDirectWindowReader::BDirectWindowReader ()
  : BDirectWindow (BRect (0, 0, 1, 1), "BDirectWindowReader",
    B_NO_BORDER_WINDOW_LOOK, B_FLOATING_ALL_WINDOW_FEEL,
    B_NOT_MOVABLE | B_NOT_ZOOMABLE | B_NOT_RESIZABLE, B_ALL_WORKSPACES),
  m_Connected (false),
  m_ConnectionVersion (0),
  m_DoNotConnect (true)
{
  BScreen  ScreenInfo (this /* Info for screen this window is on */);
  BRect    ScreenRect;

  vlog.debug ("Creating a BDirectWindowReader at address $%08X.\n",
    (unsigned int) this);

  ScreenRect = ScreenInfo.Frame ();
  MoveTo (ScreenRect.left, ScreenRect.top);
  ResizeTo (ScreenRect.Width(), ScreenRect.Height());

  AddChild (new TransparentBView (Bounds ()));
  m_DoNotConnect = false; // Now ready for active operation.
}


BDirectWindowReader::~BDirectWindowReader ()
{
  vlog.debug ("Destroying a BDirectWindowReader at address $%08X.\n",
    (unsigned int) this);

  m_DoNotConnect = true;

  // Force the window to disconnect from video memory, which will result in a
  // callback to our DirectConnected function.
  Hide ();
  Sync ();
}


void BDirectWindowReader::DirectConnected (
  direct_buffer_info *ConnectionInfoPntr)
{
  if (!m_Connected && m_DoNotConnect)
      return; // Shutting down or otherwise don't want to make a connection.

  m_ConnectionLock.Lock ();

  switch (ConnectionInfoPntr->buffer_state & B_DIRECT_MODE_MASK)
  {
    case B_DIRECT_START:
      m_Connected = true;
    case B_DIRECT_MODIFY:
      m_ConnectionVersion++;
      m_SavedFrameBufferInfo = *ConnectionInfoPntr;
      break;

    case B_DIRECT_STOP:
      m_Connected = false;
      break;
   }

   m_ConnectionLock.Unlock ();
}


unsigned int BDirectWindowReader::getPixelFormat (rfb::PixelFormat &pf)
{
  unsigned int EndianTest;
  unsigned int ReturnValue;

  m_ConnectionLock.Lock ();
  if (!m_Connected)
  {
    // Oops, looks like there is no frame buffer set up.
    m_ConnectionLock.Unlock ();
    return 0;
  }

  // Set up some initial default values.  The actual values will be put in
  // depending on the particular video mode.

  pf.bpp = m_SavedFrameBufferInfo.bits_per_pixel;
  // Number of actual colour bits, excluding alpha and pad bits.
  pf.depth = m_SavedFrameBufferInfo.bits_per_pixel;
  pf.trueColour = true; // It usually is a non-palette video mode.

  EndianTest = 1;
  pf.bigEndian = ((* (unsigned char *) &EndianTest) == 0);

  pf.blueShift = 0;
  pf.greenShift = 8;
  pf.redShift = 16;
  pf.redMax = pf.greenMax = pf.blueMax = 255;

  // Now set it according to the actual screen format.

  switch (m_SavedFrameBufferInfo.pixel_format)
  {
    case B_RGB32: // xRGB 8:8:8:8, stored as little endian uint32
    case B_RGBA32: // ARGB 8:8:8:8, stored as little endian uint32
      pf.bpp = 32;
      pf.depth = 24;
      pf.blueShift = 0;
      pf.greenShift = 8;
      pf.redShift = 16;
      pf.redMax = pf.greenMax = pf.blueMax = 255;
      break;

    case B_RGB24:
      pf.bpp = 24;
      pf.depth = 24;
      pf.blueShift = 0;
      pf.greenShift = 8;
      pf.redShift = 16;
      pf.redMax = pf.greenMax = pf.blueMax = 255;
      break;

    case B_RGB16: // xRGB 5:6:5, stored as little endian uint16
      pf.bpp = 16;
      pf.depth = 16;
      pf.blueShift = 0;
      pf.greenShift = 5;
      pf.redShift = 11;
      pf.redMax = 31;
      pf.greenMax = 63;
      pf.blueMax = 31;
      break;

    case B_RGB15: // RGB 1:5:5:5, stored as little endian uint16
    case B_RGBA15: // ARGB 1:5:5:5, stored as little endian uint16
      pf.bpp = 16;
      pf.depth = 15;
      pf.blueShift = 0;
      pf.greenShift = 5;
      pf.redShift = 10;
      pf.redMax = pf.greenMax = pf.blueMax = 31;
      break;

    case B_CMAP8: // 256-color index into the color table.
    case B_GRAY8: // 256-color greyscale value.
      pf.bpp = 8;
      pf.depth = 8;
      pf.blueShift = 0;
      pf.greenShift = 2;
      pf.redShift = 4;
      pf.redMax = 3;
      pf.greenMax = 3;
      pf.blueMax = 3;
      pf.trueColour = false;
      break;
      
    case B_RGB32_BIG: // xRGB 8:8:8:8, stored as big endian uint32
    case B_RGBA32_BIG: // ARGB 8:8:8:8, stored as big endian uint32
    case B_RGB24_BIG: // Currently unused
    case B_RGB16_BIG: // RGB 5:6:5, stored as big endian uint16
    case B_RGB15_BIG: // xRGB 1:5:5:5, stored as big endian uint16
    case B_RGBA15_BIG: // ARGB 1:5:5:5, stored as big endian uint16
      vlog.error ("Unimplemented big endian video mode #%d in "
      "BDirectWindowReader::getPixelFormat.\n",
      (unsigned int) m_SavedFrameBufferInfo.pixel_format);
      break;
    
    default:
      vlog.error ("Unimplemented video mode #%d in "
      "BDirectWindowReader::getPixelFormat.\n",
      (unsigned int) m_SavedFrameBufferInfo.pixel_format);
      break;
  }

  ReturnValue = m_ConnectionVersion;
    // Grab a copy since it may change when the lock is unlocked.

  m_ConnectionLock.Unlock ();
  return ReturnValue;
}



/******************************************************************************
 * Implementation of the FrameBufferBeOS class.  Constructor, destructor and
 * the rest of the member functions in mostly alphabetical order.
 */

FrameBufferBeOS::FrameBufferBeOS () :
  m_ReaderWindowPntr (NULL),
  m_CachedPixelFormatVersion (0),
  m_CachedStride (0)
{
  vlog.debug ("Constructing a FrameBufferBeOS object.\n");

  if (!BDirectWindow::SupportsWindowMode ())
  {
    throw rdr::Exception ("Windowed mode not supported for BDirectWindow, "
      "maybe your graphics card needs DMA support and a hardware cursor",
      "FrameBufferBeOS");
  }
}


FrameBufferBeOS::~FrameBufferBeOS ()
{
  vlog.debug ("Destroying a FrameBufferBeOS object.\n");

  if (m_ReaderWindowPntr != NULL)
  {
    m_ReaderWindowPntr->Lock ();
    m_ReaderWindowPntr->Quit (); // Closing the window makes it self destruct.
    m_ReaderWindowPntr = NULL;
  }
}


int FrameBufferBeOS::getStride () const
{
  return m_CachedStride;
}


void FrameBufferBeOS::grabRect (const rfb::Rect &rect)
{
  // Does nothing in BeOS, since the frame buffer is the screen and doesn't
  // need grabbing.  Also, grabRegion is the normal function used, grabRect is
  // only used by Windows (grabRegion iterates through all rectangles and calls
  // grabRect for each one).
  UpdateToCurrentScreenBitmapSettings ();
}


void FrameBufferBeOS::grabRegion (const rfb::Region &rgn)
{
  UpdateToCurrentScreenBitmapSettings ();
}


void FrameBufferBeOS::UpdateToCurrentScreenBitmapSettings ()
{
  direct_buffer_info *DirectInfoPntr;

  if (m_ReaderWindowPntr == NULL)
  {
    vlog.debug ("FrameBufferBeOS::UpdateToCurrentScreenBitmapSettings "
    "called for the first time, initialising frame buffer access.\n");
    m_ReaderWindowPntr = new BDirectWindowReader;
    m_ReaderWindowPntr->Show (); // Opens the window and starts its thread.
  }

  // We just have to make sure that the settings (width, height, bitmap data
  // pointer, etc) are up to date, and hopefully not changing until the
  // grabbing has been done.

  if (m_CachedPixelFormatVersion !=
  m_ReaderWindowPntr->m_ConnectionVersion)
  {
    m_CachedPixelFormatVersion = m_ReaderWindowPntr->getPixelFormat (format);
    DirectInfoPntr = &m_ReaderWindowPntr->m_SavedFrameBufferInfo;

    // Also update the real data - the actual bits and buffer size.

    data = (rdr::U8 *) DirectInfoPntr->bits;
    width_ = DirectInfoPntr->window_bounds.right -
      DirectInfoPntr->window_bounds.left + 1;
    height_ = DirectInfoPntr->window_bounds.bottom -
      DirectInfoPntr->window_bounds.top + 1;

    // Update the cached stride value.

    if (DirectInfoPntr->bits_per_pixel <= 0)
      m_CachedStride = 0;
    else
      m_CachedStride = DirectInfoPntr->bytes_per_row /
      DirectInfoPntr->bits_per_pixel;

    // Print out the new settings.

    char TempString [2048];
    sprintf (TempString,
      "FrameBufferBeOS::UpdateToCurrentScreenBitmapSettings new settings:\n"
      "Width=%d, Stride=%d, Height=%d, Bits at $%08X,\n",
      width_, m_CachedStride, height_, (unsigned int) data);
    format.print (TempString + strlen (TempString),
      sizeof (TempString) - strlen (TempString));
    vlog.debug (TempString);
  }
}
