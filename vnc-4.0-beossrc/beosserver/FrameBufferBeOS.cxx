/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/FrameBufferBeOS.cxx,v 1.3 2004/06/07 01:06:50 agmsmith Exp agmsmith $
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
 * Revision 1.3  2004/06/07 01:06:50  agmsmith
 * Starting to get the SDesktop working with the frame buffer
 * and a BDirectWindow.
 *
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
 * This wraps the VNC colour map around the BeOS colour map.  It's a pretty
 * passive class, so other people do things to it to change the values.
 */

class ColourMapHolder : public rfb::ColourMap
{
public:
  virtual void lookup (int index, int* r, int* g, int* b);

  color_map m_BeOSColourMap;
    // The actual BeOS colour map to use.  Copied from the current screen
    // palette.
};


void ColourMapHolder::lookup (int index, int* r, int* g, int* b)
{
  rgb_color IndexedColour;

  if (index >= 0 && index < 256)
    IndexedColour = m_BeOSColourMap.color_list [index];
  else
  {
    IndexedColour.red = IndexedColour.green = 128;
    IndexedColour.blue = IndexedColour.alpha = 255;
  }
  *r = IndexedColour.red;
  *g = IndexedColour.green;
  *b = IndexedColour.blue;
}



/******************************************************************************
 * This BView draws the status text line and fills the window.
 */

class StatusDisplayBView : public BView
{
public:
  StatusDisplayBView (BRect ViewSize);

  virtual void AttachedToWindow (void);
  virtual void Draw (BRect UpdateRect);

  char *m_StringPntr;
};


