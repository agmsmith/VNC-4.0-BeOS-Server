/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/ServerMain.cxx,v 1.3 2004/01/25 02:57:42 agmsmith Exp agmsmith $
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
 * $Log: ServerMain.cxx,v $
 */

/* VNC library headers. */

#include <rfb/FrameBuffer.h>
#include <rfb/LogWriter.h>
#include <rfb/SDesktop.h>

/* BeOS (Be Operating System) headers. */

#include <DirectWindow.h>
#include <Locker.h>

/* Our source code */

#include "FrameBufferBeOS.h"
#include "SDesktopBeOS.h"



/******************************************************************************
 * Global variables, and not-so-variable things too.  Grouped by functionality.
 */

static rfb::LogWriter vlog("SDesktopBeOS");


SDesktopBeOS::SDesktopBeOS () :
  m_FrameBufferBeOSPntr (NULL),
  m_ServerPntr (NULL)
{
}


SDesktopBeOS::~SDesktopBeOS ()
{
  delete m_FrameBufferBeOSPntr;
  m_FrameBufferBeOSPntr = NULL;
}


void SDesktopBeOS::setServer (rfb::VNCServer *ServerPntr)
{
  m_ServerPntr = ServerPntr;
}


void SDesktopBeOS::start ()
{
  if (m_FrameBufferBeOSPntr == NULL)
    m_FrameBufferBeOSPntr = new FrameBufferBeOS;

  m_ServerPntr->setPixelBuffer (m_FrameBufferBeOSPntr);
}


void SDesktopBeOS::stop ()
{
  delete m_FrameBufferBeOSPntr;
  m_FrameBufferBeOSPntr = NULL;
  m_ServerPntr->setPixelBuffer (NULL);
}


void SDesktopBeOS::framebufferUpdateRequest ()
{
  // Time to send an update to a client?
}


rfb::Point SDesktopBeOS::getFbSize ()
{
  if (m_FrameBufferBeOSPntr != NULL)
    return rfb::Point (
      m_FrameBufferBeOSPntr->width (), m_FrameBufferBeOSPntr->height ());
  return rfb::Point (640, 480);
}
