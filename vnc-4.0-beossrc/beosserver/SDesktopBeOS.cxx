/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0-beossrc/beosserver/RCS/SDesktopBeOS.cxx,v 1.37 2015/09/06 16:52:54 agmsmith Exp agmsmith $
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
 * Copyright (C) 2004 by Alexander G. M. Smith.
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
 * Revision 1.37  2015/09/06 16:52:54  agmsmith
 * Change background content refresh to be every half second.  That way when
 * the user scrubbs the mouse, they'll see fairly recent content, even with
 * the slow BScreen capture technique.  Formerly you'd have to wait for a
 * full screen refresh before it would grab the actual screen contents.
 *
 * Revision 1.36  2015/09/04 23:55:59  agmsmith
 * On fast computers grab the screen contents on every sliver update,
 * not just at the start of every full screen update.  Lets you scrub
 * the screen with the mouse just like you can with the direct access
 * to video memory technique.  Makes VNC nicer on slow connections to
 * a fast virtual machine.
 *
 * Revision 1.35  2013/04/24 18:34:27  agmsmith
 * Fix PPC version cheap cursor graphics being mangled due to endienness.
 *
 * Revision 1.34  2013/04/24 15:19:15  agmsmith
 * Fix PPC compile with extra function declaration.
 *
 * Revision 1.33  2013/04/23 19:36:04  agmsmith
 * Updated types to compile with GCC 4.6.3, mostly const and some type changes.
 *
 * Revision 1.32  2013/02/19 21:07:21  agmsmith
 * Fixed bug in unmapped key codes with code number wrong due to
 * loop optimisation that skipped updating the code.
 *
 * Revision 1.31  2013/02/19 03:21:52  agmsmith
 * Doc wording.
 *
 * Revision 1.30  2013/02/18 17:30:00  agmsmith
 * Now sends fake shift and other modifier key presses to enable typing of letters
 * that are sent without the correct shift codes.  Like capital A on the iPad VNC
 * that sends it without any shift keys being pressed before it.  Also lets the
 * iPad send fancy codes like the currency symbols.  Added Euro to the table,
 * though BeOS doesn't have a pressable key for it.  Also fixed several bugs
 * with sending messages - wasn't sending some modifier key presses due to a
 * bug, also sends the Bytes part of the message properly (it can be more than
 * one byte for UTF-8 multibyte strings) and also send raw_char as the keycode for
 * the unmodified key corresponding to the key being pressed.
 *
 * Revision 1.29  2013/02/12 22:18:33  agmsmith
 * Add a gradient bitmap for when no screen buffer is available,
 * add a timeout to locking the frame buffer so that Haiku can
 * work with its 0.5 second processing time limit.
 *
 * Revision 1.28  2013/02/12 18:59:11  agmsmith
 * Reduce priority of debug message for update size changes.
 *
 * Revision 1.27  2011/11/13 16:24:04  agmsmith
 * Add a spot of white to the cursor.
 *
 * Revision 1.26  2011/11/11 21:57:54  agmsmith
 * Changed the cheap cursor to be twice as big and have a bit of colour,
 * so you can see it more easily on the iPad version of VNC.
 *
 * Revision 1.25  2005/02/14 02:31:01  agmsmith
 * Added an option to turn off various screen scanning methods,
 * and an option and code to draw a cursor in the outgoing data.
 *
 * Revision 1.24  2005/02/13 01:42:05  agmsmith
 * Can now receive clipboard text from the remote clients and
 * put it on the BeOS clipboard.
 *
 * Revision 1.23  2005/02/06 22:03:10  agmsmith
 * Changed to use the new BScreen reading method if the
 * BDirectWindow one doesn't work.  Also removed the screen
 * mode slow change with the yellow bar fake screen.
 *
 * Revision 1.22  2005/01/02 23:57:17  agmsmith
 * Fixed up control keys to avoid using the defective BeOS keyboard
 * mapping, which maps control-D to the End key and other similar
 * problems.
 *
 * Revision 1.21  2005/01/02 21:57:29  agmsmith
 * Made the event injector simpler - only need one device, not
 * separate ones for keyboard and mouse.  Also renamed it to
 * InputEventInjector to be in line with it's more general use.
 *
 * Revision 1.20  2005/01/02 21:05:33  agmsmith
 * Found screen resolution bug - wasn't testing the screen width
 * or height to detect a change, just depth.  Along the way added
 * some cool colour shifting animations on a fake screen.
 *
 * Revision 1.19  2005/01/01 21:31:02  agmsmith
 * Added double click timing detection, so that you can now double
 * click on a window title to minimize it.  Was missing the "clicks"
 * field in mouse down BMessages.
 *
 * Revision 1.18  2005/01/01 20:46:47  agmsmith
 * Make the default control/alt key swap detection be the alt is alt
 * and control is control setting, in case the keyboard isn't a
 * standard USA keyboard layout.
 *
 * Revision 1.17  2005/01/01 20:34:34  agmsmith
 * Swap control and alt keys if the user's keymap has them swapped.
 * Since it uses a physical key code (92) to detect the left control
 * key, it won't work on all keyboards, just US standard.
 *
 * Revision 1.16  2004/12/13 03:57:37  agmsmith
 * Combined functions for doing background update with grabbing the
 * screen memory.  Also limit update size to at least 4 scan lines.
 *
 * Revision 1.15  2004/12/02 02:24:28  agmsmith
 * Check for buggy operating systems which might allow the screen to
 * change memory addresses even while it is locked.
 *
 * Revision 1.14  2004/11/28 00:22:11  agmsmith
 * Also show frame rate.
 *
 * Revision 1.13  2004/11/27 22:53:59  agmsmith
 * Changed update technique to scan a small part of the screen each time
 * so that big updates don't slow down the interactivity by being big.
 * There is also an adaptive algorithm that makes the updates small
 * enough to be quick on the average.
 *
 * Revision 1.12  2004/09/13 01:41:24  agmsmith
 * Trying to get control keys working.
 *
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

/* Posix headers. */

#include <stdlib.h>

/* VNC library headers. */

#include <rfb/PixelBuffer.h>
#include <rfb/LogWriter.h>
#include <rfb/SDesktop.h>

#define XK_MISCELLANY 1
#define XK_LATIN1 1
#define XK_CURRENCY 1
#include <rfb/keysymdef.h>

/* BeOS (Be Operating System) headers. */

#include <Clipboard.h>
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

static rfb::BoolParameter TryBDirectWindow ("ScreenReaderBDirect",
  "Set to TRUE if you want to include the BDirectWindow screen reader "
  "technique in the set of techniques the program attempts to use.  "
  "This one is the recommended one for faster responses since "
  "it maps the video memory into main memory, rather than requiring a "
  "copy operation to read it.  Thus waving the mouse pointer will be able "
  "to update the screen under the mouse with the contents at that instant.  "
  "However, only well supported video boards can use this and ones that "
  "aren't supported will just fail the startup test and not use this "
  "technique even if you ask for it.",
  1);

static rfb::BoolParameter TryBScreen ("ScreenReaderBScreen",
  "Set to TRUE if you want to include the BScreen based screen reader "
  "technique in the set of techniques the program attempts to use.  "
  "This one is compatible with all video boards, but much slower since "
  "it copies the whole screen to a bitmap before starting to work.  This "
  "also makes small updates useless - waving the mouse pointer won't "
  "reveal anything new until the next full screen update.  One advantage "
  "is that if you boot up without a good video driver, it does show the "
  "mouse cursor, since the cursor is actually drawn into the frame buffer "
  "by BeOS rather than using a hardware sprite.",
  1);

static rfb::BoolParameter ShowCheapCursor ("ShowCheapCursor",
  "If you want to see a marker on the screen where the mouse may be, turn on "
  "this parameter.  The shape (a blue/black/white cross large enough to be "
  "visible on an iPad) gets sent to the client side, which will hopefully "
  "display it instead of your usual mouse cursor or no cursor.  If there "
  "was a way of getting the current BeOS cursor shape, we'd use that.  "
  "Alternatively you can try booting in safe video mode, then the cursor is "
  "drawn into the screen bitmap rather than done as a separate hardware "
  "cursor, but it is very slow.",
  0);


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


// This table is used for converting VNC key codes into UTF-8 characters, for
// printable characters.  Function keys and modifier keys (like shift) are
// handled separately.

typedef struct VNCKeyToUTF8Struct
{
  uint16 vncKeyCode;
  uint16 suggestedBeOSKeyCode;
  const char *utf8String;
} VNCKeyToUTF8Record, *VNCKeyToUTF8Pointer;

