/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/FrameBufferBeOS.h,v 1.3 2004/06/07 01:06:50 agmsmith Exp agmsmith $
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

  unsigned int LockFrameBuffer ();
  void UnlockFrameBuffer ();
    // Call these to lock the frame buffer so that none of the settings
    // or data pointers change, then do your work, then unlock it.  All
    // the other functions in this class assume you have locked the
    // frame buffer if needed.  LockFrameBuffer returns the serial number
    // of the current settings, which gets incremented if the OS changes
    // screen resolution etc, so you can tell if you have to change things.
    // Maximum lock time is 3 seconds, otherwise the OS might give up on
    // the screen updates and render the bitmap pointer invalid.

  void SetDisplayMessage (const char *StringPntr);
    // Sets the little bit of text in the corner of the screen that shows
    // the status of the server.

  unsigned int UpdatePixelFormatEtc ();
    // Makes sure the pixel format, width, height, raw bits pointer are
    // all up to date, matching the actual screen.  Returns the serial
    // number of the settings, so you can tell if they have changed since
    // the last time you called this function by a change in the serial
    // number.

protected:
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
