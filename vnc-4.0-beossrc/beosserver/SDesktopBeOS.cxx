/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/SDesktopBeOS.cxx,v 1.2 2004/06/27 20:31:44 agmsmith Exp agmsmith $
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
 * Revision 1.2  2004/06/27 20:31:44  agmsmith
 * Got it working, so you can now see the desktop in different
 * video modes (except 8 bit).  Even lets you switch screens!
 *
 * Revision 1.1  2004/06/07 01:07:28  agmsmith
 * Initial revision
 */

/* VNC library headers. */

#include <rfb/FrameBuffer.h>
#include <rfb/LogWriter.h>
#include <rfb/SDesktop.h>

/* BeOS (Be Operating System) headers. */

#include <DirectWindow.h>
#include <Input.h> // For BInputDevice.
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
  m_InputDeviceKeyboardPntr (NULL),
  m_InputDeviceMousePntr (NULL),
  m_LastMouseButtonState (0),
  m_LastMouseX (-1.0F),
  m_LastMouseY (-1.0F),
  m_NextForcedUpdateTime (0),
  m_ServerPntr (NULL)
{
}


SDesktopBeOS::~SDesktopBeOS ()
{
  delete m_FrameBufferBeOSPntr;
  m_FrameBufferBeOSPntr = NULL;
  delete m_InputDeviceKeyboardPntr;
  m_InputDeviceKeyboardPntr = NULL;
  delete m_InputDeviceMousePntr;
  m_InputDeviceMousePntr = NULL;
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

  if (m_InputDeviceKeyboardPntr == NULL)
    m_InputDeviceKeyboardPntr = find_input_device ("VNC Fake Keyboard");
  if (m_InputDeviceMousePntr == NULL)
    m_InputDeviceMousePntr = find_input_device ("VNC Fake Mouse");
}


void SDesktopBeOS::stop ()
{
  vlog.debug ("stop called.");

  delete m_FrameBufferBeOSPntr;
  m_FrameBufferBeOSPntr = NULL;
  m_ServerPntr->setPixelBuffer (NULL);

  delete m_InputDeviceKeyboardPntr;
  m_InputDeviceKeyboardPntr = NULL;
  delete m_InputDeviceMousePntr;
  m_InputDeviceMousePntr = NULL;
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
    m_NextForcedUpdateTime = system_time () + 10000000;
  }
}


void SDesktopBeOS::forcedUpdateCheck ()
{
  if (system_time () > m_NextForcedUpdateTime)
  {
    framebufferUpdateRequest ();
    m_NextForcedUpdateTime = system_time () + 10000000;
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


void SDesktopBeOS::pointerEvent (const rfb::Point& pos, rdr::U8 buttonmask)
{
  if (m_InputDeviceMousePntr == NULL || m_FrameBufferBeOSPntr == NULL ||
  m_FrameBufferBeOSPntr->width () <= 0)
    return;

  float    AbsoluteY;
  float    AbsoluteX;
  BMessage EventMessage;
  rdr::U8  NewMouseButtons;
  rdr::U8  OldMouseButtons;

  // Swap buttons 2 and 3, since BeOS uses left, middle, right and the rest of
  // the world has left, right, middle.  Or vice versa?

  NewMouseButtons = (buttonmask & 0xF9);
  if (buttonmask & 2)
    NewMouseButtons |= 4;
  if (buttonmask & 4)
    NewMouseButtons |= 2;
  buttonmask = NewMouseButtons;

  // Check for regular mouse button presses, don't include mouse wheel etc.

  NewMouseButtons = (buttonmask & 7);
  OldMouseButtons = (m_LastMouseButtonState & 7);

  if (NewMouseButtons == 0 && OldMouseButtons != 0)
    EventMessage.what = B_MOUSE_UP;
  else if (NewMouseButtons != 0 && OldMouseButtons == 0)
    EventMessage.what = B_MOUSE_DOWN;
  else
    EventMessage.what = B_MOUSE_MOVED;

  // Use absolute positions for the mouse, which just means writing a float
  // rather than an int for the X and Y values.  Scale it assuming the current
  // screen size.

  AbsoluteX = pos.x / (float) m_FrameBufferBeOSPntr->width ();
  if (AbsoluteX < 0.0F) AbsoluteX = 0.0F;
  if (AbsoluteX > 1.0F) AbsoluteX = 1.0F;

  AbsoluteY = pos.y / (float) m_FrameBufferBeOSPntr->height ();
  if (AbsoluteY < 0.0F) AbsoluteY = 0.0F;
  if (AbsoluteY > 1.0F) AbsoluteY = 1.0F;

  // Send the mouse movement or button press message to our Input Server
  // add-on, which will then forward it to the event queue input.  Avoid
  // sending messages which do nothing (can happen when the mouse wheel is
  // used).

  if (EventMessage.what != B_MOUSE_MOVED ||
  m_LastMouseX != AbsoluteX || m_LastMouseY != AbsoluteY ||
  NewMouseButtons != OldMouseButtons)
  {
    EventMessage.AddFloat ("x", AbsoluteX);
    EventMessage.AddFloat ("y", AbsoluteY);
    EventMessage.AddInt64 ("when", system_time ());
    EventMessage.AddInt32 ("buttons", NewMouseButtons);
    m_InputDeviceMousePntr->Control ('ViNC', &EventMessage);
  }

  // Check for a mouse wheel change (button 4 press+release is wheel up one
  // notch, button 5 is wheel down).  These get sent as a separate
  // B_MOUSE_WHEEL_CHANGED message.  We do it on the release.

  NewMouseButtons = (buttonmask & 0x18); // Don't include mouse wheel etc.
  OldMouseButtons = (m_LastMouseButtonState & 0x18);
  EventMessage.what = 0; // Means no event decided on yet.

  if (NewMouseButtons != OldMouseButtons)
  {
    if ((NewMouseButtons & 0x8) == 0 && (OldMouseButtons & 0x8) != 0)
    {
      // Button 4 has been released.  Wheel up a notch.
      EventMessage.what = B_MOUSE_WHEEL_CHANGED;
      EventMessage.AddInt64 ("when", system_time ());
      EventMessage.AddFloat ("be:wheel_delta_x", 0.0F);
      EventMessage.AddFloat ("be:wheel_delta_y", -1.0F);
    }
    else if ((NewMouseButtons & 0x10) == 0 && (OldMouseButtons & 0x10) != 0)
    {
      // Button 5 has been released.  Wheel down a notch.
      EventMessage.what = B_MOUSE_WHEEL_CHANGED;
      EventMessage.AddInt64 ("when", system_time ());
      EventMessage.AddFloat ("be:wheel_delta_x", 0.0F);
      EventMessage.AddFloat ("be:wheel_delta_y", 1.0F);
    }
  }
  if (EventMessage.what != 0)
    m_InputDeviceMousePntr->Control ('ViNC', &EventMessage);
  
  // Hack - for some reason the client stops requesting frames and thinks it is
  // up to date when the user clicks on something.  So force an update soon
  // after a mouse click or move is made.

  m_NextForcedUpdateTime = system_time () + 300000;

  m_LastMouseButtonState = buttonmask;
  m_LastMouseX = AbsoluteX;
  m_LastMouseY = AbsoluteY;
}


void SDesktopBeOS::keyEvent (rdr::U32 key, bool down)
{
}