// Redundant declaration needed for PPC compiler.
extern "C" int CompareVNCKeyRecords (const void *APntr, const void *BPntr);
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
  {/* 0x0020 */ XK_space, 94, " "},
  {/* 0x0021 */ XK_exclam, 18, "!"},
  {/* 0x0022 */ XK_quotedbl, 70, "\""},
  {/* 0x0023 */ XK_numbersign, 20, "#"},
  {/* 0x0024 */ XK_dollar, 21, "$"},
  {/* 0x0025 */ XK_percent, 22, "%"},
  {/* 0x0026 */ XK_ampersand, 24, "&"},
  {/* 0x0027 */ XK_apostrophe, 70, "'"},
  {/* 0x0028 */ XK_parenleft, 26, "("},
  {/* 0x0029 */ XK_parenright, 27, ")"},
  {/* 0x002a */ XK_asterisk, 25, "*"},
  {/* 0x002b */ XK_plus, 29, "+"},
  {/* 0x002c */ XK_comma, 83, ","},
  {/* 0x002d */ XK_minus, 28, "-"},
  {/* 0x002e */ XK_period, 84, "."},
  {/* 0x002f */ XK_slash, 85, "/"},
  {/* 0x0030 */ XK_0, 27, "0"},
  {/* 0x0031 */ XK_1, 18, "1"},
  {/* 0x0032 */ XK_2, 19, "2"},
  {/* 0x0033 */ XK_3, 20, "3"},
  {/* 0x0034 */ XK_4, 21, "4"},
  {/* 0x0035 */ XK_5, 22, "5"},
  {/* 0x0036 */ XK_6, 23, "6"},
  {/* 0x0037 */ XK_7, 24, "7"},
  {/* 0x0038 */ XK_8, 25, "8"},
  {/* 0x0039 */ XK_9, 26, "9"},
  {/* 0x003a */ XK_colon, 69, ":"},
  {/* 0x003b */ XK_semicolon, 69, ";"},
  {/* 0x003c */ XK_less, 83, "<"},
  {/* 0x003d */ XK_equal, 29, "="},
  {/* 0x003e */ XK_greater, 84, ">"},
  {/* 0x003f */ XK_question, 85, "?"},
  {/* 0x0040 */ XK_at, 19, "@"},
  {/* 0x0041 */ XK_A, 60, "A"},
  {/* 0x0042 */ XK_B, 80, "B"},
  {/* 0x0043 */ XK_C, 78, "C"},
  {/* 0x0044 */ XK_D, 62, "D"},
  {/* 0x0045 */ XK_E, 41, "E"},
  {/* 0x0046 */ XK_F, 63, "F"},
  {/* 0x0047 */ XK_G, 64, "G"},
  {/* 0x0048 */ XK_H, 65, "H"},
  {/* 0x0049 */ XK_I, 46, "I"},
  {/* 0x004a */ XK_J, 66, "J"},
  {/* 0x004b */ XK_K, 67, "K"},
  {/* 0x004c */ XK_L, 68, "L"},
  {/* 0x004d */ XK_M, 82, "M"},
  {/* 0x004e */ XK_N, 81, "N"},
  {/* 0x004f */ XK_O, 47, "O"},
  {/* 0x0050 */ XK_P, 48, "P"},
  {/* 0x0051 */ XK_Q, 39, "Q"},
  {/* 0x0052 */ XK_R, 42, "R"},
  {/* 0x0053 */ XK_S, 61, "S"},
  {/* 0x0054 */ XK_T, 43, "T"},
  {/* 0x0055 */ XK_U, 45, "U"},
  {/* 0x0056 */ XK_V, 79, "V"},
  {/* 0x0057 */ XK_W, 40, "W"},
  {/* 0x0058 */ XK_X, 77, "X"},
  {/* 0x0059 */ XK_Y, 44, "Y"},
  {/* 0x005a */ XK_Z, 76, "Z"},
  {/* 0x005b */ XK_bracketleft, 49, "["},
  {/* 0x005c */ XK_backslash, 51, "\\"},
  {/* 0x005d */ XK_bracketright, 50, "]"},
  {/* 0x005e */ XK_asciicircum, 23, "^"},
  {/* 0x005f */ XK_underscore, 28, "_"},
  {/* 0x0060 */ XK_grave, 17, "`"},
  {/* 0x0061 */ XK_a, 60, "a"},
  {/* 0x0062 */ XK_b, 80, "b"},
  {/* 0x0063 */ XK_c, 78, "c"},
  {/* 0x0064 */ XK_d, 62, "d"},
  {/* 0x0065 */ XK_e, 41, "e"},
  {/* 0x0066 */ XK_f, 63, "f"},
  {/* 0x0067 */ XK_g, 64, "g"},
  {/* 0x0068 */ XK_h, 65, "h"},
  {/* 0x0069 */ XK_i, 46, "i"},
  {/* 0x006a */ XK_j, 66, "j"},
  {/* 0x006b */ XK_k, 67, "k"},
  {/* 0x006c */ XK_l, 68, "l"},
  {/* 0x006d */ XK_m, 82, "m"},
  {/* 0x006e */ XK_n, 81, "n"},
  {/* 0x006f */ XK_o, 47, "o"},
  {/* 0x0070 */ XK_p, 48, "p"},
  {/* 0x0071 */ XK_q, 39, "q"},
  {/* 0x0072 */ XK_r, 42, "r"},
  {/* 0x0073 */ XK_s, 61, "s"},
  {/* 0x0074 */ XK_t, 43, "t"},
  {/* 0x0075 */ XK_u, 45, "u"},
  {/* 0x0076 */ XK_v, 79, "v"},
  {/* 0x0077 */ XK_w, 40, "w"},
  {/* 0x0078 */ XK_x, 77, "x"},
  {/* 0x0079 */ XK_y, 44, "y"},
  {/* 0x007a */ XK_z, 76, "z"},
  {/* 0x007b */ XK_braceleft, 49, "{"},
  {/* 0x007c */ XK_bar, 51, "|"},
  {/* 0x007d */ XK_braceright, 50, "}"},
  {/* 0x007e */ XK_asciitilde, 17, "~"},
  {/* 0x00a0 */ XK_nobreakspace, 94, " "}, // If compiler barfs use: "\0xc2\0xa0"
  {/* 0x00a1 */ XK_exclamdown, 18, "¡"},
  {/* 0x00a2 */ XK_cent, 21, "¢"},
  {/* 0x00a3 */ XK_sterling, 20, "£"},
  {/* 0x00a4 */ XK_currency, 0, "¤"},
  {/* 0x00a5 */ XK_yen, 44, "¥"},
  {/* 0x00a6 */ XK_brokenbar, 0, "¦"},
  {/* 0x00a7 */ XK_section, 24, "§"},
  {/* 0x00a8 */ XK_diaeresis, 69, "¨"},
  {/* 0x00a9 */ XK_copyright, 64, "©"},
  {/* 0x00aa */ XK_ordfeminine, 26, "ª"},
  {/* 0x00ab */ XK_guillemotleft, 26, "«"},
  {/* 0x00ac */ XK_notsign, 51, "¬"},
  {/* 0x00ad */ XK_hyphen, 0, "­"},
  {/* 0x00ae */ XK_registered, 42, "®"},
  {/* 0x00af */ XK_macron, 0, "¯"},
  {/* 0x00b0 */ XK_degree, 25, "°"},
  {/* 0x00b1 */ XK_plusminus, 29, "±"},
  {/* 0x00b2 */ XK_twosuperior, 0, "²"},
  {/* 0x00b3 */ XK_threesuperior, 0, "³"},
  {/* 0x00b4 */ XK_acute, 41, "´"},
  {/* 0x00b5 */ XK_mu, 82, "µ"},
  {/* 0x00b6 */ XK_paragraph, 24, "¶"},
  {/* 0x00b7 */ XK_periodcentered, 0, "·"},
  {/* 0x00b8 */ XK_cedilla, 0, "¸"},
  {/* 0x00b9 */ XK_onesuperior, 0, "¹"},
  {/* 0x00ba */ XK_masculine, 27, "º"},
  {/* 0x00bb */ XK_guillemotright, 126, "»"},
  {/* 0x00bc */ XK_onequarter, 0, "¼"},
  {/* 0x00bd */ XK_onehalf, 0, "½"},
  {/* 0x00be */ XK_threequarters, 0, "¾"},
  {/* 0x00bf */ XK_questiondown, 85, "¿"},
  {/* 0x00c0 */ XK_Agrave, 60, "À"},
  {/* 0x00c1 */ XK_Aacute, 0, "Á"},
  {/* 0x00c2 */ XK_Acircumflex, 0, "Â"},
  {/* 0x00c3 */ XK_Atilde, 0, "Ã"},
  {/* 0x00c4 */ XK_Adiaeresis, 0, "Ä"},
  {/* 0x00c5 */ XK_Aring, 60, "Å"},
  {/* 0x00c6 */ XK_AE, 68, "Æ"},
  {/* 0x00c7 */ XK_Ccedilla, 78, "Ç"},
  {/* 0x00c8 */ XK_Egrave, 0, "È"},
  {/* 0x00c9 */ XK_Eacute, 0, "É"},
  {/* 0x00ca */ XK_Ecircumflex, 0, "Ê"},
  {/* 0x00cb */ XK_Ediaeresis, 0, "Ë"},
  {/* 0x00cc */ XK_Igrave, 0, "Ì"},
  {/* 0x00cd */ XK_Iacute, 0, "Í"},
  {/* 0x00ce */ XK_Icircumflex, 0, "Î"},
  {/* 0x00cf */ XK_Idiaeresis, 0, "Ï"},
  {/* 0x00d0 */ XK_ETH, 0, "Ð"},
  {/* 0x00d1 */ XK_Ntilde, 81, "Ñ"},
  {/* 0x00d2 */ XK_Ograve, 0, "Ò"},
  {/* 0x00d3 */ XK_Oacute, 0, "Ó"},
  {/* 0x00d4 */ XK_Ocircumflex, 0, "Ô"},
  {/* 0x00d5 */ XK_Otilde, 0, "Õ"},
  {/* 0x00d6 */ XK_Odiaeresis, 0, "Ö"},
  {/* 0x00d7 */ XK_multiply, 0, "×"},
  {/* 0x00d8 */ XK_Ooblique, 47, "Ø"},
  {/* 0x00d9 */ XK_Ugrave, 0, "Ù"},
  {/* 0x00da */ XK_Uacute, 0, "Ú"},
  {/* 0x00db */ XK_Ucircumflex, 0, "Û"},
  {/* 0x00dc */ XK_Udiaeresis, 0, "Ü"},
  {/* 0x00dd */ XK_Yacute, 0, "Ý"},
  {/* 0x00de */ XK_THORN, 0, "Þ"},
  {/* 0x00df */ XK_ssharp, 80, "ß"},
  {/* 0x00e0 */ XK_agrave, 0, "à"},
  {/* 0x00e1 */ XK_aacute, 0, "á"},
  {/* 0x00e2 */ XK_acircumflex, 0, "â"},
  {/* 0x00e3 */ XK_atilde, 0, "ã"},
  {/* 0x00e4 */ XK_adiaeresis, 0, "ä"},
  {/* 0x00e5 */ XK_aring, 60, "å"},
  {/* 0x00e6 */ XK_ae, 68, "æ"},
  {/* 0x00e7 */ XK_ccedilla, 78, "ç"},
  {/* 0x00e8 */ XK_egrave, 0, "è"},
  {/* 0x00e9 */ XK_eacute, 0, "é"},
  {/* 0x00ea */ XK_ecircumflex, 0, "ê"},
  {/* 0x00eb */ XK_ediaeresis, 0, "ë"},
  {/* 0x00ec */ XK_igrave, 0, "ì"},
  {/* 0x00ed */ XK_iacute, 0, "í"},
  {/* 0x00ee */ XK_icircumflex, 0, "î"},
  {/* 0x00ef */ XK_idiaeresis, 0, "ï"},
  {/* 0x00f0 */ XK_eth, 0, "ð"},
  {/* 0x00f1 */ XK_ntilde, 81, "ñ"},
  {/* 0x00f2 */ XK_ograve, 0, "ò"},
  {/* 0x00f3 */ XK_oacute, 0, "ó"},
  {/* 0x00f4 */ XK_ocircumflex, 0, "ô"},
  {/* 0x00f5 */ XK_otilde, 0, "õ"},
  {/* 0x00f6 */ XK_odiaeresis, 0, "ö"},
  {/* 0x00f7 */ XK_division, 85, "÷"},
  {/* 0x00f8 */ XK_oslash, 47, "ø"},
  {/* 0x00f9 */ XK_ugrave, 0, "ù"},
  {/* 0x00fa */ XK_uacute, 0, "ú"},
  {/* 0x00fb */ XK_ucircumflex, 0, "û"},
  {/* 0x00fc */ XK_udiaeresis, 0, "ü"},
  {/* 0x00fd */ XK_yacute, 0, "ý"},
  {/* 0x00fe */ XK_thorn, 0, "þ"},
  {/* 0x00ff */ XK_ydiaeresis, 0, "ÿ"},
  {/* 0x20ac */ XK_EuroSign, 0, "€"},
  {/* 0xFF08 */ XK_BackSpace, 30, UTF8_Backspace},
  {/* 0xFF09 */ XK_Tab, 38, UTF8_Tab},
  {/* 0xFF0D */ XK_Return, 71, UTF8_Return},
  {/* 0xFF1B */ XK_Escape, 1, UTF8_Escape},
  {/* 0xFF50 */ XK_Home, 32, UTF8_Home},
  {/* 0xFF51 */ XK_Left, 97, UTF8_LeftArrow},
  {/* 0xFF52 */ XK_Up, 87, UTF8_UpArrow},
  {/* 0xFF53 */ XK_Right, 99, UTF8_RightArrow},
  {/* 0xFF54 */ XK_Down, 98, UTF8_DownArrow},
  {/* 0xFF55 */ XK_Page_Up, 33, UTF8_PageUp},
  {/* 0xFF56 */ XK_Page_Down, 54, UTF8_PageDown},
  {/* 0xFF57 */ XK_End, 53, UTF8_End},
  {/* 0xFF63 */ XK_Insert, 31, UTF8_Insert},
  {/* 0xFF67 */ XK_Menu, 104, "Menu"},
  {/* 0xFF80 */ XK_KP_Space, 94, " "},
  {/* 0xFF89 */ XK_KP_Tab, 38, UTF8_Tab},
  {/* 0xFF8D */ XK_KP_Enter, 91, UTF8_Return},
  {/* 0xFF95 */ XK_KP_Home, 55, UTF8_Home},
  {/* 0xFF96 */ XK_KP_Left, 72, UTF8_LeftArrow},
  {/* 0xFF97 */ XK_KP_Up, 56, UTF8_UpArrow},
  {/* 0xFF98 */ XK_KP_Right, 74, UTF8_RightArrow},
  {/* 0xFF99 */ XK_KP_Down, 89, UTF8_DownArrow},
  {/* 0xFF9A */ XK_KP_Page_Up, 57, UTF8_PageUp},
  {/* 0xFF9B */ XK_KP_Page_Down, 90, UTF8_PageDown},
  {/* 0xFF9C */ XK_KP_End, 88, UTF8_End},
  {/* 0xFF9D */ XK_KP_Begin, 73, "KP5"},
  {/* 0xFF9E */ XK_KP_Insert, 100, UTF8_Insert},
  {/* 0xFF9F */ XK_KP_Delete, 101, UTF8_Delete},
  {/* 0xFFBD */ XK_KP_Equal, 29, "="},
  {/* 0xFFAA */ XK_KP_Multiply, 36, "*"},
  {/* 0xFFAB */ XK_KP_Add, 58, "+"},
  {/* 0xFFAC */ XK_KP_Separator, 83, ","},
  {/* 0xFFAD */ XK_KP_Subtract, 37, "-"},
  {/* 0xFFAE */ XK_KP_Decimal, 101, "."},
  {/* 0xFFAF */ XK_KP_Divide, 35, "/"},
  {/* 0xFFB0 */ XK_KP_0, 100, "0"},
  {/* 0xFFB1 */ XK_KP_1, 88, "1"},
  {/* 0xFFB2 */ XK_KP_2, 89, "2"},
  {/* 0xFFB3 */ XK_KP_3, 90, "3"},
  {/* 0xFFB4 */ XK_KP_4, 72, "4"},
  {/* 0xFFB5 */ XK_KP_5, 73, "5"},
  {/* 0xFFB6 */ XK_KP_6, 74, "6"},
  {/* 0xFFB7 */ XK_KP_7, 55, "7"},
  {/* 0xFFB8 */ XK_KP_8, 56, "8"},
  {/* 0xFFB9 */ XK_KP_9, 57, "9"},
  {/* 0xFFFF */ XK_Delete, 101, UTF8_Delete}
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


