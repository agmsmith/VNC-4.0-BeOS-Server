/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/FrameBufferBeOS.h,v 1.2 2004/02/08 21:13:34 agmsmith Exp agmsmith $
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
 * $Log: FrameBufferBeOS.h,v $
 * Revision 1.2  2004/02/08 21:13:34  agmsmith
 * BDirectWindow stuff under construction.
 *
 * Revision 1.1  2004/02/08 19:44:17  agmsmith
 * Initial revision
 */


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

  unsigned int getPixelFormat (rfb::PixelFormat &pf);
    // Converts the current screen resolution, bitmap pointer and other
    // pixel layout information into a pixel format.  Returns the connection
    // version number that was in force when the pixel format was retrieved.

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



/******************************************************************************
 * This subclass of rfb:FrameBuffer lets us grab pixels from the screen.
 */

class FrameBufferBeOS : public rfb::FrameBuffer
{
public:
  FrameBufferBeOS ();
  virtual ~FrameBufferBeOS ();

  virtual int getStride () const;
  virtual void grabRect (const rfb::Rect &rect);
  virtual void grabRegion (const rfb::Region &rgn);

protected:
  void UpdateToCurrentScreenBitmapSettings ();
    // Makes sure the pixel format, width, height, raw bits pointer are
    // all up to date, matching the actual screen.

  class BDirectWindowReader *m_ReaderWindowPntr;
    // Our BDirectWindow which is used for accessing the frame buffer.  NULL if
    // it hasn't been created.

  unsigned int m_CachedPixelFormatVersion;
    // This version number helps us quickly detect changes to the video mode,
    // and thus let us avoid setting the pixel format on every frame grab.
    
  unsigned int m_CachedStride;
    // Number of pixels on a whole row.  Equals number of bytes per row
    // (including padding bytes) divided by the number of bytes per pixel.
};
