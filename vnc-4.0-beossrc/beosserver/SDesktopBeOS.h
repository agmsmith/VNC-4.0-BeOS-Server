/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/FrameBufferBeOS.h,v 1.2 2004/02/08 21:13:34 agmsmith Exp agmsmith $
 *
 * This is the static desktop glue implementation that holds the frame buffer
 * and handles mouse messages, the clipboard and other BeOS things on one side,
 * and talks to the VNC Server on the other side.
 *
 * Seems simple, but it shares the BDirectWindowReader with the frame buffer (a
 * FrameBufferBeOS) and uses the BDirectWindowReader for part of its view into
 * BeOS.  However, this static desktop is in charge - it creates the
 * FrameBufferBeOS, which in turn creates the BDirectWindowReader.
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
 */


/******************************************************************************
 * This is our main glue class for interfacing between VNC and BeOS.
 */

class SDesktopBeOS : public rfb::SDesktop
{
public:
  SDesktopBeOS ();
  virtual ~SDesktopBeOS ();

  void setServer (rfb::VNCServer *ServerPntr);
    // Specifies the VNC server to use.  This is the thing which will parse VNC
    // messages, handle network connections etc.

  virtual void start ();
    // start() is called by the server when the first client authenticates
    // successfully, and can be used to begin any expensive tasks which are not
    // needed when there are no clients.  A valid PixelBuffer must have been
    // set via the VNCServer's setPixelBuffer() method by the time this call
    // returns.

  virtual void stop ();
    // stop() is called by the server when there are no longer any
    // authenticated clients, and therefore the desktop can cease any expensive
    // tasks.

  virtual void framebufferUpdateRequest ();
    // framebufferUpdateRequest() is called to let the desktop know that at
    // least one client has become ready for an update.  Desktops can check
    // whether there are clients ready at any time by calling the VNCServer's
    // clientsReadyForUpdate() method.

  virtual rfb::Point getFbSize ();
    // getFbSize() returns the current dimensions of the framebuffer.
    // This can be called even while the SDesktop is not start()ed.

protected:
  class FrameBufferBeOS *m_FrameBufferBeOSPntr;
    // Our FrameBufferBeOS instance and the associated BDirectWindowReader
    // (which may or may not exist) which is used for accessing the frame
    // buffer.  NULL if it hasn't been created.

  rfb::VNCServer *m_ServerPntr;
    // Identifies our server, which we can tell about our frame buffer and
    // other changes.  NULL if it hasn't been set yet.
};