static void PrintModifierState(uint32 Modifiers, char *OutputBuffer)
{
  // Append a readable description of the modifier keys to the buffer, which
  // should be 135 characters or longer to be safe.

  if (Modifiers & B_CAPS_LOCK)
    strcat(OutputBuffer, "CAPS_LOCK ");
  if (Modifiers & B_SCROLL_LOCK)
    strcat(OutputBuffer, "SCROLL_LOCK ");
  if (Modifiers & B_NUM_LOCK)
    strcat(OutputBuffer, "NUM_LOCK ");
  if (Modifiers & B_LEFT_SHIFT_KEY)
    strcat(OutputBuffer, "LEFT_SHIFT ");
  if (Modifiers & B_RIGHT_SHIFT_KEY)
    strcat(OutputBuffer, "RIGHT_SHIFT ");
  if (Modifiers & B_LEFT_COMMAND_KEY)
    strcat(OutputBuffer, "LEFT_COMMAND ");
  if (Modifiers & B_RIGHT_COMMAND_KEY)
    strcat(OutputBuffer, "RIGHT_COMMAND ");
  if (Modifiers & B_LEFT_CONTROL_KEY)
    strcat(OutputBuffer, "LEFT_CONTROL ");
  if (Modifiers & B_RIGHT_CONTROL_KEY)
    strcat(OutputBuffer, "RIGHT_CONTROL ");
  if (Modifiers & B_LEFT_OPTION_KEY)
    strcat(OutputBuffer, "LEFT_OPTION ");
  if (Modifiers & B_RIGHT_OPTION_KEY)
    strcat(OutputBuffer, "RIGHT_OPTION ");

  size_t OutLen = strlen (OutputBuffer);
  if (OutLen > 0)
    OutputBuffer[OutLen-1] = 0; // Remove trailing space.
}


