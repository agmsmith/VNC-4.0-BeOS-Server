/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0-beossrc/beosserver/RCS/SDesktopBeOS.cxx,v 1.11 2004/09/13 00:18:27 agmsmith Exp agmsmith $
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
 * Revision 1.11  2004/09/13 00:18:27  agmsmith
 * Do updates separately, only based on the timer running out,
 * so that other events all get processed first before the slow
 * screen update starts.
 *
 * Revision 1.10  2004/08/23 00:51:59  agmsmith
 * Force an update shortly after a key press.
 *
 * Revision 1.9  2004/08/23 00:24:17  agmsmith
 * Added a search for plain keyboard keys, so now you can type text
 * over VNC!  But funny key combinations likely won't work.
 *
 * Revision 1.8  2004/08/22 21:15:38  agmsmith
 * Keyboard work continues, adding the Latin-1 character set.
 *
 * Revision 1.7  2004/08/02 22:00:35  agmsmith
 * Got nonprinting keys working, next up is UTF-8 generating keys.
 *
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
// (like shift) are handled separately.

typedef struct VNCKeyToUTF8Struct
{
  uint16 vncKeyCode;
  char  *utf8String;
} VNCKeyToUTF8Record, *VNCKeyToUTF8Pointer;

extern "C" int CompareVNCKeyRecords (const void *APntr, const void *BPntr)
{
  int Result;

  Result = ((VNCKeyToUTF8Pointer) APntr)->vncKeyCode;
  Result -= ((VNCKeyToUTF8Pointer) BPntr)->vncKeyCode;
  return Result;
}

