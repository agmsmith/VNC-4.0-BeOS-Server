/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/SDesktopBeOS.cxx,v 1.1 2004/06/07 01:07:28 agmsmith Exp agmsmith $
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
 * $Log: SDesktopBeOS.cxx,v $
 * Revision 1.1  2004/06/07 01:07:28  agmsmith
 * Initial revision
 *
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
  vlog.debug ("setServer called.");
  m_ServerPntr = ServerPntr;
}


void SDesktopBeOS::start ()
{
  vlog.debug ("start called.");

  if (m_FrameBufferBeOSPntr == NULL)
    m_FrameBufferBeOSPntr = new FrameBufferBeOS;

  m_ServerPntr->setPixelBuffer (m_FrameBufferBeOSPntr);
}


void SDesktopBeOS::stop ()
{
  vlog.debug ("stop called.");
  delete m_FrameBufferBeOSPntr;
  m_FrameBufferBeOSPntr = NULL;
  m_ServerPntr->setPixelBuffer (NULL);
}


void SDesktopBeOS::framebufferUpdateRequest ()
{
  rfb::PixelFormat NewScreenFormat;
  rfb::PixelFormat OldScreenFormat;
  rfb::Rect        RectangleChanged;
  char             TempString [80];
  static int       UpdateCounter = 0;

  if (m_FrameBufferBeOSPntr != NULL)
  {
    m_FrameBufferBeOSPntr->LockFrameBuffer ();

    try 
    {
      // Get the current screen size etc, if it has changed then inform the
      // server about the change.

      OldScreenFormat = m_FrameBufferBeOSPntr->getPF ();
      m_FrameBufferBeOSPntr->UpdatePixelFormatEtc ();
      NewScreenFormat = m_FrameBufferBeOSPntr->getPF ();
      if (!NewScreenFormat.equal(OldScreenFormat))
        m_ServerPntr->setPixelBuffer (m_FrameBufferBeOSPntr);

      // Tell the server to resend the changed areas. 

      RectangleChanged.setXYWH (0, 0,
        m_FrameBufferBeOSPntr->width (), m_FrameBufferBeOSPntr->height ());
      m_ServerPntr->add_changed (rfb::Region (RectangleChanged));
    }
    catch (...)
    {
      m_FrameBufferBeOSPntr->UnlockFrameBuffer ();
      throw;
    }
    m_FrameBufferBeOSPntr->UnlockFrameBuffer ();

    UpdateCounter++;
    sprintf (TempString, "Update #%d", UpdateCounter);
    m_FrameBufferBeOSPntr->SetDisplayMessage (TempString);
  }
}


rfb::Point SDesktopBeOS::getFbSize ()
{
  vlog.debug ("getFbSize called.");

  if (m_FrameBufferBeOSPntr != NULL)
  {
    m_FrameBufferBeOSPntr->LockFrameBuffer ();
    m_FrameBufferBeOSPntr->UpdatePixelFormatEtc ();
    m_FrameBufferBeOSPntr->UnlockFrameBuffer ();

    return rfb::Point (
      m_FrameBufferBeOSPntr->width (), m_FrameBufferBeOSPntr->height ());
  }
  return rfb::Point (640, 480);
}