static const char * NameOfScanCode(uint32 ScanCode)
{
  // Returns a readable description of the BeOS keyboard scan code, which can
  // be up to 18 characters long.

  static const char * KeyNames [] = {
    "Null", // 0
    "Esc", // 1
    "F1", // 2
    "F2", // 3
    "F3", // 4 
    "F4", // 5 
    "F5", // 6 
    "F6", // 7
    "F7", // 8
    "F8", // 9
    "F9", // 10
    "F10", // 11
    "F11", // 12
    "F12", // 13
    "PrintScreen-SysRq", // 14
    "ScrollLock", // 15
    "Pause-Break", // 16
    "`~", // 17
    "1!", // 18
    "2@", // 19
    "3#", // 20
    "4$", // 21
    "5%", // 22
    "6^", // 23
    "7&", // 24
    "8*", // 25
    "9(", // 26
    "0)", // 27
    "-_", // 28
    "=+", // 29
    "Backspace", // 30
    "Insert", // 31
    "Home", // 32
    "PageUp", // 33
    "NumLock", // 34
    "/KP", // 35
    "*KP", // 36
    "-KP", // 37
    "Tab", // 38
    "qQ", // 39
    "wW", // 40
    "eE", // 41
    "rR", // 42
    "tT", // 43
    "yY", // 44
    "uU", // 45
    "iI", // 46
    "oO", // 47
    "pP", // 48
    "[{", // 49
    "]}", // 50
    "\\|", // 51
    "Delete", // 52
    "End", // 53
    "PageDown", // 54
    "Home7KP", // 55
    "Up8KP", // 56
    "PageUp9KP", // 57
    "+KP", // 58
    "CapsLock", // 59
    "aA", // 60
    "sS", // 61
    "dD", // 62
    "fF", // 63
    "gG", // 64
    "hH", // 65
    "jJ", // 66
    "kK", // 67
    "lL", // 68
    ";:", // 69
    "'\"", // 70
    "Enter", // 71
    "Left4KP", // 72
    "Nop5KP", // 73
    "Right6KP", // 74
    "LeftShift", // 75
    "zZ", // 76
    "xX", // 77
    "cC", // 78
    "vV", // 79
    "bB", // 80
    "nN", // 81
    "mM", // 82
    ",<", // 83
    ".>", // 84
    "/?", // 85
    "RightShift", // 86
    "Up", // 87
    "End1KP", // 88
    "Down2KP", // 89
    "PageDown3KP", // 90
    "EnterKP", // 91
    "LeftControl", // 92
    "LeftAlt", // 93
    "Space", // 94
    "RightAlt", // 95
    "RightControl", // 96
    "Left", // 97
    "Down", // 98
    "Right", // 99
    "Insert0KP", // 100
    "Delete.KP", // 101
    "LeftWindows", // 102
    "RightWindows", // 103
    "Menu" // 104
  };
  const unsigned int KeyNamesSize = sizeof (KeyNames) / sizeof (KeyNames[0]);

  if (ScanCode >= KeyNamesSize)
    return "?";

  return KeyNames [ScanCode];
}




/******************************************************************************
 * The SDesktopBeOS class, methods follow in mostly alphabetical order.
 */

SDesktopBeOS::SDesktopBeOS () :
  m_BackgroundGrabScreenLastTime (0),
  m_BackgroundNextScanLineY (-1),
  m_BackgroundNumberOfScanLinesPerUpdate (32),
  m_BackgroundUpdateStartTime (0),
  m_DoubleClickTimeLimit (500000),
  m_EventInjectorPntr (NULL),
  m_FrameBufferBeOSPntr (NULL),
  m_KeyCharStrings (NULL),
  m_KeyMapPntr (NULL),
  m_LastMouseButtonState (0),
  m_LastMouseDownCount (1),
  m_LastMouseDownTime (0),
  m_LastMouseX (-1.0F),
  m_LastMouseY (-1.0F),
  m_ServerPntr (NULL),
  m_UserModifierState(0)
{
  get_click_speed (&m_DoubleClickTimeLimit);
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

  delete m_EventInjectorPntr;
  m_EventInjectorPntr = NULL;
}


void SDesktopBeOS::BackgroundScreenUpdateCheck ()
{
  bigtime_t        ElapsedTime = 0;
  int              Height;
  rfb::PixelFormat NewScreenFormat;
  int              NumberOfUpdates;
  rfb::PixelFormat OldScreenFormat;
  int              OldUpdateSize = 0;
  rfb::Rect        RectangleToUpdate;
  unsigned int     ScreenFormatSerialNumber;
  char             TempString [30];
  static int       UpdateCounter = 0;
  float            UpdatesPerSecond = 0;
  int              Width;

  if (m_FrameBufferBeOSPntr == NULL || m_ServerPntr == NULL ||
  (Width = m_FrameBufferBeOSPntr->width ()) <= 0 ||
  (Height = m_FrameBufferBeOSPntr->height ()) <= 0)
    return;

  ScreenFormatSerialNumber = m_FrameBufferBeOSPntr->LockFrameBuffer ();

  try
  {
    // Get the current screen size etc, if it has changed then inform the
    // server about the change.  Since the screen is locked, this also means
    // that VNC will be reading from a screen buffer that's the size it thinks
    // it is (no going past the end of video memory and causing a crash).

    OldScreenFormat = m_FrameBufferBeOSPntr->getPF ();
    m_FrameBufferBeOSPntr->UpdatePixelFormatEtc ();
    NewScreenFormat = m_FrameBufferBeOSPntr->getPF ();

    if (!NewScreenFormat.equal(OldScreenFormat) ||
    Width != m_FrameBufferBeOSPntr->width () ||
    Height != m_FrameBufferBeOSPntr->height ())
    {
      // This will trigger a full screen update too, which takes a while.
      vlog.debug("Screen resolution has changed, redrawing everything.");
      m_ServerPntr->setPixelBuffer (m_FrameBufferBeOSPntr);
      if (ShowCheapCursor)
        MakeCheapCursor ();
    }
    else // No screen change, try an update.
    {
      if (m_BackgroundNextScanLineY < 0 || m_BackgroundNextScanLineY >= Height)
      {
        // Time to start a new frame.  Update the number of scan lines to
        // process based on the performance in the previous frame.  Less scan
        // lines if the number of updates per second is too small, larger
        // slower updates if they are too fast.

        NumberOfUpdates = (Height + m_BackgroundNumberOfScanLinesPerUpdate - 1)
          / m_BackgroundNumberOfScanLinesPerUpdate; // Will be at least 1.
        ElapsedTime = system_time () - m_BackgroundUpdateStartTime;
        if (ElapsedTime <= 0)
          ElapsedTime = 1;
        UpdatesPerSecond = NumberOfUpdates / (ElapsedTime / 1000000.0F);
        OldUpdateSize = m_BackgroundNumberOfScanLinesPerUpdate;
        if (UpdatesPerSecond < 20)
          m_BackgroundNumberOfScanLinesPerUpdate--;
        else if (UpdatesPerSecond > 30)
          m_BackgroundNumberOfScanLinesPerUpdate++;

        // Only go as small as 4 for performance reasons.
        if (m_BackgroundNumberOfScanLinesPerUpdate <= 4)
          m_BackgroundNumberOfScanLinesPerUpdate = 4;
        else if (m_BackgroundNumberOfScanLinesPerUpdate > Height)
          m_BackgroundNumberOfScanLinesPerUpdate = Height;

        m_BackgroundNextScanLineY = 0;
        m_FrameBufferBeOSPntr->GrabScreen ();
        m_BackgroundGrabScreenLastTime = m_BackgroundUpdateStartTime =
          system_time ();
      }

      // Mark the current work unit of scan lines as needing an update.

      RectangleToUpdate.setXYWH (0, m_BackgroundNextScanLineY,
        Width, m_BackgroundNumberOfScanLinesPerUpdate);
      if (RectangleToUpdate.br.y > Height)
        RectangleToUpdate.br.y = Height;
      // Updated current screen contents if half a second has gone by.
      if (system_time () - m_BackgroundGrabScreenLastTime > 500000)
      {
        m_FrameBufferBeOSPntr->GrabScreen ();
        // Note that update of the time stamp is done AFTER the grab, in case
        // the grab is really slow and takes a lot of time, so we always have
        // half a second of regular operations before the next grab.
        m_BackgroundGrabScreenLastTime = system_time ();
      }
      rfb::Region RegionChanged (RectangleToUpdate);
      m_ServerPntr->add_changed (RegionChanged);

      // Tell the server to resend the changed areas, causing it to read the
      // screen memory in the areas marked as changed, compare it with the
      // previous version, and send out any changes.

      m_ServerPntr->tryUpdate ();

      m_BackgroundNextScanLineY += m_BackgroundNumberOfScanLinesPerUpdate;
    }
  }
  catch (...)
  {
    m_FrameBufferBeOSPntr->UnlockFrameBuffer ();
    throw;
  }

  m_FrameBufferBeOSPntr->UnlockFrameBuffer ();

  // Do the debug printing outside the lock, since printing goes through the
  // windowing system, which needs access to the screen.

  if (OldUpdateSize != 0) // If a full screen update has been finished.
  {
    if (OldUpdateSize != m_BackgroundNumberOfScanLinesPerUpdate)
      vlog.write(150 /* less important than debug level of 100 */,
      "Background update size changed from %d to %d scan lines "
      "due to performance of %.4f updates per second in the previous full "
      "screen frame.  Last frame achieved %.3f frames per second.",
      OldUpdateSize, m_BackgroundNumberOfScanLinesPerUpdate,
      UpdatesPerSecond, 1000000.0F / ElapsedTime);

    UpdateCounter++;
    sprintf (TempString, "Update #%d", UpdateCounter);
    m_FrameBufferBeOSPntr->SetDisplayMessage (TempString);
  }
}