static VNCKeyToUTF8Record VNCKeyToUTF8Array [] =
{ // Note that this table is in increasing order of VNC key code,
  // so that a binary search can be done.
  {/* 0x0020 */ XK_space, " "},
  {/* 0x0021 */ XK_exclam, "!"},
  {/* 0x0022 */ XK_quotedbl, "\""},
  {/* 0x0023 */ XK_numbersign, "#"},
  {/* 0x0024 */ XK_dollar, "$"},
  {/* 0x0025 */ XK_percent, "%"},
  {/* 0x0026 */ XK_ampersand, "&"},
  {/* 0x0027 */ XK_apostrophe, "'"},
  {/* 0x0028 */ XK_parenleft, "("},
  {/* 0x0029 */ XK_parenright, ")"},
  {/* 0x002a */ XK_asterisk, "*"},
  {/* 0x002b */ XK_plus, "+"},
  {/* 0x002c */ XK_comma, ","},
  {/* 0x002d */ XK_minus, "-"},
  {/* 0x002e */ XK_period, "."},
  {/* 0x002f */ XK_slash, "/"},
  {/* 0x0030 */ XK_0, "0"},
  {/* 0x0031 */ XK_1, "1"},
  {/* 0x0032 */ XK_2, "2"},
  {/* 0x0033 */ XK_3, "3"},
  {/* 0x0034 */ XK_4, "4"},
  {/* 0x0035 */ XK_5, "5"},
  {/* 0x0036 */ XK_6, "6"},
  {/* 0x0037 */ XK_7, "7"},
  {/* 0x0038 */ XK_8, "8"},
  {/* 0x0039 */ XK_9, "9"},
  {/* 0x003a */ XK_colon, ":"},
  {/* 0x003b */ XK_semicolon, ";"},
  {/* 0x003c */ XK_less, "<"},
  {/* 0x003d */ XK_equal, "="},
  {/* 0x003e */ XK_greater, ">"},
  {/* 0x003f */ XK_question, "?"},
  {/* 0x0040 */ XK_at, "@"},
  {/* 0x0041 */ XK_A, "A"},
  {/* 0x0042 */ XK_B, "B"},
  {/* 0x0043 */ XK_C, "C"},
  {/* 0x0044 */ XK_D, "D"},
  {/* 0x0045 */ XK_E, "E"},
  {/* 0x0046 */ XK_F, "F"},
  {/* 0x0047 */ XK_G, "G"},
  {/* 0x0048 */ XK_H, "H"},
  {/* 0x0049 */ XK_I, "I"},
  {/* 0x004a */ XK_J, "J"},
  {/* 0x004b */ XK_K, "K"},
  {/* 0x004c */ XK_L, "L"},
  {/* 0x004d */ XK_M, "M"},
  {/* 0x004e */ XK_N, "N"},
  {/* 0x004f */ XK_O, "O"},
  {/* 0x0050 */ XK_P, "P"},
  {/* 0x0051 */ XK_Q, "Q"},
  {/* 0x0052 */ XK_R, "R"},
  {/* 0x0053 */ XK_S, "S"},
  {/* 0x0054 */ XK_T, "T"},
  {/* 0x0055 */ XK_U, "U"},
  {/* 0x0056 */ XK_V, "V"},
  {/* 0x0057 */ XK_W, "W"},
  {/* 0x0058 */ XK_X, "X"},
  {/* 0x0059 */ XK_Y, "Y"},
  {/* 0x005a */ XK_Z, "Z"},
  {/* 0x005b */ XK_bracketleft, "["},
  {/* 0x005c */ XK_backslash, "\\"},
  {/* 0x005d */ XK_bracketright, "]"},
  {/* 0x005e */ XK_asciicircum, "^"},
  {/* 0x005f */ XK_underscore, "_"},
  {/* 0x0060 */ XK_grave, "`"},
  {/* 0x0061 */ XK_a, "a"},
  {/* 0x0062 */ XK_b, "b"},
  {/* 0x0063 */ XK_c, "c"},
  {/* 0x0064 */ XK_d, "d"},
  {/* 0x0065 */ XK_e, "e"},
  {/* 0x0066 */ XK_f, "f"},
  {/* 0x0067 */ XK_g, "g"},
  {/* 0x0068 */ XK_h, "h"},
  {/* 0x0069 */ XK_i, "i"},
  {/* 0x006a */ XK_j, "j"},
  {/* 0x006b */ XK_k, "k"},
  {/* 0x006c */ XK_l, "l"},
  {/* 0x006d */ XK_m, "m"},
  {/* 0x006e */ XK_n, "n"},
  {/* 0x006f */ XK_o, "o"},
  {/* 0x0070 */ XK_p, "p"},
  {/* 0x0071 */ XK_q, "q"},
  {/* 0x0072 */ XK_r, "r"},
  {/* 0x0073 */ XK_s, "s"},
  {/* 0x0074 */ XK_t, "t"},
  {/* 0x0075 */ XK_u, "u"},
  {/* 0x0076 */ XK_v, "v"},
  {/* 0x0077 */ XK_w, "w"},
  {/* 0x0078 */ XK_x, "x"},
  {/* 0x0079 */ XK_y, "y"},
  {/* 0x007a */ XK_z, "z"},
  {/* 0x007b */ XK_braceleft, "{"},
  {/* 0x007c */ XK_bar, "|"},
  {/* 0x007d */ XK_braceright, "}"},
  {/* 0x007e */ XK_asciitilde, "~"},
  {/* 0x00a0 */ XK_nobreakspace, " "}, // If compiler barfs use: "\0xc2\0xa0"
  {/* 0x00a1 */ XK_exclamdown, "¡"},
  {/* 0x00a2 */ XK_cent, "¢"},
  {/* 0x00a3 */ XK_sterling, "£"},
  {/* 0x00a4 */ XK_currency, "¤"},
  {/* 0x00a5 */ XK_yen, "¥"},
  {/* 0x00a6 */ XK_brokenbar, "¦"},
  {/* 0x00a7 */ XK_section, "§"},
  {/* 0x00a8 */ XK_diaeresis, "¨"},
  {/* 0x00a9 */ XK_copyright, "©"},
  {/* 0x00aa */ XK_ordfeminine, "ª"},
  {/* 0x00ab */ XK_guillemotleft, "«"},
  {/* 0x00ac */ XK_notsign, "¬"},
  {/* 0x00ad */ XK_hyphen, "­"},
  {/* 0x00ae */ XK_registered, "®"},
  {/* 0x00af */ XK_macron, "¯"},
  {/* 0x00b0 */ XK_degree, "°"},
  {/* 0x00b1 */ XK_plusminus, "±"},
  {/* 0x00b2 */ XK_twosuperior, "²"},
  {/* 0x00b3 */ XK_threesuperior, "³"},
  {/* 0x00b4 */ XK_acute, "´"},
  {/* 0x00b5 */ XK_mu, "µ"},
  {/* 0x00b6 */ XK_paragraph, "¶"},
  {/* 0x00b7 */ XK_periodcentered, "·"},
  {/* 0x00b8 */ XK_cedilla, "¸"},
  {/* 0x00b9 */ XK_onesuperior, "¹"},
  {/* 0x00ba */ XK_masculine, "º"},
  {/* 0x00bb */ XK_guillemotright, "»"},
  {/* 0x00bc */ XK_onequarter, "¼"},
  {/* 0x00bd */ XK_onehalf, "½"},
  {/* 0x00be */ XK_threequarters, "¾"},
  {/* 0x00bf */ XK_questiondown, "¿"},
  {/* 0x00c0 */ XK_Agrave, "À"},
  {/* 0x00c1 */ XK_Aacute, "Á"},
  {/* 0x00c2 */ XK_Acircumflex, "Â"},
  {/* 0x00c3 */ XK_Atilde, "Ã"},
  {/* 0x00c4 */ XK_Adiaeresis, "Ä"},
  {/* 0x00c5 */ XK_Aring, "Å"},
  {/* 0x00c6 */ XK_AE, "Æ"},
  {/* 0x00c7 */ XK_Ccedilla, "Ç"},
  {/* 0x00c8 */ XK_Egrave, "È"},
  {/* 0x00c9 */ XK_Eacute, "É"},
  {/* 0x00ca */ XK_Ecircumflex, "Ê"},
  {/* 0x00cb */ XK_Ediaeresis, "Ë"},
  {/* 0x00cc */ XK_Igrave, "Ì"},
  {/* 0x00cd */ XK_Iacute, "Í"},
  {/* 0x00ce */ XK_Icircumflex, "Î"},
  {/* 0x00cf */ XK_Idiaeresis, "Ï"},
  {/* 0x00d0 */ XK_ETH, "Ð"},
  {/* 0x00d1 */ XK_Ntilde, "Ñ"},
  {/* 0x00d2 */ XK_Ograve, "Ò"},
  {/* 0x00d3 */ XK_Oacute, "Ó"},
  {/* 0x00d4 */ XK_Ocircumflex, "Ô"},
  {/* 0x00d5 */ XK_Otilde, "Õ"},
  {/* 0x00d6 */ XK_Odiaeresis, "Ö"},
  {/* 0x00d7 */ XK_multiply, "×"},
  {/* 0x00d8 */ XK_Ooblique, "Ø"},
  {/* 0x00d9 */ XK_Ugrave, "Ù"},
  {/* 0x00da */ XK_Uacute, "Ú"},
  {/* 0x00db */ XK_Ucircumflex, "Û"},
  {/* 0x00dc */ XK_Udiaeresis, "Ü"},
  {/* 0x00dd */ XK_Yacute, "Ý"},
  {/* 0x00de */ XK_THORN, "Þ"},
  {/* 0x00df */ XK_ssharp, "ß"},
  {/* 0x00e0 */ XK_agrave, "à"},
  {/* 0x00e1 */ XK_aacute, "á"},
  {/* 0x00e2 */ XK_acircumflex, "â"},
  {/* 0x00e3 */ XK_atilde, "ã"},
  {/* 0x00e4 */ XK_adiaeresis, "ä"},
  {/* 0x00e5 */ XK_aring, "å"},
  {/* 0x00e6 */ XK_ae, "æ"},
  {/* 0x00e7 */ XK_ccedilla, "ç"},
  {/* 0x00e8 */ XK_egrave, "è"},
  {/* 0x00e9 */ XK_eacute, "é"},
  {/* 0x00ea */ XK_ecircumflex, "ê"},
  {/* 0x00eb */ XK_ediaeresis, "ë"},
  {/* 0x00ec */ XK_igrave, "ì"},
  {/* 0x00ed */ XK_iacute, "í"},
  {/* 0x00ee */ XK_icircumflex, "î"},
  {/* 0x00ef */ XK_idiaeresis, "ï"},
  {/* 0x00f0 */ XK_eth, "ð"},
  {/* 0x00f1 */ XK_ntilde, "ñ"},
  {/* 0x00f2 */ XK_ograve, "ò"},
  {/* 0x00f3 */ XK_oacute, "ó"},
  {/* 0x00f4 */ XK_ocircumflex, "ô"},
  {/* 0x00f5 */ XK_otilde, "õ"},
  {/* 0x00f6 */ XK_odiaeresis, "ö"},
  {/* 0x00f7 */ XK_division, "÷"},
  {/* 0x00f8 */ XK_oslash, "ø"},
  {/* 0x00f9 */ XK_ugrave, "ù"},
  {/* 0x00fa */ XK_uacute, "ú"},
  {/* 0x00fb */ XK_ucircumflex, "û"},
  {/* 0x00fc */ XK_udiaeresis, "ü"},
  {/* 0x00fd */ XK_yacute, "ý"},
  {/* 0x00fe */ XK_thorn, "þ"},
  {/* 0x00ff */ XK_ydiaeresis, "ÿ"},
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
  {/* 0xFFFF */ XK_Delete, UTF8_Delete}
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


void SDesktopBeOS::DoScreenUpdate ()
{
  rfb::PixelFormat NewScreenFormat;
  rfb::PixelFormat OldScreenFormat;
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

      // Tell the server to resend the changed areas, by telling it to examine
      // everything except the corner of the screen with the changing number.

      rfb::Rect RectangleChanged (0, 0,
        m_FrameBufferBeOSPntr->width (), m_FrameBufferBeOSPntr->height ());
      rfb::Region RegionChanged (RectangleChanged);
      m_ServerPntr->add_changed (RegionChanged);
      m_ServerPntr->tryUpdate ();
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

    m_NextForcedUpdateTime = system_time () + 20000000;
  }
}


