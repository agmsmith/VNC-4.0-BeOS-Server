/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0-beossrc/beosserver/RCS/SDesktopBeOS.cxx,v 1.6 2004/08/02 15:54:24 agmsmith Exp agmsmith $
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
 * Revision 1.6  2004/08/02 15:54:24  agmsmith
 * Rearranged methods to be in alphabetical order, still under construction.
 *
 * Revision 1.5  2004/07/25 21:03:27  agmsmith
 * Under construction, adding keymap and keycode simulation.
 *
 * Revision 1.4  2004/07/19 22:30:19  agmsmith
 * Updated to work with VNC 4.0 source code (was 4.0 beta 4).
 *
 * Revision 1.3  2004/07/05 00:53:32  agmsmith
 * Added mouse event handling - break down the network mouse event into
 * individual BMessages for the different mouse things, including the
 * mouse wheel.  Also add a forced refresh once in a while.
 *
 * Revision 1.2  2004/06/27 20:31:44  agmsmith
 * Got it working, so you can now see the desktop in different
 * video modes (except 8 bit).  Even lets you switch screens!
 *
 * Revision 1.1  2004/06/07 01:07:28  agmsmith
 * Initial revision
 */

/* VNC library headers. */

#include <rfb/PixelBuffer.h>
#include <rfb/LogWriter.h>
#include <rfb/SDesktop.h>

#define XK_MISCELLANY 1
#define XK_LATIN1 1
#include <rfb/keysymdef.h>

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


static char UTF8_Backspace [] = {B_BACKSPACE, 0};
static char UTF8_Return [] = {B_RETURN, 0};
static char UTF8_Tab [] = {B_TAB, 0};
static char UTF8_Escape [] = {B_ESCAPE, 0};
static char UTF8_LeftArrow [] = {B_LEFT_ARROW, 0};
static char UTF8_RightArrow [] = {B_RIGHT_ARROW, 0};
static char UTF8_UpArrow [] = {B_UP_ARROW, 0};
static char UTF8_DownArrow [] = {B_DOWN_ARROW, 0};
static char UTF8_Insert [] = {B_INSERT, 0};
static char UTF8_Delete [] = {B_DELETE, 0};
static char UTF8_Home [] = {B_HOME, 0};
static char UTF8_End [] = {B_END, 0};
static char UTF8_PageUp [] = {B_PAGE_UP, 0};
static char UTF8_PageDown [] = {B_PAGE_DOWN, 0};


// This table is used for converting VNC key codes into UTF-8 characters, but
// only for the normal printable characters.  Function keys and modifier keys
// (like shift) are handled separately.  Note that the table is in increasing
// order of VNC key code, so that a binary search can be done.

typedef struct VNCKeyToUTF8Struct
{
  uint16 vncKeyCode;
  char  *utf8String;
} VNCKeyToUTF8Record, *VNCKeyToUTF8Pointer;

extern "C" int CompareVNCKeyRecords (const void *APntr, const void *BPntr)
{
  int Result;

  Result = ((VNCKeyToUTF8Pointer) APntr)->vncKeyCode;
  Result -= ((VNCKeyToUTF8Pointer) BPntr)->vncKeyCode;;
  return Result;
}

static VNCKeyToUTF8Record VNCKeyToUTF8Array [] =
{
  {/* 0x0020 */ XK_space, " "},
  {/* 0xFF08 */ XK_BackSpace, UTF8_Backspace},
  {/* 0xFF09 */ XK_Tab, UTF8_Tab},
  {/* 0xFF0D */ XK_Return, UTF8_Return},
  {/* 0xFF1B */ XK_Escape, UTF8_Escape},
  {/* 0xFF50 */ XK_Home, UTF8_Home},
  {/* 0xFF51 */ XK_Left, UTF8_LeftArrow},
  {/* 0xFF52 */ XK_Up, UTF8_UpArrow},
  {/* 0xFF53 */ XK_Right, UTF8_RightArrow},
  {/* 0xFF54 */ XK_Down, UTF8_DownArrow},
  {/* 0xFF55 */ XK_Page_Up, UTF8_PageUp},
  {/* 0xFF56 */ XK_Page_Down, UTF8_PageDown},
  {/* 0xFF57 */ XK_End, UTF8_End},
  {/* 0xFF63 */ XK_Insert, UTF8_Insert},
  {/* 0xFFFF */ XK_Delete, UTF8_Delete},
};



/******************************************************************************
 * Utility functions.
 */

static inline void SetKeyState (
  key_info &KeyState,
  uint8 KeyCode,
  bool KeyIsDown)
{
  uint8 BitMask;
  uint8 Index;

  if (KeyCode <= 0 || KeyCode >= 128)
    return; // Keycodes are from 1 to 127, zero means no key defined.

  Index = KeyCode / 8;
  BitMask = (1 << (7 - (KeyCode & 7)));
  if (KeyIsDown)
    KeyState.key_states[Index] |= BitMask;
  else
    KeyState.key_states[Index] &= ~BitMask;
}