void SDesktopBeOS::clientCutText (const char* str, int len)
{
  BMessage *ClipMsgPntr;
  if (be_clipboard->Lock())
  {
    be_clipboard->Clear ();
    if ((ClipMsgPntr = be_clipboard->Data ()) != NULL)
    {
      ClipMsgPntr->AddData ("text/plain", B_MIME_TYPE,
        str, len);
      be_clipboard->Commit ();
    }
    be_clipboard->Unlock ();
  }
}


uint8 SDesktopBeOS::FindKeyCodeFromMap (
  int32 *MapOffsetArray,
  const char *KeyAsString,
  bool KeyPadPreferred)
{
  int          Delta;
  int          Index;
  unsigned int KeyCode;
  int32       *OffsetPntr;
  char        *StringPntr;

  // If looking for the keycode that produces a symbol, we may have multiple
  // choices - like the number keys on the main keyboard and on the keypad.  To
  // give preference to the ones on the main keypad, a search is done with
  // those keys first.  To give preference to the keypad keys, the search is
  // done in reverse order.

  static const uint8 KeyCodesWithKeypadLast [] =
  { // Main keyboard key codes first.
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
    52, 53, 54, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
    75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,
    92, 93, 94, 95, 96, 97, 98, 99, 102, 103, 104, 105, 106, 107, 108, 109,
    110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124,
    125, 126, 127,

    // Keypad codes.
    34, 35, 36, 37, 55, 56, 57, 58, 72, 73, 74, 88, 89, 90, 91, 100, 101
  };

  const int ArraySize =
    sizeof (KeyCodesWithKeypadLast) / sizeof (KeyCodesWithKeypadLast[0]);

  if (KeyPadPreferred)
  {
    Index = ArraySize - 1;
    Delta = -1;
  }
  else // Normal forward search.
  {
    Index = 1 /* zero not valid */;
    Delta = 1;
  }

  for ( ; Index > 0 && Index < ArraySize; Index += Delta)
  {
    KeyCode = KeyCodesWithKeypadLast[Index];
    OffsetPntr = MapOffsetArray + KeyCode;
    StringPntr = m_KeyCharStrings + *OffsetPntr;
    uint8 StringLen = (uint8) (*StringPntr);
    if (StringLen == 0)
      continue; // Length of string (Pascal style with length byte) is zero.
    if (memcmp (StringPntr + 1, KeyAsString, StringLen) == 0 &&
    KeyAsString [StringLen] == 0 /* look for NUL at end of search string */)
      return KeyCode;
  }
  return 0;
}


