/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/FrameBufferBeOS.cxx,v 1.1 2004/02/08 19:44:17 agmsmith Exp agmsmith $
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
 * This variation of BDirectWindow allows us to capture the pixels on the
 * screen.  It works by changing the window's size to cover the whole screen,
 * then makes itself invisible (window borders off screen and background redraw
 * set to non-draw transparent) and moves behind all other windows (so users
 * can't click on it).  Then rather than the conventional use of a
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
};


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



/******************************************************************************
 * Implementation of the FrameBufferBeOS class.  Constructor, destructor and
 * the rest of the member functions in mostly alphabetical order.
 */

FrameBufferBeOS::FrameBufferBeOS ()
  : m_ReaderWindowPntr (NULL)
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


void FrameBufferBeOS::grabRect (const rfb::Rect &rect)
{
  if (m_ReaderWindowPntr == NULL)
  {
    vlog.debug ("FrameBufferBeOS::grabRect called for the first time, "
      "initialising frame buffer access.\n");
    m_ReaderWindowPntr = new BDirectWindowReader;
    m_ReaderWindowPntr->Show (); // Opens the window and starts its thread.
  }
}
