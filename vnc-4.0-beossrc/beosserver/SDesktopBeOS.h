/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0-beossrc/beosserver/RCS/SDesktopBeOS.h,v 1.3 2004/07/19 22:30:19 agmsmith Exp agmsmith $
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
 * $Log: SDesktopBeOS.h,v $
 * Revision 1.3  2004/07/19 22:30:19  agmsmith
 * Updated to work with VNC 4.0 source code (was 4.0 beta 4).
 *
 * Revision 1.2  2004/07/05 00:53:32  agmsmith
 * Added mouse event handling - break down the network mouse event into
 * individual BMessages for the different mouse things, including the
 * mouse wheel.  Also add a forced refresh once in a while.
 *
 * Revision 1.1  2004/06/07 01:07:28  agmsmith
 * Initial revision
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

  virtual void start (rfb::VNCServer* vs);
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

  void forcedUpdateCheck ();
    // Checks if it is time for a forced update, and does it if needed.  This
    // gets called periodically by the server.

  virtual rfb::Point getFbSize ();
    // getFbSize() returns the current dimensions of the framebuffer.
    // This can be called even while the SDesktop is not start()ed.

  virtual void pointerEvent (const rfb::Point& pos, rdr::U8 buttonmask);
    // The remote user has moved the mouse or clicked a button.

  virtual void keyEvent (rdr::U32 key, bool down);
    // The remote user has pressed a key.

  void UpdateDerivedModifiersAndPressedModifierKeys (key_info &KeyState);
    // Looks at the modifier flags for individual modifier keys (left and right
    // control, L&R shift, etc) and sets the derived modifier flags (plain
    // control, plain shift, etc) to match.  Then it updates the keyboard bits
    // to show the corresponding buttons being pressed down or up (using the
    // previously obtained keymap).

protected:
  class FrameBufferBeOS *m_FrameBufferBeOSPntr;
    // Our FrameBufferBeOS instance and the associated BDirectWindowReader
    // (which may or may not exist) which is used for accessing the frame
    // buffer.  NULL if it hasn't been created.

  BInputDevice *m_InputDeviceKeyboardPntr;
  BInputDevice *m_InputDeviceMousePntr;
    // Gives access to our Input Server add-on which lets us inject mouse and
    // keyboard event messages.  NULL if the connection isn't open or isn't
    // available.  Connected when the desktop starts, disconnected when it
    // stops.

  char    *m_KeyCharStrings;
  key_map *m_KeyMapPntr;
    // NULL if not in use, otherwise they point to our copy (call free() when
    // done) of the keyboard mapping strings and tables that convert keyboard
    // scan codes into UTF-8 strings and various other keyboard mode
    // operations.  We actually use the tables in reverse to figure out which
    // buttons to press.  The keymap is copied from the current active one when
    // the desktop starts, so it doesn't reflect changes to the keymap while it
    // is running.

  key_info m_LastKeyState;
    // Identifies which of the 127 keys are currently being held down on the
    // imaginary ghost of the user's keyboard (using the current keymap to
    // figure out which keys do what).  Also remembers the modifier modes (caps
    // lock etc) last in use.

  rdr::U8 m_LastMouseButtonState;
    // The mouse buttons from the last remote mouse update.  Needed since
    // we have to convert the mouse events into up, down and moved events.

  float m_LastMouseX;
  float m_LastMouseY;
    // Last absolute (0.0 to 1.0) mouse position reported to BeOS.  Needed so
    // that we can avoid sending redundant mouse moved messages, particularly
    // if the user is moving the mouse wheel or just pressing buttons.

  bigtime_t m_NextForcedUpdateTime;
    // When the system clock reaches this time, do a full screen refresh.
    // Needed to awaken dead clients, that seem to stop updating once a button
    // is pressed.  It is normally some time in the future.

  rfb::VNCServer *m_ServerPntr;
    // Identifies our server, which we can tell about our frame buffer and
    // other changes.  NULL if it hasn't been set yet.
};