const char* SDesktopBeOS::FindMappedSymbolFromKeycode (
  int32 *MapOffsetArray,
  uint8 KeyCode)
{
  // Looks up the symbol for a given keycode in a keymap.  Returns results
  // copied to a temporary global string, so copy them elsewhere if you want to
  // keep them.  Also not thread safe.

  static const char *EmptyString = "";
  int32             *OffsetPntr;
  static char        ReturnedString[8];
  uint8              StringLength;
  char              *StringPntr;

  if (KeyCode >= 128)
    return EmptyString;

  OffsetPntr = MapOffsetArray + KeyCode;
  StringPntr = m_KeyCharStrings + *OffsetPntr;
  StringLength = *StringPntr++; // Pascal style with initial length byte.
  if (StringLength <= 0)
    return EmptyString;
  if (StringLength >= sizeof (ReturnedString))
    StringLength = sizeof (ReturnedString) - 1;

  memcpy (ReturnedString, StringPntr, StringLength);
  ReturnedString[StringLength] = 0;

  return ReturnedString;
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
  bool                KeyPadUsed;
  VNCKeyToUTF8Record  KeyToUTF8SearchData;
  VNCKeyToUTF8Pointer KeyToUTF8Pntr;
  key_info            NewKeyState;

  if (m_EventInjectorPntr == NULL || m_FrameBufferBeOSPntr == NULL ||
  m_FrameBufferBeOSPntr->width () <= 0 || m_KeyMapPntr == NULL)
    return;

  NewKeyState = m_LastKeyState;
  vlog.debug ("VNC keycode $%04X received, key is %s.",
    key, down ? "down" : "up");

  // If it's a shift or other modifier key, update our internal modifiers
  // state.

  switch (key)
  {
    case XK_Caps_Lock: ChangedModifiers = B_CAPS_LOCK; break;
    case XK_Scroll_Lock: ChangedModifiers = B_SCROLL_LOCK; break;
    case XK_Num_Lock: ChangedModifiers = B_NUM_LOCK; break;
    case XK_Shift_L: ChangedModifiers = B_LEFT_SHIFT_KEY; break;
    case XK_Shift_R: ChangedModifiers = B_RIGHT_SHIFT_KEY; break;

     // Are alt/control swapped?  The menu preference does it by swapping the
     // keycodes in the keymap (couldn't find any other way of detecting it).
     // The usual left control key is 92, and left alt is 93.  Usual means on
     // the standard US keyboard layout.  Other keyboards may have other
     // physical key codes for control and alt, so the default is to have alt
     // be the command key and control be the control key.

     case XK_Control_L: ChangedModifiers =
      ((m_KeyMapPntr->left_control_key != 93) ?
      B_LEFT_CONTROL_KEY : B_LEFT_COMMAND_KEY); break;
    case XK_Control_R: ChangedModifiers =
      ((m_KeyMapPntr->left_control_key != 93) ?
      B_RIGHT_CONTROL_KEY : B_RIGHT_COMMAND_KEY); break;

    case XK_Alt_L: ChangedModifiers =
      ((m_KeyMapPntr->left_control_key != 93) ?
      B_LEFT_COMMAND_KEY : B_LEFT_CONTROL_KEY); break;
    case XK_Alt_R: ChangedModifiers =
      ((m_KeyMapPntr->left_control_key != 93) ?
      B_RIGHT_COMMAND_KEY : B_RIGHT_CONTROL_KEY); break;

    case XK_Meta_L: // RealVNC sends left Windows as this.
    case XK_Super_L: // TigerVNC sends left Windows as this.
      ChangedModifiers = B_LEFT_OPTION_KEY; break;
    case XK_Meta_R: // RealVNC sends right Windows as this.
    case XK_Super_R: // TigerVNC sends right Windows as this.
      ChangedModifiers = B_RIGHT_OPTION_KEY; break;

    default: ChangedModifiers = 0;
  }

  // Update the modifiers for the particular modifier key if one was pressed.

  if (ChangedModifiers != 0)
  {
    if (down)
    {
      NewKeyState.modifiers |= ChangedModifiers;
      m_UserModifierState |= ChangedModifiers;
    }
    else
    {
      NewKeyState.modifiers &= ~ChangedModifiers;
      m_UserModifierState &= ~ChangedModifiers;
    }
    UpdateDerivedModifiers (m_UserModifierState);
    UpdateDerivedModifiers (NewKeyState.modifiers);
    WriteModifiersToKeyState (NewKeyState);

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
      m_EventInjectorPntr->Control ('EInj', &EventMessage);
      EventMessage.MakeEmpty ();

      char TempString[200];
      strcpy (TempString, "Modifiers changed to: ");
      PrintModifierState (NewKeyState.modifiers, TempString);
      vlog.debug ("%s", TempString);
    }

    SendUnmappedKeys (m_LastKeyState, NewKeyState);
    m_LastKeyState = NewKeyState;

    if (ChangedModifiers != B_SCROLL_LOCK)
      return; // No actual typeable key was pressed, nothing further to do.
  }

  // Look for keys which don't have UTF-8 codes, like the function keys.  They
  // also are a direct press; they don't try to fiddle with the shift keys.

  KeyCode = 0;
  KeyPadUsed = (key >= XK_KP_Space && key <= XK_KP_9);

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
    vlog.debug ("%s functionish key code %d (%s).",
      down ? "Pressed" : "Released", KeyCode, NameOfScanCode (KeyCode));

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
    m_EventInjectorPntr->Control ('EInj', &EventMessage);
    EventMessage.MakeEmpty ();
    return;
  }

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

  if (KeyToUTF8Pntr == NULL)
  {
    vlog.info ("VNC keycode $%04X (%s) isn't recognised, ignoring it.",
      key, down ? "down" : "up");
    return; // Key not handled, ignore it.
  }

  // Have a corresponding UTF-8 string we are wanting to type.  For debugging,
  // make a printable version.

  char KeyAsReadableUTF8String [40];
  {
    const char *pSource = KeyToUTF8Pntr->utf8String;
    char *pDest = KeyAsReadableUTF8String;
    while (*pSource != 0 &&
    pDest < KeyAsReadableUTF8String + sizeof (KeyAsReadableUTF8String) - 4)
    {
      if (*pSource < 32 || *pSource == 127)
      {
        sprintf (pDest, "%02d", (int) *pSource);
        pSource++;
        pDest += strlen (pDest);
      }
      else
        *pDest++ = *pSource++;
    }
    *pDest = 0;
  }

  // Look for the UTF-8 string in the keymap tables which are currently active
  // (reflecting the current modifier keys) to see which key code it is.

  strcpy (KeyAsString, KeyToUTF8Pntr->utf8String);
  KeyCode = 0;
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_OPTION_KEY) &&
  (m_LastKeyState.modifiers & B_CAPS_LOCK) &&
  (m_LastKeyState.modifiers & B_SHIFT_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->option_caps_shift_map, KeyAsString, KeyPadUsed);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_OPTION_KEY) &&
  (m_LastKeyState.modifiers & B_CAPS_LOCK))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->option_caps_map, KeyAsString, KeyPadUsed);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_OPTION_KEY) &&
  (m_LastKeyState.modifiers & B_SHIFT_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->option_shift_map, KeyAsString, KeyPadUsed);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_OPTION_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->option_map, KeyAsString, KeyPadUsed);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_CAPS_LOCK) &&
  (m_LastKeyState.modifiers & B_SHIFT_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->caps_shift_map, KeyAsString, KeyPadUsed);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_CAPS_LOCK))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->caps_map, KeyAsString, KeyPadUsed);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_SHIFT_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->shift_map, KeyAsString, KeyPadUsed);
  if (KeyCode == 0)
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->normal_map, KeyAsString, KeyPadUsed);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_CONTROL_KEY))
    KeyCode = FindKeyCodeFromMap ( /* Control is last since weird. */
      m_KeyMapPntr->control_map, KeyAsString, KeyPadUsed);

  if (KeyCode != 0)
  {
    // Got a key that works with the current modifier settings.  Simulate
    // pressing it.

    vlog.debug ("%s regular key code %d (%s) to achieve \"%s\".",
      down ? "Pressed" : "Released", KeyCode, NameOfScanCode (KeyCode),
      KeyAsReadableUTF8String);

    SetKeyState (m_LastKeyState, KeyCode, down);
    SendMappedKeyMessage (KeyCode, down, KeyAsString, EventMessage);

    // Finally, revert back to the modifier keys the user had specified, if
    // this was a release of the key.

    if (m_LastKeyState.modifiers != m_UserModifierState && !down)
    {
      char TempString[400];
      strcpy (TempString, "Restoring modifiers from (");
      PrintModifierState (m_LastKeyState.modifiers, TempString);
      strcat (TempString, ") to user's original modifiers of (");
      PrintModifierState (m_UserModifierState, TempString);
      strcat (TempString, ").");
      vlog.debug ("%s", TempString);

      NewKeyState = m_LastKeyState;
      NewKeyState.modifiers = m_UserModifierState;
      WriteModifiersToKeyState (NewKeyState);

      EventMessage.what = B_MODIFIERS_CHANGED;
      EventMessage.AddInt64 ("when", system_time ());
      EventMessage.AddInt32 ("be:old_modifiers", m_LastKeyState.modifiers);
      EventMessage.AddInt32 ("modifiers", NewKeyState.modifiers);
      EventMessage.AddData ("states",
        B_UINT8_TYPE, &NewKeyState.key_states, 16);
      m_EventInjectorPntr->Control ('EInj', &EventMessage);
      EventMessage.MakeEmpty ();

      SendUnmappedKeys (m_LastKeyState, NewKeyState);
      m_LastKeyState = NewKeyState;
    }

    return; // Finished with ordinary key presses.
  }

  // The key isn't an obvious one.  Check all the keymap tables to see if we
  // can get to that character by pressing combinations of the modifier keys.
  // If it's found, temporarily switch to those modifier keys being pressed and
  // then send the message for that key pressed.  If it was a key-up then
  // release the temporary modifier key changes and go back to the modifier
  // keys the user specified.

  uint32 NewModifier = 0;
  if (0 != (KeyCode = FindKeyCodeFromMap (
  m_KeyMapPntr->normal_map, KeyAsString, KeyPadUsed)))
    NewModifier = 0;
  else if (0 != (KeyCode = FindKeyCodeFromMap (
  m_KeyMapPntr->shift_map, KeyAsString, KeyPadUsed)))
    NewModifier = B_LEFT_SHIFT_KEY;
  else if (0 != (KeyCode = FindKeyCodeFromMap (
  m_KeyMapPntr->caps_map, KeyAsString, KeyPadUsed)))
    NewModifier = B_CAPS_LOCK;
  else if (0 != (KeyCode = FindKeyCodeFromMap (
  m_KeyMapPntr->caps_shift_map, KeyAsString, KeyPadUsed)))
    NewModifier = B_CAPS_LOCK | B_LEFT_SHIFT_KEY;
  else if (0 != (KeyCode = FindKeyCodeFromMap (
  m_KeyMapPntr->option_map, KeyAsString, KeyPadUsed)))
    NewModifier = B_LEFT_OPTION_KEY;
  else if (0 != (KeyCode = FindKeyCodeFromMap (
  m_KeyMapPntr->option_shift_map, KeyAsString, KeyPadUsed)))
    NewModifier = B_LEFT_OPTION_KEY | B_LEFT_SHIFT_KEY;
  else if (0 != (KeyCode = FindKeyCodeFromMap (
  m_KeyMapPntr->option_caps_map, KeyAsString, KeyPadUsed)))
    NewModifier = B_LEFT_OPTION_KEY | B_CAPS_LOCK;
  else if (0 != (KeyCode = FindKeyCodeFromMap (
  m_KeyMapPntr->option_caps_shift_map, KeyAsString, KeyPadUsed)))
    NewModifier = B_LEFT_OPTION_KEY | B_CAPS_LOCK | B_LEFT_SHIFT_KEY;
  else if (0 != (KeyCode = FindKeyCodeFromMap (
  m_KeyMapPntr->control_map, KeyAsString, KeyPadUsed)))
    NewModifier = B_LEFT_CONTROL_KEY;

  if (KeyCode == 0)
  {
    vlog.debug ("VNC keycode $%04X (%s) maps to \"%s\", but it isn't obvious "
      "which BeOS keys need to be \"pressed\" to achieve that.  Ignoring it.",
      key, down ? "down" : "up", KeyAsReadableUTF8String);
    return;
  }

  NewModifier |= (m_LastKeyState.modifiers & (
    B_SCROLL_LOCK | B_NUM_LOCK | B_LEFT_COMMAND_KEY | B_RIGHT_COMMAND_KEY));
  UpdateDerivedModifiers (NewModifier);

  { // Debug message block.
    char TempString[400];
    sprintf (TempString, "VNC keycode $%04X (%s) maps to \"%s\", but we need "
      "to change modifiers from (", key, down ? "down" : "up",
      KeyAsReadableUTF8String);
    PrintModifierState (m_LastKeyState.modifiers, TempString);
    strcat (TempString, ") to (");
    PrintModifierState (NewModifier, TempString);
    strcat (TempString, ").");
    vlog.debug ("%s", TempString);
  }

  NewKeyState = m_LastKeyState;
  NewKeyState.modifiers = NewModifier;
  WriteModifiersToKeyState (NewKeyState);

  if (NewKeyState.modifiers != m_LastKeyState.modifiers)
  {
    EventMessage.what = B_MODIFIERS_CHANGED;
    EventMessage.AddInt64 ("when", system_time ());
    EventMessage.AddInt32 ("be:old_modifiers", m_LastKeyState.modifiers);
    EventMessage.AddInt32 ("modifiers", NewKeyState.modifiers);
    EventMessage.AddData ("states",
      B_UINT8_TYPE, &NewKeyState.key_states, 16);
    m_EventInjectorPntr->Control ('EInj', &EventMessage);
    EventMessage.MakeEmpty ();
  }
  SendUnmappedKeys (m_LastKeyState, NewKeyState);
  m_LastKeyState = NewKeyState;

  // Now send the actual key, using the modified modifier keys.

  vlog.debug ("%s temporarily shifted key code %d (%s).",
    down ? "Pressed" : "Released", KeyCode, NameOfScanCode (KeyCode));

  SetKeyState (m_LastKeyState, KeyCode, down);
  SendMappedKeyMessage (KeyCode, down, KeyAsString, EventMessage);
}


void SDesktopBeOS::MakeCheapCursor (void)
{
  if (m_FrameBufferBeOSPntr == NULL || m_ServerPntr == NULL)
    return;

#define BIG_CURSOR 1
#if BIG_CURSOR
  static unsigned short CrossMask [15 /* Our cursor is 15 rows tall */] =
  { // A one bit per pixel transparency mask, high bit is first one in second byte.
    0x8003, // 0000 0011 1000 0000
    0x8003, // 0000 0011 1000 0000
    0x8003, // 0000 0011 1000 0000
    0x8003, // 0000 0011 1000 0000
    0x0001, // 0000 0001 0000 0000
    0x0001, // 0000 0001 0000 0000
    0x1EF1, // 1111 0001 0001 1110
    0xFEFE, // 1111 1110 1111 1110
    0x1EF1, // 1111 0001 0001 1110
    0x0001, // 0000 0001 0000 0000
    0x0001, // 0000 0001 0000 0000
    0x8003, // 0000 0011 1000 0000
    0x8003, // 0000 0011 1000 0000
    0x8003, // 0000 0011 1000 0000
    0x8003, // 0000 0011 1000 0000
  };
#else
  static unsigned char CrossMask [7 /* Our cursor is 7 rows tall */] =
  { // A one bit per pixel transparency mask, high bit first in each byte.
    0x10, // 00010000
    0x10, // 00010000
    0x10, // 00010000
    0xEE, // 11101110
    0x10, // 00010000
    0x10, // 00010000
    0x10, // 00010000
  };
#endif

  rfb::Pixel         Black;
  rfb::Pixel         Blue;
  rfb::Pixel         White;
  rfb::Rect          Rectangle;

  rfb::ManagedPixelBuffer CursorBitmap (m_FrameBufferBeOSPntr->getPF (),
#if BIG_CURSOR
   15, 15);
#else
   7, 7);