uint8 SDesktopBeOS::FindKeyCodeFromMap (
  int32 *MapOffsetArray,
  char *KeyAsString)
{
  unsigned int KeyCode;
  int32       *OffsetPntr;
  char        *StringPntr;

  OffsetPntr = MapOffsetArray + 1 /* Skip over [0]. */;
  for (KeyCode = 1 /* zero not valid */; KeyCode <= 127;
  KeyCode++, OffsetPntr++)
  {
    StringPntr = m_KeyCharStrings + *OffsetPntr;
    if (*StringPntr == 0)
      continue; // Length of string (pascal style with length byte) is zero.
    if (memcmp (StringPntr + 1, KeyAsString, *StringPntr) == 0 &&
    KeyAsString [*StringPntr] == 0 /* look for NUL at end of search string */)
      return KeyCode;
  }
  return 0;
}


void SDesktopBeOS::forcedUpdateCheck ()
{
  if (system_time () > m_NextForcedUpdateTime)
    DoScreenUpdate ();
}


void SDesktopBeOS::framebufferUpdateRequest ()
{
  m_NextForcedUpdateTime = system_time () + 1000000;
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
  char                ControlCharacterAsUTF8 [2];
  BMessage            EventMessage;
  uint8               KeyCode;
  char                KeyAsString [16];
  VNCKeyToUTF8Record  KeyToUTF8SearchData;
  VNCKeyToUTF8Pointer KeyToUTF8Pntr;
  key_info            NewKeyState;

  if (m_InputDeviceKeyboardPntr == NULL || m_FrameBufferBeOSPntr == NULL ||
  m_FrameBufferBeOSPntr->width () <= 0 || m_KeyMapPntr == NULL)
    return;

  vlog.debug ("VNC keycode $%04X received, key is %s.",
    key, down ? "down" : "up");

  // Hack - force a screen update a short time after a key is pressed, since it
  // is likely that the screen will have changed a bit.

  m_NextForcedUpdateTime = system_time () + 300000;

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
      // key message since that's what BeOS does.

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
    KeyCode = m_KeyMapPntr->scroll_key; // Usually equals B_SCROLL_KEY.
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

  // Special case for control keys.  If control is down (but Command isn't,
  // since if Command is down then control is ignored by BeOS) then convert the
  // VNC keycodes into control characters.  That's because VNC sends control-A
  // as the same code for the letter A.

  KeyToUTF8Pntr = NULL;
  if ((m_LastKeyState.modifiers & B_COMMAND_KEY) == 0 &&
  (m_LastKeyState.modifiers & B_CONTROL_KEY) != 0 &&
  key >= 0x40 /* @ sign */ && key <= 0x7f /* Del key */)
  {
    KeyToUTF8SearchData.vncKeyCode = key;
    ControlCharacterAsUTF8 [0] = (key & 31);
    ControlCharacterAsUTF8 [1] = 0;
    KeyToUTF8SearchData.utf8String = ControlCharacterAsUTF8;
    KeyToUTF8Pntr = &KeyToUTF8SearchData;
  }
  else
  {
    // The rest of the keys have an equivalent UTF-8 character.  Convert the
    // key code into a UTF-8 character, which will later be used to determine
    // which keys to press.
  
    KeyToUTF8SearchData.vncKeyCode = key;
    KeyToUTF8Pntr = (VNCKeyToUTF8Pointer) bsearch (
      &KeyToUTF8SearchData,
      VNCKeyToUTF8Array,
      sizeof (VNCKeyToUTF8Array) / sizeof (VNCKeyToUTF8Record),
      sizeof (VNCKeyToUTF8Record),
      CompareVNCKeyRecords);
  }
  if (KeyToUTF8Pntr == NULL)
  {
    vlog.info ("VNC keycode $%04X (%s) isn't recognised, ignoring it.",
      key, down ? "down" : "up");
    return; // Key not handled, ignore it.
  }

  // Look for the UTF-8 string in the keymap tables which are currently active
  // (reflecting the current modifier keys) to see which key code it is.

  strcpy (KeyAsString, KeyToUTF8Pntr->utf8String);
  KeyCode = 0;
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_CONTROL_KEY) &&
  /* Can't type control characters while the command key is down */
  !(m_LastKeyState.modifiers & B_COMMAND_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->control_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_OPTION_KEY) &&
  (m_LastKeyState.modifiers & B_CAPS_LOCK) &&
  (m_LastKeyState.modifiers & B_SHIFT_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->option_caps_shift_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_OPTION_KEY) &&
  (m_LastKeyState.modifiers & B_CAPS_LOCK))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->option_caps_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_OPTION_KEY) &&
  (m_LastKeyState.modifiers & B_SHIFT_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->option_shift_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_OPTION_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->option_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_CAPS_LOCK) &&
  (m_LastKeyState.modifiers & B_SHIFT_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->caps_shift_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_CAPS_LOCK))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->caps_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_SHIFT_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->shift_map, KeyAsString);
  if (KeyCode == 0)
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->normal_map, KeyAsString);

  if (KeyCode != 0)
  {
    // Got a key that works with the current modifier settings.
    // Just simulate pressing it.

    SetKeyState (m_LastKeyState, KeyCode, down);

    EventMessage.what = down ? B_KEY_DOWN : B_KEY_UP;
    EventMessage.AddInt64 ("when", system_time ());
    EventMessage.AddInt32 ("key", KeyCode);
    EventMessage.AddInt32 ("modifiers", m_LastKeyState.modifiers);
    EventMessage.AddInt8 ("byte", KeyAsString [0]);
    EventMessage.AddString ("bytes", KeyAsString);
    EventMessage.AddData ("states", B_UINT8_TYPE,
      m_LastKeyState.key_states, 16);
    EventMessage.AddInt32 ("raw_char", key & 0xFF); // Could be wrong.
    m_InputDeviceKeyboardPntr->Control ('ViNC', &EventMessage);
    EventMessage.MakeEmpty ();
    return;
  }

  // The key isn't an obvious one.  Check all the keymap tables and simulate
  // pressing shift keys etc. to get that code, then send the message for that
  // key pressed, then release the temporary modifier key changes.  Well,
  // that's a lot of work so it will be done later if needed.  I won't even
  // write much about dead keys.

  vlog.debug ("VNC keycode $%04X (%s) maps to \"%s\", but it isn't obvious "
    "which BeOS keys need to be \"pressed\" to achieve that.  Ignoring it.",
    key, down ? "down" : "up", KeyAsString);
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

  NewMouseButtons = (buttonmask & 0x18); // Just examine mouse wheel "buttons".
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
