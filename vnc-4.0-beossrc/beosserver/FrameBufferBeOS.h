/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/FrameBufferBeOS.h,v 1.1 2004/02/08 19:44:17 agmsmith Exp agmsmith $
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

  virtual void grabRect (const rfb::Rect &rect);

private:
  class BDirectWindowReader *m_ReaderWindowPntr;
    // Our BDirectWindow which is used for accessing the frame buffer.  NULL if
    // it hasn't been created.
};