#endif

#if defined(__POWERPC__) && BIG_CURSOR
  // Need to reverse the byte order for big-endian CPUs.
  {
    static bool bReverseDone = false;
    if (!bReverseDone)
    {
      int i;
      bReverseDone = true;
      for (i = 0; i < sizeof(CrossMask) / sizeof(CrossMask[0]); i++)
      {
        unsigned short Temp, Low, High;
        Temp = CrossMask[i];
        Low = (Temp & 0xFF);
        High = ((Temp >> 8) & 0xFF);
        Temp = ((Low << 8) | High );
        CrossMask[i] = Temp;
      }
    }
  }
#endif


  Black = m_FrameBufferBeOSPntr->getPF().pixelFromRGB (
    (rdr::U16) 0, (rdr::U16) 0, (rdr::U16) 0,
    m_FrameBufferBeOSPntr->getColourMap());
  White = m_FrameBufferBeOSPntr->getPF().pixelFromRGB (
    (rdr::U16) 0xFFFF, (rdr::U16) 0xFFFF, (rdr::U16) 0xFFFF,
    m_FrameBufferBeOSPntr->getColourMap());
  Blue = m_FrameBufferBeOSPntr->getPF().pixelFromRGB (
    (rdr::U16) 0x4000, (rdr::U16) 0x4000, (rdr::U16) 0xFFFF,
    m_FrameBufferBeOSPntr->getColourMap());

  // Fill in the colours underlying the cursor shape.
#if BIG_CURSOR
  Rectangle.setXYWH (0, 0, 15, 15);
  CursorBitmap.fillRect (Rectangle, White);
  Rectangle.setXYWH (6, 0, 3, 15);
  CursorBitmap.fillRect (Rectangle, Blue);
  Rectangle.setXYWH (0, 6, 15, 3);
  CursorBitmap.fillRect (Rectangle, Blue);
  Rectangle.setXYWH (7, 0, 1, 15);
  CursorBitmap.fillRect (Rectangle, Black);
  Rectangle.setXYWH (0, 7, 15, 1);
  CursorBitmap.fillRect (Rectangle, Black);
  Rectangle.setXYWH (7, 1, 1, 3);
  CursorBitmap.fillRect (Rectangle, White);
  Rectangle.setXYWH (7, 11, 1, 3);
  CursorBitmap.fillRect (Rectangle, White);
  Rectangle.setXYWH (1, 7, 3, 1);
  CursorBitmap.fillRect (Rectangle, White);
  Rectangle.setXYWH (11, 7, 3, 1);
  CursorBitmap.fillRect (Rectangle, White);
#else
  Rectangle.setXYWH (0, 0, 7, 7);
  CursorBitmap.fillRect (Rectangle, White);
  Rectangle.setXYWH (3, 0, 1, 7);
  CursorBitmap.fillRect (Rectangle, Black);
  Rectangle.setXYWH (0, 3, 7, 1);
  CursorBitmap.fillRect (Rectangle, Black);
#endif

  // Tell the server to use it.
  m_ServerPntr->setCursor (
#if BIG_CURSOR
    15, 15, 7, 7, /* x, y, hotx, hoty */
#else
    7, 7, 3, 3, /* x, y, hotx, hoty */
#endif
    CursorBitmap.data, CrossMask);
}