StatusDisplayBView::StatusDisplayBView (BRect ViewSize)
  : BView (ViewSize, "StatusDisplayBView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW)
{
}


void StatusDisplayBView::AttachedToWindow (void)
{
  SetViewColor (100, 100, 0);
  SetHighColor (255, 255, 128);
  SetLowColor (ViewColor ());
}


void StatusDisplayBView::Draw (BRect UpdateRect)
{
  font_height FontHeight;
  BPoint      Location;
  float       Width;

  Location = Bounds().LeftTop ();
  GetFontHeight (&FontHeight);
  Location.y +=
    (Bounds().Height() - FontHeight.ascent - FontHeight.descent) / 2 +
    FontHeight.ascent;
  Width = StringWidth (m_StringPntr);
  Location.x += (Bounds().Width() - Width) / 2;
  DrawString (m_StringPntr, Location);
}



/******************************************************************************
 * This variation of BDirectWindow allows us to capture the pixels on the
 * screen.  It works by appearing in the corner (I couldn't make it invisible
 * and behind the desktop window).  Then rather than the conventional use of a
 * BDirectWindow where the application writes to the frame buffer memory
 * directly, we just read it directly.
 */

class BDirectWindowReader : public BDirectWindow
{
public:
  BDirectWindowReader ();
  virtual ~BDirectWindowReader ();

  virtual void DirectConnected (direct_buffer_info *ConnectionInfoPntr);
    // Callback called by the OS when the video resolution changes or the frame
    // buffer setup is otherwise changed.

  unsigned int LockSettings ();
  void UnlockSettings ();
    // Lock and unlock the access to the common bitmap information data,
    // so that the OS can't change it while the bitmap is being read.
    // Keep locked for at most 3 seconds, otherwise the OS will time out
    // and render the bitmap pointer invalid.  Lock returns the serial number
    // of the changes, so if the serial number is unchanged then the settings
    // (width, height, bitmap pointer, screen depth) are unchanged too.

  virtual void ScreenChanged (BRect frame, color_space mode);
    // Informs the window about a screen resolution change.

  void SetDisplayString (const char *NewString);
    // Sets the one line of text that's displayed by the status window.

  ColourMapHolder m_ColourMap;
    // The colour map for this window (and screen) when it is in palette mode.

  bool m_Connected;
    // TRUE if we are connected to the video memory, FALSE if not.  TRUE means
    // that video memory has been mapped into this process's address space and
    // we have a valid pointer to the frame buffer.  Don't try reading from
    // video memory if this is FALSE!

  unsigned int m_ConnectionVersion;
    // A counter that is bumped up by one every time the connection changes.
    // Makes it easy to see if your cached connection info is still valid.

  BLocker m_ConnectionLock;
    // This mutual exclusion lock makes sure that the callbacks from the OS to
    // notify the window about frame buffer changes (usually a screen
    // resolution change and the resulting change in frame buffer address and
    // size) are mutually exclusive from other window operations (like reading
    // the frame buffer or destroying the window).  Maximum lock time is 3
    // seconds, then the OS might kill the program for not responding.

  volatile bool m_DoNotConnect;
    // A flag that the destructor sets to tell the rest of the window code not
    // to try reconnecting to the frame buffer.

  direct_buffer_info m_SavedFrameBufferInfo;
      // A copy of the frame buffer information (bitmap address, video mode,
      // screen size) from the last direct connection callback by the OS.  Only
      // valid if m_Connected is true.  You should also lock m_ConnectionLock
      // while reading information from this structure, so it doesn't change
      // unexpectedly.

  BRect m_ScreenSize;
    // The size of the screen as a rectangle.  Updated when the resolution
    // changes.

protected:
  char m_DisplayString [1024];
    // This string is displayed in the server status window.  Since we couldn't
    // hide the window, or make it smaller than 10x10 pixels, we make it a
    // feature.

  BView *m_StatusDisplayView;
    // Points to the status display view if it exists (which just draws the
    // contents of m_DisplayString), NULL otherwise.
};


BDirectWindowReader::BDirectWindowReader ()
  : BDirectWindow (BRect (0, 0, 1, 1), "BDirectWindowReader",
    B_NO_BORDER_WINDOW_LOOK,
    B_FLOATING_ALL_WINDOW_FEEL,
    B_NOT_MOVABLE | B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AVOID_FOCUS,
    B_ALL_WORKSPACES),
  m_Connected (false),
  m_ConnectionVersion (0),
  m_DoNotConnect (true),
  m_StatusDisplayView (NULL)
{
  BScreen  ScreenInfo (this /* Info for screen this window is on */);

  m_ScreenSize = ScreenInfo.Frame ();
  MoveTo (m_ScreenSize.left, m_ScreenSize.top);
  ResizeTo (80, 20); // A small window so it doesn't obscure the desktop.

  memcpy (&m_ColourMap.m_BeOSColourMap,
    ScreenInfo.ColorMap (), sizeof (m_ColourMap.m_BeOSColourMap));

  strcpy (m_DisplayString, "VNC-BeOS");
  m_StatusDisplayView = new StatusDisplayBView (Bounds ());
  ((StatusDisplayBView *) m_StatusDisplayView)->m_StringPntr = m_DisplayString;
  AddChild (m_StatusDisplayView);
  m_DoNotConnect = false; // Now ready for active operation.
}


BDirectWindowReader::~BDirectWindowReader ()
{
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


unsigned int BDirectWindowReader::LockSettings ()
{
  m_ConnectionLock.Lock ();
  return m_ConnectionVersion;
}


void BDirectWindowReader::ScreenChanged (BRect frame, color_space mode)
{
  BDirectWindow::ScreenChanged (frame, mode);

  m_ConnectionLock.Lock ();
  m_ScreenSize = frame;
  m_ConnectionVersion++;
  m_ConnectionLock.Unlock ();

  // Update the palette if it is relevant.

  if (mode == B_CMAP8 || mode == B_GRAY8 || mode == B_GRAYSCALE_8_BIT ||
  mode == B_COLOR_8_BIT)
  {
    BScreen  ScreenInfo (this /* Info for screen this window is on */);
    memcpy (&m_ColourMap.m_BeOSColourMap,
      ScreenInfo.ColorMap (), sizeof (m_ColourMap.m_BeOSColourMap));
  }
}


void BDirectWindowReader::SetDisplayString (const char *NewString)
{
  if (NewString == NULL || NewString[0] == 0)
    m_DisplayString [0] = 0;
  else
    strncpy (m_DisplayString, NewString, sizeof (m_DisplayString));
  m_DisplayString [sizeof (m_DisplayString) - 1] = 0;

  if (m_StatusDisplayView != NULL)
    m_StatusDisplayView->Invalidate ();
}


void BDirectWindowReader::UnlockSettings ()
{
  m_ConnectionLock.Unlock ();
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
  vlog.debug ("Constructing a FrameBufferBeOS object.");

  if (BDirectWindow::SupportsWindowMode ())
  {
    m_ReaderWindowPntr = new BDirectWindowReader;
    m_ReaderWindowPntr->Show (); // Opens the window and starts its thread.
    snooze (50000 /* microseconds */);
    m_ReaderWindowPntr->Sync (); // Wait for it to finish drawing etc.
    snooze (50000 /* microseconds */);
    LockFrameBuffer ();
    UpdatePixelFormatEtc ();
    UnlockFrameBuffer ();
  }
  else
    throw rdr::Exception ("Windowed mode not supported for BDirectWindow, "
      "maybe your graphics card needs DMA support and a hardware cursor",
      "FrameBufferBeOS");
}


FrameBufferBeOS::~FrameBufferBeOS ()
{
  vlog.debug ("Destroying a FrameBufferBeOS object.");

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
}


void FrameBufferBeOS::grabRegion (const rfb::Region &rgn)
{
}


unsigned int FrameBufferBeOS::LockFrameBuffer ()
{
  if (m_ReaderWindowPntr != NULL)
    return m_ReaderWindowPntr->LockSettings ();
  return 0;
}


void FrameBufferBeOS::UnlockFrameBuffer ()
{
  if (m_ReaderWindowPntr != NULL)
    m_ReaderWindowPntr->UnlockSettings ();
}


void FrameBufferBeOS::SetDisplayMessage (const char *StringPntr)
{
  if (m_ReaderWindowPntr != NULL)
  {
    m_ReaderWindowPntr->Lock ();
    m_ReaderWindowPntr->SetDisplayString (StringPntr);
    m_ReaderWindowPntr->Unlock ();
  }
}


unsigned int FrameBufferBeOS::UpdatePixelFormatEtc ()
{
  direct_buffer_info *DirectInfoPntr;
  unsigned int        EndianTest;

  if (m_ReaderWindowPntr == NULL)
  {
    width_ = 0;
    height_ = 0;
    return m_CachedPixelFormatVersion;
  }

  if (m_CachedPixelFormatVersion != m_ReaderWindowPntr->m_ConnectionVersion)
  {
    m_CachedPixelFormatVersion = m_ReaderWindowPntr->m_ConnectionVersion;
    DirectInfoPntr = &m_ReaderWindowPntr->m_SavedFrameBufferInfo;

    // Set up some initial default values.  The actual values will be put in
    // depending on the particular video mode.

    colourmap = &m_ReaderWindowPntr->m_ColourMap;

    format.bpp = DirectInfoPntr->bits_per_pixel;
    // Number of actual colour bits, excluding alpha and pad bits.
    format.depth = DirectInfoPntr->bits_per_pixel;
    format.trueColour = true; // It usually is a non-palette video mode.

    EndianTest = 1;
    format.bigEndian = ((* (unsigned char *) &EndianTest) == 0);

    format.blueShift = 0;
    format.greenShift = 8;
    format.redShift = 16;
    format.redMax = format.greenMax = format.blueMax = 255;

    // Now set it according to the actual screen format.

    switch (DirectInfoPntr->pixel_format)
    {
      case B_RGB32: // xRGB 8:8:8:8, stored as little endian uint32
      case B_RGBA32: // ARGB 8:8:8:8, stored as little endian uint32
        format.bpp = 32;
        format.depth = 24;
        format.blueShift = 0;
        format.greenShift = 8;
        format.redShift = 16;
        format.redMax = format.greenMax = format.blueMax = 255;
        break;

      case B_RGB24:
        format.bpp = 24;
        format.depth = 24;
        format.blueShift = 0;
        format.greenShift = 8;
        format.redShift = 16;
        format.redMax = format.greenMax = format.blueMax = 255;
        break;

      case B_RGB16: // xRGB 5:6:5, stored as little endian uint16
        format.bpp = 16;
        format.depth = 16;
        format.blueShift = 0;
        format.greenShift = 5;
        format.redShift = 11;
        format.redMax = 31;
        format.greenMax = 63;
        format.blueMax = 31;
        break;

      case B_RGB15: // RGB 1:5:5:5, stored as little endian uint16
      case B_RGBA15: // ARGB 1:5:5:5, stored as little endian uint16
        format.bpp = 16;
        format.depth = 15;
        format.blueShift = 0;
        format.greenShift = 5;
        format.redShift = 10;
        format.redMax = format.greenMax = format.blueMax = 31;
        break;

      case B_CMAP8: // 256-color index into the color table.
      case B_GRAY8: // 256-color greyscale value.
        format.bpp = 8;
        format.depth = 8;
        format.blueShift = 0;
        format.greenShift = 0;
        format.redShift = 0;
        format.redMax = 255;
        format.greenMax = 255;
        format.blueMax = 255;
        format.trueColour = false;
        break;

      case B_RGB32_BIG: // xRGB 8:8:8:8, stored as big endian uint32
      case B_RGBA32_BIG: // ARGB 8:8:8:8, stored as big endian uint32
      case B_RGB24_BIG: // Currently unused
      case B_RGB16_BIG: // RGB 5:6:5, stored as big endian uint16
      case B_RGB15_BIG: // xRGB 1:5:5:5, stored as big endian uint16
      case B_RGBA15_BIG: // ARGB 1:5:5:5, stored as big endian uint16
        vlog.error ("Unimplemented big endian video mode #%d in "
        "UpdatePixelFormatEtc.",
        (unsigned int) DirectInfoPntr->pixel_format);
        break;

      default:
        vlog.error ("Unimplemented video mode #%d in UpdatePixelFormatEtc.",
        (unsigned int) DirectInfoPntr->pixel_format);
        break;
    }

    // Also update the real data - the actual bits and buffer size.

    data = (rdr::U8 *) DirectInfoPntr->bits;

    width_ = (int) (m_ReaderWindowPntr->m_ScreenSize.right -
      m_ReaderWindowPntr->m_ScreenSize.left + 1.5);
    height_ = (int) (m_ReaderWindowPntr->m_ScreenSize.bottom -
      m_ReaderWindowPntr->m_ScreenSize.top + 1.5);

    // Update the cached stride value.

    if (DirectInfoPntr->bits_per_pixel <= 0)
      m_CachedStride = 0;
    else
      m_CachedStride = DirectInfoPntr->bytes_per_row /
      ((DirectInfoPntr->bits_per_pixel + 7) / 8);

    // Print out the new settings.  May cause a deadlock if you happen to be
    // printing this when the video mode is switching, since the AppServer
    // would be locked out and unable to display the printed text.  But
    // that only happens in debug mode.

    char TempString [2048];
    sprintf (TempString,
      "UpdatePixelFormatEtc new settings: "
      "Width=%d, Stride=%d, Height=%d, Bits at $%08X, ",
      width_, m_CachedStride, height_, (unsigned int) data);
    format.print (TempString + strlen (TempString),
      sizeof (TempString) - strlen (TempString));
    vlog.debug (TempString);
  }

  return m_CachedPixelFormatVersion;
}