/******************************************************************************
 * The SDesktopBeOS class, methods follow in mostly alphabetical order.
 */

SDesktopBeOS::SDesktopBeOS () :
  m_FrameBufferBeOSPntr (NULL),
  m_InputDeviceKeyboardPntr (NULL),
  m_InputDeviceMousePntr (NULL),
  m_KeyCharStrings (NULL),
  m_KeyMapPntr (NULL),
  m_LastMouseButtonState (0),
  m_LastMouseX (-1.0F),
  m_LastMouseY (-1.0F),
  m_NextForcedUpdateTime (0),
  m_ServerPntr (NULL)
{
  memset (&m_LastKeyState, 0, sizeof (m_LastKeyState));
}


SDesktopBeOS::~SDesktopBeOS ()
{
  free (m_KeyCharStrings);
  m_KeyCharStrings = NULL;
  free (m_KeyMapPntr);
  m_KeyMapPntr = NULL;

  delete m_FrameBufferBeOSPntr;
  m_FrameBufferBeOSPntr = NULL;

  delete m_InputDeviceKeyboardPntr;
  m_InputDeviceKeyboardPntr = NULL;
  delete m_InputDeviceMousePntr;
  m_InputDeviceMousePntr = NULL;
}


void SDesktopBeOS::forcedUpdateCheck ()
{
  if (system_time () > m_NextForcedUpdateTime)
  {
    framebufferUpdateRequest ();
    m_NextForcedUpdateTime = system_time () + 10000000;
  }
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


void SDesktopBeOS::keyEvent (rdr::U32 key, bool down)
{
  uint32              ChangedModifiers;
  BMessage            EventMessage;
  uint8               KeyCode;
  char                KeyAsString [16];
  VNCKeyToUTF8Record  KeyToUTF8SearchData;
  VNCKeyToUTF8Pointer KeyToUTF8Pntr;
  key_info            NewKeyState;

  printf ("SDesktopBeOS::keyEvent  Key %X, down: %d\n", key, down);

  if (m_InputDeviceKeyboardPntr == NULL || m_FrameBufferBeOSPntr == NULL ||
  m_FrameBufferBeOSPntr->width () <= 0 || m_KeyMapPntr == NULL)
    return;

  NewKeyState = m_LastKeyState;

  // If it's a shift or other modifier key, update our internal modifiers
  // state.

  switch (key)
  {
    case XK_Caps_Lock: ChangedModifiers = B_CAPS_LOCK; break;
    case XK_Scroll_Lock: ChangedModifiers = B_SCROLL_LOCK; break;
    case XK_Num_Lock: ChangedModifiers = B_NUM_LOCK; break;
    case XK_Shift_L: ChangedModifiers = B_LEFT_SHIFT_KEY; break;
    case XK_Shift_R: ChangedModifiers = B_RIGHT_SHIFT_KEY; break;
    case XK_Control_L: ChangedModifiers = B_LEFT_CONTROL_KEY; break;
    case XK_Control_R: ChangedModifiers = B_RIGHT_CONTROL_KEY; break;
    case XK_Alt_L: ChangedModifiers = B_LEFT_COMMAND_KEY; break;
    case XK_Alt_R: ChangedModifiers = B_RIGHT_COMMAND_KEY; break;
    case XK_Meta_L: ChangedModifiers = B_LEFT_OPTION_KEY; break;
    case XK_Meta_R: ChangedModifiers = B_RIGHT_OPTION_KEY; break;
    default: ChangedModifiers = 0;
  }

  // Update the modifiers for the particular modifier key if one was pressed.

  if (ChangedModifiers != 0)
  {
    if (down)
      NewKeyState.modifiers |= ChangedModifiers;
    else
      NewKeyState.modifiers &= ~ChangedModifiers;

    UpdateDerivedModifiersAndPressedModifierKeys (NewKeyState);

    if (NewKeyState.modifiers != m_LastKeyState.modifiers)
    {
      // Send a B_MODIFIERS_CHANGED message to update the system with the new
      // modifier key settings.  Note that this gets sent before the unmapped
      // key message.

      EventMessage.what = B_MODIFIERS_CHANGED;
      EventMessage.AddInt64 ("when", system_time ());
      EventMessage.AddInt32 ("be:old_modifiers", m_LastKeyState.modifiers);
      EventMessage.AddInt32 ("modifiers", NewKeyState.modifiers);
      EventMessage.AddData ("states",
        B_UINT8_TYPE, &NewKeyState.key_states, 16);
      m_InputDeviceKeyboardPntr->Control ('ViNC', &EventMessage);
      EventMessage.MakeEmpty ();
    }

    SendUnmappedKeys (m_LastKeyState, NewKeyState);
    m_LastKeyState = NewKeyState;

    if (ChangedModifiers != B_SCROLL_LOCK)
      return; // No actual typeable key was pressed, nothing further to do.
  }

  // Look for keys which don't have UTF-8 codes, like the function keys.  They
  // also are a direct press; they don't try to fiddle with the shift keys.

  KeyCode = 0;
  if (key >= XK_F1 && key <= XK_F12)
    KeyCode = B_F1_KEY + key - XK_F1;
  else if (key == XK_Scroll_Lock)
    KeyCode = B_SCROLL_KEY;
  else if (key == XK_Print)
    KeyCode = B_PRINT_KEY;
  else if (key == XK_Pause)
    KeyCode = B_PAUSE_KEY;

  if (KeyCode != 0)
  {
    SetKeyState (m_LastKeyState, KeyCode, down);
    
    EventMessage.what = down ? B_KEY_DOWN : B_KEY_UP;
    EventMessage.AddInt64 ("when", system_time ());
    EventMessage.AddInt32 ("key", KeyCode);
    EventMessage.AddInt32 ("modifiers", m_LastKeyState.modifiers);
    KeyAsString [0] = B_FUNCTION_KEY;
    KeyAsString [1] = 0;
    EventMessage.AddInt8 ("byte", KeyAsString [0]);
    EventMessage.AddString ("bytes", KeyAsString);
    EventMessage.AddData ("states", B_UINT8_TYPE,
      m_LastKeyState.key_states, 16);
    EventMessage.AddInt32 ("raw_char", KeyAsString [0]);
    m_InputDeviceKeyboardPntr->Control ('ViNC', &EventMessage);
    EventMessage.MakeEmpty ();
    return;
  }

  // The rest of the keys have an equivalent UTF-8 character.  Convert the key
  // code into a UTF-8 character, which will later be used to determine which
  // keys to press.

  KeyToUTF8SearchData.vncKeyCode = key;
  KeyToUTF8Pntr = bsearch (
    (const void *) &KeyToUTF8SearchData,
    (const void *) VNCKeyToUTF8Array,
    sizeof (VNCKeyToUTF8Record),
    sizeof (VNCKeyToUTF8Array) / sizeof (VNCKeyToUTF8Record),
    CompareVNCKeyRecords);
  if (KeyToUTF8Pntr == NULL)
    return; // Key not handled, ignore it.

  bleeble;
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
  // used).  We just need to provide absolute position "x" and "y", which the
  // system will convert to a "where" BPoint, "buttons" for the current button
  // state and the "when" time field.  The system will add "modifiers",
  // "be:transit" and "be:view_where".

  if (EventMessage.what != B_MOUSE_MOVED ||
  m_LastMouseX != AbsoluteX || m_LastMouseY != AbsoluteY ||
  NewMouseButtons != OldMouseButtons)
  {
    EventMessage.AddFloat ("x", AbsoluteX);
    EventMessage.AddFloat ("y", AbsoluteY);
    EventMessage.AddInt64 ("when", system_time ());
    EventMessage.AddInt32 ("buttons", NewMouseButtons);
    m_InputDeviceMousePntr->Control ('ViNC', &EventMessage);
    EventMessage.MakeEmpty ();
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


void SDesktopBeOS::SendUnmappedKeys (
  key_info &OldKeyState,
  key_info &NewKeyState)
{
  int      BitIndex;
  int      ByteIndex;
  uint8    DeltaBit;
  BMessage EventMessage;
  int      KeyCode;
  uint8    Mask;
  uint8    NewBits;
  uint8    OldBits;
  
  KeyCode = 0;
  for (ByteIndex = 0; ByteIndex < 7; ByteIndex++)
  {
    OldBits = OldKeyState.key_states[ByteIndex];
    NewBits = NewKeyState.key_states[ByteIndex];
    for (BitIndex = 7; BitIndex >= 0; BitIndex--, KeyCode++)
    {
      Mask = 1 << BitIndex;
      DeltaBit = (OldBits ^ NewBits) & Mask;
      if (DeltaBit == 0)
        continue; // Key hasn't changed.
      EventMessage.what =
        (NewBits & Mask) ? B_UNMAPPED_KEY_DOWN : B_UNMAPPED_KEY_UP;
      EventMessage.AddInt64 ("when", system_time ());
      EventMessage.AddInt32 ("key", KeyCode);
      EventMessage.AddInt32 ("modifiers", NewKeyState.modifiers);
      EventMessage.AddData ("states",
        B_UINT8_TYPE, &NewKeyState.key_states, 16);
      m_InputDeviceKeyboardPntr->Control ('ViNC', &EventMessage);
      EventMessage.MakeEmpty ();
    }
  }
}



void SDesktopBeOS::setServer (rfb::VNCServer *ServerPntr)
{
  vlog.debug ("setServer called.");
  m_ServerPntr = ServerPntr;
}


void SDesktopBeOS::start (rfb::VNCServer* vs)
{
  vlog.debug ("start called.");

  if (m_FrameBufferBeOSPntr == NULL)
    m_FrameBufferBeOSPntr = new FrameBufferBeOS;
  m_ServerPntr->setPixelBuffer (m_FrameBufferBeOSPntr);

  if (m_InputDeviceKeyboardPntr == NULL)
    m_InputDeviceKeyboardPntr = find_input_device ("VNC Fake Keyboard");
  if (m_InputDeviceMousePntr == NULL)
    m_InputDeviceMousePntr = find_input_device ("VNC Fake Mouse");

  if (m_KeyMapPntr != NULL || m_KeyCharStrings != NULL)
    throw rfb::Exception ("SDesktopBeOS::start: key map pointers not "
    "NULL, bug!");
  get_key_map (&m_KeyMapPntr, &m_KeyCharStrings);
  if (m_KeyMapPntr == NULL || m_KeyCharStrings == NULL)
    throw rfb::Exception ("SDesktopBeOS::start: get_key_map has failed, "
    "so we can't simulate the keyboard buttons being pressed!");
}


void SDesktopBeOS::stop ()
{
  vlog.debug ("stop called.");

  free (m_KeyCharStrings);
  m_KeyCharStrings = NULL;
  free (m_KeyMapPntr);
  m_KeyMapPntr = NULL;

  delete m_FrameBufferBeOSPntr;
  m_FrameBufferBeOSPntr = NULL;
  m_ServerPntr->setPixelBuffer (NULL);

  delete m_InputDeviceKeyboardPntr;
  m_InputDeviceKeyboardPntr = NULL;
  delete m_InputDeviceMousePntr;
  m_InputDeviceMousePntr = NULL;
}


void SDesktopBeOS::UpdateDerivedModifiersAndPressedModifierKeys (
  key_info &KeyState)
{
  uint32 TempModifiers;

  // Update the virtual modifiers, ones which have a left and right key that do
  // the same thing to reflect the state of the real keys.

  TempModifiers = KeyState.modifiers;

  if (TempModifiers & (B_LEFT_SHIFT_KEY | B_RIGHT_SHIFT_KEY))
    TempModifiers |= B_SHIFT_KEY;
  else
    TempModifiers &= ~ (uint32) B_SHIFT_KEY;

  if (TempModifiers & (B_LEFT_COMMAND_KEY | B_RIGHT_COMMAND_KEY))
    TempModifiers |= B_COMMAND_KEY;
  else
    TempModifiers &= ~ (uint32) B_COMMAND_KEY;

  if (TempModifiers & (B_LEFT_CONTROL_KEY | B_RIGHT_CONTROL_KEY))
    TempModifiers |= B_CONTROL_KEY;
  else
    TempModifiers &= ~ (uint32) B_CONTROL_KEY;

  if (TempModifiers & (B_LEFT_OPTION_KEY | B_RIGHT_OPTION_KEY))
    TempModifiers |= B_OPTION_KEY;
  else
    TempModifiers &= ~ (uint32) B_OPTION_KEY;

  KeyState.modifiers = TempModifiers;

  if (m_KeyMapPntr == NULL)
    return; // Can't do anything more without it.

  // Update the pressed keys to reflect the modifiers.  Use the keymap to find
  // the keycode for the actual modifier key.

  SetKeyState (KeyState,
    m_KeyMapPntr->caps_key, TempModifiers & B_CAPS_LOCK);
  SetKeyState (KeyState,
    m_KeyMapPntr->scroll_key, TempModifiers & B_SCROLL_LOCK);
  SetKeyState (KeyState,
    m_KeyMapPntr->num_key, TempModifiers & B_NUM_LOCK);
  SetKeyState (KeyState,
    m_KeyMapPntr->left_shift_key, TempModifiers & B_LEFT_SHIFT_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->right_shift_key, TempModifiers & B_RIGHT_SHIFT_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->left_command_key, TempModifiers & B_LEFT_COMMAND_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->right_command_key, TempModifiers & B_RIGHT_COMMAND_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->left_control_key, TempModifiers & B_LEFT_CONTROL_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->right_control_key, TempModifiers & B_RIGHT_CONTROL_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->left_option_key, TempModifiers & B_LEFT_OPTION_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->right_option_key, TempModifiers & B_RIGHT_OPTION_KEY);
}