void SDesktopBeOS::pointerEvent (const rfb::Point& pos, rdr::U8 buttonmask)
{
  if (m_EventInjectorPntr == NULL || m_FrameBufferBeOSPntr == NULL ||
  m_ServerPntr == NULL || m_FrameBufferBeOSPntr->width () <= 0)
    return;

  float     AbsoluteY;
  float     AbsoluteX;
  bigtime_t CurrentTime;
  bigtime_t ElapsedTime;
  BMessage  EventMessage;
  rdr::U8   NewMouseButtons;
  rdr::U8   OldMouseButtons;

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
  // state and the "when" time field.  We also have to fake the "clicks" field
  // with the current double click count, but only for mouse down messages.
  // The system will add "modifiers", "be:transit" and "be:view_where".

  if (EventMessage.what != B_MOUSE_MOVED ||
  m_LastMouseX != AbsoluteX || m_LastMouseY != AbsoluteY ||
  NewMouseButtons != OldMouseButtons)
  {
    CurrentTime = system_time ();
    EventMessage.AddFloat ("x", AbsoluteX);
    EventMessage.AddFloat ("y", AbsoluteY);
    EventMessage.AddInt64 ("when", CurrentTime);
    EventMessage.AddInt32 ("buttons", NewMouseButtons);
    if (EventMessage.what == B_MOUSE_DOWN)
    {
      ElapsedTime = CurrentTime - m_LastMouseDownTime;
      if (ElapsedTime <= m_DoubleClickTimeLimit)
        m_LastMouseDownCount++;
      else // Counts as a new single click.
        m_LastMouseDownCount = 1;
      EventMessage.AddInt32 ("clicks", m_LastMouseDownCount);
      m_LastMouseDownTime = CurrentTime;
    }

    m_EventInjectorPntr->Control ('EInj', &EventMessage);
    EventMessage.MakeEmpty ();

    // Also request a screen update later on for the area around the mouse
    // coordinates.  That way moving the mouse around will update the screen
    // under the mouse pointer.  Same for clicking, since that often brings up
    // menus which need to be made visible.

    if (ShowCheapCursor)
      m_ServerPntr->setCursorPos (pos.x, pos.y);

    rfb::Rect ScreenRect;
    ScreenRect.setXYWH (0, 0,
      m_FrameBufferBeOSPntr->width (), m_FrameBufferBeOSPntr->height ());
    rfb::Rect RectangleToUpdate;
    RectangleToUpdate.setXYWH (pos.x - 32, pos.y - 32, 64, 64);
    rfb::Region RegionChanged (RectangleToUpdate.intersect (ScreenRect));
    m_ServerPntr->add_changed (RegionChanged);
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
    m_EventInjectorPntr->Control ('EInj', &EventMessage);

  m_LastMouseButtonState = buttonmask;
  m_LastMouseX = AbsoluteX;
  m_LastMouseY = AbsoluteY;
}


void SDesktopBeOS::SendMappedKeyMessage (uint8 KeyCode, bool down,
  const char *KeyAsString, BMessage &EventMessage)
{
    EventMessage.what = down ? B_KEY_DOWN : B_KEY_UP;
    EventMessage.AddInt64 ("when", system_time ());
    EventMessage.AddInt32 ("key", KeyCode);
    EventMessage.AddInt32 ("modifiers", m_LastKeyState.modifiers);

    ssize_t NumBytes = strlen (KeyAsString);
    if (NumBytes > 0)
    {
      EventMessage.AddData ("byte", B_INT8_TYPE, KeyAsString,
        1 /* numBytes is array element size */, true /* fixedSize */,
        NumBytes /* numItems in the array */);
      for (int i = 1; i < NumBytes; i++)
        EventMessage.AddData ("byte", B_INT8_TYPE, KeyAsString + i, 1);
    }

    EventMessage.AddString ("bytes", KeyAsString);
    EventMessage.AddData ("states", B_UINT8_TYPE,
      m_LastKeyState.key_states, 16);
    EventMessage.AddInt32 ("raw_char",
      FindMappedSymbolFromKeycode (m_KeyMapPntr->normal_map, KeyCode)[0]);
    m_EventInjectorPntr->Control ('EInj', &EventMessage);
    EventMessage.MakeEmpty ();
}


void SDesktopBeOS::SendUnmappedKeys (
  key_info &OldKeyState,
  key_info &NewKeyState)
{
  int          BitIndex;
  unsigned int ByteIndex;
  uint8        DeltaBit;
  BMessage     EventMessage;
  int          KeyCode;
  uint8        Mask;
  uint8        NewBits;
  uint8        OldBits;

  for (ByteIndex = 0; ByteIndex < sizeof (OldKeyState.key_states); ByteIndex++)
  {
    OldBits = OldKeyState.key_states[ByteIndex];
    NewBits = NewKeyState.key_states[ByteIndex];
    if (OldBits == NewBits)
      continue; // Group of eight keys are unchanged.
    KeyCode = ByteIndex * 8;
    for (BitIndex = 7; BitIndex >= 0; BitIndex--, KeyCode++)
    {
      Mask = 1 << BitIndex;
      DeltaBit = ((OldBits ^ NewBits) & Mask);
      if (DeltaBit == 0)
        continue; // Key hasn't changed.
      vlog.debug ("%s unmapped key code %d (%s).",
        (NewBits & Mask) ? "Pressed" : "Released", KeyCode,
        NameOfScanCode (KeyCode));
      EventMessage.what =
        (NewBits & Mask) ? B_UNMAPPED_KEY_DOWN : B_UNMAPPED_KEY_UP;
      EventMessage.AddInt64 ("when", system_time ());
      EventMessage.AddInt32 ("key", KeyCode);
      EventMessage.AddInt32 ("modifiers", NewKeyState.modifiers);
      EventMessage.AddData ("states",
        B_UINT8_TYPE, &NewKeyState.key_states, 16);
      m_EventInjectorPntr->Control ('EInj', &EventMessage);
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

  // Try various different techniques for reading the screen, their
  // constructors will fail if they aren't supported.

  if (m_FrameBufferBeOSPntr == NULL && TryBDirectWindow)
  {
    try
    {
      m_FrameBufferBeOSPntr = new FrameBufferBDirect;
    }
    catch (rdr::Exception &e)
    {
      vlog.error(e.str());
      m_FrameBufferBeOSPntr = NULL;
    }
  }

  if (m_FrameBufferBeOSPntr == NULL && TryBScreen)
  {
    try
    {
      m_FrameBufferBeOSPntr = new FrameBufferBScreen;
    }
    catch (rdr::Exception &e)
    {
      vlog.error(e.str());
      m_FrameBufferBeOSPntr = NULL;
    }
  }

  if (m_FrameBufferBeOSPntr == NULL)
    throw rdr::Exception ("Unable to find any screen reading techniques "
    "which are compatible with your video card.  Only tried the ones not "
    "turned off on the command line.  Perhaps try a different video card?",
    "SDesktopBeOS::start");

  m_ServerPntr->setPixelBuffer (m_FrameBufferBeOSPntr);

  if (ShowCheapCursor)
    MakeCheapCursor ();

  if (m_EventInjectorPntr == NULL)
    m_EventInjectorPntr =
    find_input_device ("InputEventInjector FakeKeyboard");

  if (m_KeyMapPntr != NULL || m_KeyCharStrings != NULL)
    throw rfb::Exception ("SDesktopBeOS::start: key map pointers not "
    "NULL, bug!");
  get_key_map (&m_KeyMapPntr, &m_KeyCharStrings);
  if (m_KeyMapPntr == NULL || m_KeyCharStrings == NULL)
    throw rfb::Exception ("SDesktopBeOS::start: get_key_map has failed, "
    "so we can't simulate the keyboard buttons being pressed!");

#if 1 // Dump out the key maps.
  printf ("Keymap Dump, version %d:\n", (int) m_KeyMapPntr->version);
  printf ("CapsLock key: %d (%s), ScrollLock: %d (%s), NumLock: %d (%s)\n",
    (int) m_KeyMapPntr->caps_key, NameOfScanCode (m_KeyMapPntr->caps_key),
    (int) m_KeyMapPntr->scroll_key, NameOfScanCode (m_KeyMapPntr->scroll_key),
    (int) m_KeyMapPntr->num_key, NameOfScanCode (m_KeyMapPntr->num_key));
  printf ("Left Shift: %d (%s), Right Shift: %d (%s)\n",
    (int) m_KeyMapPntr->left_shift_key,
    NameOfScanCode (m_KeyMapPntr->left_shift_key),
    (int) m_KeyMapPntr->right_shift_key,
    NameOfScanCode (m_KeyMapPntr->right_shift_key));
  printf ("Left Command: %d (%s), Right Command: %d (%s)\n",
    (int) m_KeyMapPntr->left_command_key,
    NameOfScanCode (m_KeyMapPntr->left_command_key),
    (int) m_KeyMapPntr->right_command_key,
    NameOfScanCode (m_KeyMapPntr->right_command_key));
  printf ("Left Control: %d (%s), Right Control: %d (%s)\n",
    (int) m_KeyMapPntr->left_control_key,
    NameOfScanCode (m_KeyMapPntr->left_control_key),
    (int) m_KeyMapPntr->right_control_key,
    NameOfScanCode (m_KeyMapPntr->right_control_key));
  printf ("Left Option: %d (%s), Right Option: %d (%s)\n",
    (int) m_KeyMapPntr->left_option_key,
    NameOfScanCode (m_KeyMapPntr->left_option_key),
    (int) m_KeyMapPntr->right_option_key,
    NameOfScanCode (m_KeyMapPntr->right_option_key));
  printf ("Menu key: %d (%s), Lock settings: 0x%X\n",
    (int) m_KeyMapPntr->menu_key,
    NameOfScanCode (m_KeyMapPntr->menu_key),
    (int) m_KeyMapPntr->lock_settings);

  int32 * Maps[9];
  Maps[0] = m_KeyMapPntr->control_map;
  Maps[1] = m_KeyMapPntr->option_caps_shift_map;
  Maps[2] = m_KeyMapPntr->option_caps_map;
  Maps[3] = m_KeyMapPntr->option_shift_map;
  Maps[4] = m_KeyMapPntr->option_map;
  Maps[5] = m_KeyMapPntr->caps_shift_map;
  Maps[6] = m_KeyMapPntr->caps_map;
  Maps[7] = m_KeyMapPntr->shift_map;
  Maps[8] = m_KeyMapPntr->normal_map;
  char * MapNames[9] =
  {
    "control_map",
    "option_caps_shift_map",
    "option_caps_map",
    "option_shift_map",
    "option_map",
    "caps_shift_map",
    "caps_map",
    "shift_map",
    "normal_map"
  };

  for (int i = 0; i < 9; i++)
  {
    printf ("\n%s:\n", MapNames[i]);
    bool SomethingPrinted = false;
    for (int j = 0; j < 128; j++)
    {
      unsigned char SymbolString [80];
      char  *StringPntr;

      StringPntr = m_KeyCharStrings + Maps[i][j];
      uint8 StringLen = (uint8) (*StringPntr);
      if (StringLen != 0)
      {
        SomethingPrinted = true;
        printf ("%d=%s=", j, NameOfScanCode (j));
        memcpy (SymbolString, StringPntr + 1, StringLen);
        SymbolString[StringLen] = 0;
        if (SymbolString[0] < 32 || SymbolString[0] == 127)
        {
          for (int k = 0; k < StringLen; k++)
            printf ("%02d ", SymbolString[k]);
        }
        else
          printf ("%s ", SymbolString);
      }
      if ((j & 7) == 7)
      {
        if (SomethingPrinted)
          printf ("\n");
        SomethingPrinted = false;
      }
    }
  }
#endif // Dump keymaps.
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

  delete m_EventInjectorPntr;
  m_EventInjectorPntr = NULL;
}


void SDesktopBeOS::UpdateDerivedModifiers (
  uint32 &Modifiers)
{
  uint32 TempModifiers;

  // Update the virtual modifiers, ones which have a left and right key that do
  // the same thing to reflect the state of the real keys.

  TempModifiers = Modifiers;

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

  Modifiers = TempModifiers;
}


void SDesktopBeOS::WriteModifiersToKeyState (
  key_info &KeyState)
{
  if (m_KeyMapPntr == NULL)
    return; // Can't do anything more without it.

  // Update the pressed keys to reflect the modifiers.  Use the keymap to find
  // the keycode for the actual modifier key.  Note that some (right control)
  // may be zero to show no key defined, but then key #0 doesn't exist so
  // pressing it hopefully won't do much.

  uint32 TempModifiers = KeyState.modifiers;

  SetKeyState (KeyState, m_KeyMapPntr->caps_key,
    (TempModifiers & B_CAPS_LOCK) != 0);
  SetKeyState (KeyState, m_KeyMapPntr->scroll_key,
    (TempModifiers & B_SCROLL_LOCK) != 0);
  SetKeyState (KeyState, m_KeyMapPntr->num_key,
    (TempModifiers & B_NUM_LOCK) != 0);
  SetKeyState (KeyState, m_KeyMapPntr->left_shift_key,
    (TempModifiers & B_LEFT_SHIFT_KEY) != 0);
  SetKeyState (KeyState, m_KeyMapPntr->right_shift_key,
    (TempModifiers & B_RIGHT_SHIFT_KEY) != 0);
  SetKeyState (KeyState, m_KeyMapPntr->left_command_key,
    (TempModifiers & B_LEFT_COMMAND_KEY) != 0);
  SetKeyState (KeyState, m_KeyMapPntr->right_command_key,
    (TempModifiers & B_RIGHT_COMMAND_KEY) != 0);
  SetKeyState (KeyState, m_KeyMapPntr->left_control_key,
    (TempModifiers & B_LEFT_CONTROL_KEY) != 0);
  SetKeyState (KeyState, m_KeyMapPntr->right_control_key,
    (TempModifiers & B_RIGHT_CONTROL_KEY) != 0);
  SetKeyState (KeyState, m_KeyMapPntr->left_option_key,
    (TempModifiers & B_LEFT_OPTION_KEY) != 0);
  SetKeyState (KeyState, m_KeyMapPntr->right_option_key,
    (TempModifiers & B_RIGHT_OPTION_KEY) != 0);
}
