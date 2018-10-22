/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0-beossrc/beosserver/RCS/ServerMain.cxx,v 1.28 2018/01/10 22:25:05 agmsmith Exp agmsmith $
 *
 * This is the main program for the BeOS version of the VNC server.  The basic
 * functionality comes from the VNC 4.0b4 source code (available from
 * http://www.realvnc.com/), with BeOS adaptations by Alexander G. M. Smith.
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
 * $Log: ServerMain.cxx,v $
 * Revision 1.28  2018/01/10 22:25:05  agmsmith
 * Bumped version number for New Haiku (2018) build.
 *
 * Revision 1.27  2015/09/12 00:32:08  agmsmith
 * Now with working keypad and better handling of odd keyboard combinations.
 * Also the dumb screen grab (BScreen) method now does updates once in a while
 * so that you can scrub with the mouse and see new screen contents, not the
 * stuff from the last full screen refresh.
 *
 * Revision 1.26  2014/10/05 20:14:21  agmsmith
 * Bump version number, changes all in documentation about Haiku and using
 * .hpkg
 *
 * Revision 1.25  2014/07/28 20:26:06  agmsmith
 * Change main thread priority to 5 (low) rather than the default of 10
 * (normal).  Also add a command line option to specify it.
 *
 * Revision 1.24  2013/04/23 20:49:02  agmsmith
 * Adjusted types and headers to make it buildable in GCC4 under Haiku OS.
 *
 * Revision 1.23  2013/02/11 22:31:17  agmsmith
 * Detect network failures and restart networking, which included figuring out
 * how to shut it all down first.  Now survives NetServer reboots.
 *
 * Revision 1.22  2011/11/11 21:59:57  agmsmith
 * Bumped version number for new bigger cheap cursor, for iPad viewability.
 *
 * Revision 1.21  2007/01/23 02:41:59  agmsmith
 * No changes - just a recompile with the newer gcc that does better
 * optimization.
 *
 * Revision 1.20  2005/05/30 00:41:35  agmsmith
 * Punctuation.
 *
 * Revision 1.19  2005/05/30 00:40:05  agmsmith
 * Changed output to use stdout for compatibility with the GUI shell.
 *
 * Revision 1.18  2005/02/14 02:29:56  agmsmith
 * Removed unused parameters - HTTP servers and host wildcards.
 *
 * Revision 1.17  2005/02/13 01:28:44  agmsmith
 * Now notices clipboard changes and informs all the clients about the new text
 * contents.
 *
 * Revision 1.16  2005/02/06 23:39:14  agmsmith
 * Bumpped version number.
 *
 * Revision 1.15  2005/01/03 00:19:50  agmsmith
 * Based on more recent source than 4.0 Beta 4, update comments to show that
 * it's the final 4.0 VNC source.
 *
 * Revision 1.14  2005/01/02 21:09:46  agmsmith
 * Bump the version number.
 *
 * Revision 1.13  2004/12/12 21:44:39  agmsmith
 * Remove dead event loop timer, it does get stuck for long times when the
 * network connection is down.
 *
 * Revision 1.12  2004/12/05 23:40:04  agmsmith
 * Change timing system to use the event loop rather than a separate thread.
 * Didn't fix the memory crash bug when switching screen resolution - so it's
 * not stack size or multithreading.
 *
 * Revision 1.11  2004/11/27 22:53:12  agmsmith
 * Oops, forgot about the network time delay for new data.  Make it shorter so
 * that the overall update loop is faster.
 *
 * Revision 1.10  2004/11/22 02:40:40  agmsmith
 * Changed from Pulse() timing to using a separate thread, so now mouse clicks
 * and other time sensitive responses are much more accurate (1/60 second
 * accuracy at best).
 *
 * Revision 1.9  2004/09/13 01:41:53  agmsmith
 * Update rate time limits now in the desktop module.
 *
 * Revision 1.8  2004/07/19 22:30:19  agmsmith
 * Updated to work with VNC 4.0 source code (was 4.0 beta 4).
 *
 * Revision 1.7  2004/07/05 00:53:07  agmsmith
 * Check for a forced update too.
 *
 * Revision 1.6  2004/06/27 20:31:44  agmsmith
 * Got it working, so you can now see the desktop in different
 * video modes (except 8 bit).  Even lets you switch screens!
 *
 * Revision 1.5  2004/06/07 01:06:50  agmsmith
 * Starting to get the SDesktop working with the frame buffer
 * and a BDirectWindow.
 *
 * Revision 1.4  2004/02/08 19:43:57  agmsmith
 * FrameBuffer class under construction.
 *
 * Revision 1.3  2004/01/25 02:57:42  agmsmith
 * Removed loading and saving of settings, just specify the command line
 * options every time it is activated.
 *
 * Revision 1.2  2004/01/11 00:55:42  agmsmith
 * Added network initialisation and basic server code.  Now accepts incoming
 * connections!  But doesn't display a black remote screen yet.
 *
 * Revision 1.1  2004/01/03 02:32:55  agmsmith
 * Initial revision
 */

/* Posix headers. */

#include <errno.h>
#include <stdlib.h>
#include <posix/sys/types.h>
#include <posix/sys/socket.h>
#ifdef __HAIKU__
  #include <posix/sys/select.h>
#endif

/* VNC library headers. */

#include <network/TcpSocket.h>
#include <rfb/Logger_stdio.h>
#include <rfb/LogWriter.h>
#include <rfb/SSecurityFactoryStandard.h>
#include <rfb/VNCServerST.h>

/* BeOS (Be Operating System) headers. */

#include <Alert.h>
#include <Application.h>
#include <Clipboard.h>
#include <DirectWindow.h>
#include <Locker.h>

/* Our source code */

#include "SDesktopBeOS.h"


/******************************************************************************
 * Global variables, and not-so-variable things too.  Grouped by functionality.
 */

static const unsigned int MSG_DO_POLLING_STEP = 'VPol';
  /* The message code for the BMessage which triggers a polling code to check a
  slice of the screen for changes.  The BMessage doesn't have any data, and is
  sent when the polling work is done, so that the next polling task is
  triggered almost immedately. */

static const char *g_AppSignature =
  "application/x-vnd.agmsmith.vncserver";

static const char *g_AboutText =
  "VNC Server for BeOS, based on VNC 4.0 from RealVNC http://www.realvnc.com/\n"
  "Adapted for BeOS by Alexander G. M. Smith\n"
  "$Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0-beossrc/beosserver/RCS/ServerMain.cxx,v 1.28 2018/01/10 22:25:05 agmsmith Exp agmsmith $\n"
  "Compiled on " __DATE__ " at " __TIME__ ".";

static const int k_DeadManPulseTimer = 3000000;
  /* Time delay before the pulse timer checks that the main loop is still
  running.  If not, it will create a new polling BeOS message. */

static rfb::LogWriter vlog("ServerMain");

static rfb::IntParameter port_number("PortNumber",
  "TCP/IP port on which the server will accept connections",
  5900);

static rfb::IntParameter ThreadPriority ("ThreadPriority",
  "Priority of the main thread, a value from 1 to 20.  The main thread "
  "scans the video memory for changes to the picture on the screen and thus "
  "uses a lot of CPU time.  5 is low priority (the default).  10 is "
  "BeOS/Haiku normal thread priority.  15 is high priority.  We used to use "
  "10 as the default, but on some CPU speed limited systems, this would "
  "cause periodic clicks in the audio (not good for audio broadcasting "
  "software), so the default is now 5.",
  5);

static rfb::VncAuthPasswdFileParameter vncAuthPasswd;
  // Creating this object is enough to register it with the
  // SSecurityFactoryStandard class system, specifying that we
  // store passwords in a file.



/******************************************************************************
 * ServerApp is the top level class for this program.  This handles messages
 * from the outside world and does some of the processing.  It also has
 * pointers to important data structures, like the VNC server stuff, or
 * the desktop (screen buffer access thing).
 */

class ServerApp : public BApplication
{
public: /* Constructor and destructor. */
  ServerApp ();
  ~ServerApp ();

  /* BeOS virtual functions. */
  virtual void AboutRequested ();
  virtual void MessageReceived (BMessage *MessagePntr);
  virtual void Pulse ();
  virtual bool QuitRequested ();
  virtual void ReadyToRun ();

  /* Our class methods. */
  void PollNetwork ();
  void ShutDownNetwork();

public: /* Member variables. */
  enum NetStateEnum {NET_RESTARTING, NET_UP, NET_WENT_DOWN} m_BeOSNetworkState;
    /* In BeOS the network subsystem can be manually restarted if it fails
    (which it does often).  We have to close all sockets when it fails and then
    try reopening everything periodically until it's working again.  The states
    are:
    NET_WENT_DOWN - Just went down.  Set when a fatal error is detected.  When
      the polling loop notices this state, close everything and then move to
      the NET_RESTARTING state.
    NET_RESTARTING - After a brief delay (a few seconds), try opening sockets
      and starting up the network.  If it works, move to NET_UP, otherwise move
      to NET_WENT_DOWN.
    NET_UP - Normal state, sockets are open and listening for connections. */

  SDesktopBeOS *m_FakeDesktopPntr;
    /* Provides access to the frame buffer, mouse, etc for VNC to use. */

  bigtime_t m_TimeOfLastBackgroundUpdate;
    /* The server main loop updates this with the current time whenever it
    finishes an update (checking for network input and optionally sending a
    sliver of the screen to the client).  If a long time goes by without an
    update, the pulse thread will inject a new BMessage, just in case the chain
    of update BMessages was broken. */

  network::TcpListener *m_TcpListenerPntr;
    /* A socket that listens for incoming connections. */

  rfb::VNCServerST *m_VNCServerPntr;
    /* A lot of the pre-made message processing logic is in this object. */
};



/******************************************************************************
 * Implementation of the ServerApp class.  Constructor, destructor and the rest
 * of the member functions in mostly alphabetical order.
 */

ServerApp::ServerApp ()
: BApplication (g_AppSignature),
  m_BeOSNetworkState (NET_RESTARTING),
  m_FakeDesktopPntr (NULL),
  m_TimeOfLastBackgroundUpdate (0),
  m_TcpListenerPntr (NULL),
  m_VNCServerPntr (NULL)
{
}


ServerApp::~ServerApp ()
{
  // Deallocate our main data structures.

  delete m_TcpListenerPntr;
  delete m_VNCServerPntr;
  delete m_FakeDesktopPntr;
}


/* Display a box showing information about this program. */

void ServerApp::AboutRequested ()
{
  BAlert *AboutAlertPntr;

  AboutAlertPntr = new BAlert ("About", g_AboutText, "Done");
  if (AboutAlertPntr != NULL)
  {
    AboutAlertPntr->SetShortcut (0, B_ESCAPE);
    AboutAlertPntr->Go ();
  }
}


void ServerApp::MessageReceived (BMessage *MessagePntr)
{
  try
  {
    if (MessagePntr->what == MSG_DO_POLLING_STEP)
      PollNetwork ();
    else if (MessagePntr->what == B_CLIPBOARD_CHANGED)
    {
      BMessage   *ClipMsgPntr;
      int32       TextLength;
      const char *TextPntr;

      if (m_VNCServerPntr != NULL && be_clipboard->Lock())
      {
        if ((ClipMsgPntr = be_clipboard->Data ()) != NULL)
        {
          TextPntr = NULL;
          ClipMsgPntr->FindData ("text/plain", B_MIME_TYPE,
            (const void **) &TextPntr, &TextLength);
          if (TextPntr != NULL)
            m_VNCServerPntr->serverCutText (TextPntr, TextLength);
        }
        be_clipboard->Unlock ();
      }
    }
    else
      /* Pass the unprocessed message to the inherited function, maybe it knows
      what to do.  This includes replies to messages we sent ourselves. */
      BApplication::MessageReceived (MessagePntr);

  } catch (rdr::SystemException &s) {
    vlog.error(s.str());
  } catch (rdr::Exception &e) {
    vlog.error(e.str());
  } catch (...) {
    vlog.error("Unknown or unhandled exception while processing a "
      "BeOS message.");
  }
}


void ServerApp::PollNetwork ()
{
  if (m_VNCServerPntr == NULL)
    return;

  if (m_BeOSNetworkState == NET_RESTARTING)
  {
    vlog.debug("In state NET_RESTARTING.");

    if (m_TcpListenerPntr != NULL)
      delete m_TcpListenerPntr;
    m_TcpListenerPntr = new network::TcpListener ((int)port_number);

    vlog.info("Listening on port %d", (int)port_number);
    m_BeOSNetworkState = NET_UP;
    return;
  }

  if (m_BeOSNetworkState == NET_WENT_DOWN)
  {
    vlog.debug("In state NET_WENT_DOWN.");
    ShutDownNetwork();
    m_BeOSNetworkState = NET_RESTARTING;
    return;
  }

  try
  {
    int            cur_fd;
    int            highest_fd;
    fd_set         rfds;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 5000; // Time delay in millionths of a second, keep short.

    FD_ZERO(&rfds);
    highest_fd = 0;

    cur_fd = m_TcpListenerPntr->getFd();
    FD_SET(cur_fd, &rfds);
    if (cur_fd > highest_fd)
      highest_fd = cur_fd;

    std::list<network::Socket*> sockets;
    m_VNCServerPntr->getSockets(&sockets);
    std::list<network::Socket*>::iterator iter;
    for (iter = sockets.begin(); iter != sockets.end(); iter++)
    {
      cur_fd = (*iter)->getFd();
      FD_SET(cur_fd, &rfds);
      if (cur_fd > highest_fd)
        highest_fd = cur_fd;
    }

    if (highest_fd >= FD_SETSIZE) // Probably trashed stack if this happened.
      throw rdr::SystemException("FD_SETSIZE Exceeded", -1);

    int n = select(highest_fd + 1, &rfds, 0, 0, &tv);
    if (n < 0) throw rdr::SystemException("select", errno);

    for (iter = sockets.begin(); iter != sockets.end(); iter++) {
      if (FD_ISSET((*iter)->getFd(), &rfds)) {
        m_VNCServerPntr->processSocketEvent(*iter);
      }
    }

    if (FD_ISSET(m_TcpListenerPntr->getFd(), &rfds)) {
      network::Socket* sock = m_TcpListenerPntr->accept();
      if (sock != NULL) // NULL if it fails security checks.  Can throw too.
        m_VNCServerPntr->addClient(sock);
    }

    m_VNCServerPntr->checkTimeouts();

    // Run the background scan of the screen for changes, but only when an
    // update is requested.  Otherwise the update timing feedback system won't
    // work correctly (bursts of ridiculously high frame rates when the client
    // isn't asking for a new update).

    if (m_VNCServerPntr->clientsReadyForUpdate ())
      m_FakeDesktopPntr->BackgroundScreenUpdateCheck ();

    // Trigger the next update pretty much immediately, after other intervening
    // messages in the queue have been processed.

    m_TimeOfLastBackgroundUpdate = system_time ();
    PostMessage (MSG_DO_POLLING_STEP);
  }
  catch (rdr::Exception &e)
  {
    vlog.error(e.str());
    m_BeOSNetworkState = NET_WENT_DOWN;
  }
}


void ServerApp::Pulse ()
{
  bigtime_t ElapsedTime = system_time () - m_TimeOfLastBackgroundUpdate;

  if (m_TimeOfLastBackgroundUpdate == 0 ||
  ElapsedTime > (bigtime_t) (1.5 * k_DeadManPulseTimer))
  {
    vlog.debug ("ServerApp::Pulse: Restarting the BMessage timing cycle "
      "after %0.2f idle seconds.", ElapsedTime / 1000000.0);
    m_TimeOfLastBackgroundUpdate = system_time ();
    PostMessage (MSG_DO_POLLING_STEP);
  }
}


/* A quit request message has come in. */

bool ServerApp::QuitRequested ()
{
  be_clipboard->StopWatching (be_app_messenger);
  ShutDownNetwork();
  return BApplication::QuitRequested ();
}


void ServerApp::ReadyToRun ()
{
  try
  {
    /* VNC Setup. */

    m_FakeDesktopPntr = new SDesktopBeOS ();

    m_VNCServerPntr = new rfb::VNCServerST ("MyBeOSVNCServer",
      m_FakeDesktopPntr, NULL /* security factory */);

    m_FakeDesktopPntr->setServer (m_VNCServerPntr);

    network::TcpSocket::initTcpSockets();

    be_clipboard->StartWatching (be_app_messenger);

    SetPulseRate (k_DeadManPulseTimer); // Deadman timer checks occasionally.
  }
  catch (rdr::Exception &e)
  {
    vlog.error(e.str());
    PostMessage (B_QUIT_REQUESTED);
  }
}


void ServerApp::ShutDownNetwork()
{
  if (m_VNCServerPntr != NULL)
  {
    // Close all the clients.  This just sets a flag.
    m_VNCServerPntr->closeClients("BeOS Network went down, "
      "perhaps due to a NetServer restart or shutting down the program.");

    // Do an update to actually close the clients attached to the dead
    // sockets, also deletes the sockets.
    std::list<network::Socket*> sockets;
    m_VNCServerPntr->getSockets(&sockets);
    std::list<network::Socket*>::iterator iter;
    for (iter = sockets.begin(); iter != sockets.end(); iter++)
        m_VNCServerPntr->processSocketEvent(*iter);
  }

  // Also close our listening for new connections socket.  That's all of the
  // sockets that interact with the BeOS networking system.
  if (m_TcpListenerPntr != NULL)
  {
    m_TcpListenerPntr->shutdown();
    delete m_TcpListenerPntr;
    m_TcpListenerPntr = NULL;
  }

  // Just in case someone cancelled shutdown, this will let it restart.
  m_BeOSNetworkState = NET_WENT_DOWN;

  vlog.debug("Successfully shut down the network.");
}


// Display the program usage info, then terminate the program.

static void usage (const char *programName)
{
  fprintf(stdout, g_AboutText);
  fprintf(stdout, "\n\nusage: %s [<parameters>]\n", programName);
  fprintf(stdout,"\n"
    "Parameters can be turned on with -<param> or off with -<param>=0\n"
    "Parameters which take a value can be specified as "
    "-<param> <value>\n"
    "Other valid forms are <param>=<value> -<param>=<value> "
    "--<param>=<value>\n"
    "Parameter names are case-insensitive.  The parameters are:\n\n");
  rfb::Configuration::listParams(79, 14);
  exit(1);
}



/******************************************************************************
 * Finally, the main program which drives it all.
 */

int main (int argc, char** argv)
{
  ServerApp MyApp;
  int       ReturnCode = 0;

  if (MyApp.InitCheck () != B_OK)
  {
    // Unable to initialise BApplication.  No error logging facilities yet.
    return -1;
  }

  try {
    rfb::initStdIOLoggers();
    rfb::LogWriter::setLogParams("*:stdout:30");
      // Normal level is 30, use 1000 for debug messages.  Or on the command
      // line to see everything try: --log=*:stdout:1000

    // Override the default parameters with new values from the command line.
    // Display the usage message and exit the program if an unknown parameter
    // is encountered.

    for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
        if (rfb::Configuration::setParam(argv[i]))
          continue;
        if (i+1 < argc) {
          if (rfb::Configuration::setParam(&argv[i][1], argv[i+1])) {
            i++;
            continue;
          }
        }
        usage(argv[0]);
      }
      if (rfb::Configuration::setParam(argv[i]))
        continue;
      usage(argv[0]);
    }

    // Set the priority of the main polling thread.  If it's normal, than some
    // computers don't have enough CPU time for other things and you get clicks
    // in the audio.  So the default is to be lower than normal and we now need
    // an option to set it back to normal.

    thread_id MyThread = find_thread (NULL);
    int32 NewPriority = ThreadPriority;
    if (NewPriority < 1)
      NewPriority = 1;
    else if (NewPriority > 20)
      NewPriority = 20;
    vlog.debug ("Changing priority of main thread to %d.", NewPriority);
    set_thread_priority (MyThread, NewPriority);

    MyApp.Run (); // Run the main event loop.
    ReturnCode = 0;
  }
  catch (rdr::SystemException &s) {
    vlog.error(s.str());
    ReturnCode = s.err;
  } catch (rdr::Exception &e) {
    vlog.error(e.str());
    ReturnCode = -1;
  } catch (...) {
    vlog.error("Unknown or unhandled exception detected at top level, "
      "exiting.");
    ReturnCode = -1;
  }

  return ReturnCode;
}
