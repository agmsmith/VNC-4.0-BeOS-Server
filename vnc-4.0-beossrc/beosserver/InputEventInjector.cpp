/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/SDesktopBeOS.cxx,v 1.2 2004/06/27 20:31:44 agmsmith Exp $
 *
 * This is the add-in (shared .so library for BeOS) which injects keyboard and
 * mouse events into the BeOS InputServer, letting the remote system move the
 * mouse and simulate keyboard button presses.
 *
 * It registers itself as a keyboard device and a mouse device with the
 * InputServer.  It also receives messages from other programs using the
 * BInputDevice Control system, and then copies and forwards those messages to
 * the InputServer.
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
 */

/* BeOS (Be Operating System) headers. */

#include <InputServerDevice.h>


/* POSIX headers. */

#include <stdio.h>


/******************************************************************************
 * Global variables, and not-so-variable things too.  Grouped by functionality.
 */

extern "C" _EXPORT BInputServerDevice* instantiate_input_device (void);


static struct input_device_ref FakeKeyboardLink =
{
  "VNC Fake Keyboard",
  B_KEYBOARD_DEVICE,
  (void *) 1 /* cookie */
};

static struct input_device_ref FakeMouseLink =
{
  "VNC Fake Mouse",
  B_POINTING_DEVICE,
  (void *) 2 /* cookie */
};

static struct input_device_ref *RegistrationRefList [3] =
{
  &FakeKeyboardLink,
  &FakeMouseLink,
  NULL
};



/******************************************************************************
 * The main class, does just about everything.
 */

class VNCAppServerInterface : public BInputServerDevice
{
public:
  VNCAppServerInterface ();
  virtual ~VNCAppServerInterface ();
  virtual status_t InitCheck ();
  virtual status_t Start (const char *device, void *cookie);
  virtual status_t Stop (const char *device, void *cookie);
  virtual status_t Control (
    const char *device, void *cookie, uint32 code, BMessage *message);

protected:
  bool m_KeyboardEnabled;
  bool m_MouseEnabled;
};


VNCAppServerInterface::VNCAppServerInterface ()
: m_KeyboardEnabled (false), m_MouseEnabled (false)
{
}


VNCAppServerInterface::~VNCAppServerInterface ()
{
}


status_t VNCAppServerInterface::InitCheck ()
{
  RegisterDevices (RegistrationRefList);

  return B_OK;
}


status_t VNCAppServerInterface::Start (const char *device, void *cookie)
{
  if ((int) cookie == 1)
    m_KeyboardEnabled = true;
  else if ((int) cookie == 2)
    m_MouseEnabled = true;
  else
    return B_ERROR;

  return B_OK;
}


status_t VNCAppServerInterface::Stop (const char *device, void *cookie)
{
  if ((int) cookie == 1)
    m_KeyboardEnabled = false;
  else if ((int) cookie == 2)
    m_MouseEnabled = false;
  else
    return B_ERROR;

  return B_OK;
}


status_t VNCAppServerInterface::Control (
  const char *device,
  void *cookie,
  uint32 code,
  BMessage *message)
{
  BMessage *EventMsgPntr = NULL;

  if ((int) cookie == 1)
  {
    if (m_KeyboardEnabled && code == 'ViNC' && message != NULL)
    {
      EventMsgPntr = new BMessage (*message);
      return EnqueueMessage (EventMsgPntr);
    }
  }
  else if ((int) cookie == 2)
  {
    if (m_MouseEnabled && code == 'ViNC' && message != NULL)
    {
      EventMsgPntr = new BMessage (*message);
      return EnqueueMessage (EventMsgPntr);
    }
  }
  else
    return B_ERROR;

  return B_OK;
}



BInputServerDevice* instantiate_input_device (void)
{
  return new VNCAppServerInterface;
}
