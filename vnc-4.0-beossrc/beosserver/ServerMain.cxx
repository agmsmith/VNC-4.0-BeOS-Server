/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/ServerMain.cxx,v 1.3 2004/01/25 02:57:42 agmsmith Exp agmsmith $
 *
 * This is the main program for the BeOS version of the VNC server.  The basic
 * functionality comes from the VNC 4.0b4 source code (available from
 * http://www.realvnc.com/), with BeOS adaptations by Alexander G. M. Smith.
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
#include <socket.h>

/* VNC library headers. */

#include <network/TcpSocket.h>
#include <rfb/FrameBuffer.h>
#include <rfb/Logger_stdio.h>
#include <rfb/LogWriter.h>
#include <rfb/SDesktop.h>
#include <rfb/SSecurityFactoryStandard.h>
#include <rfb/VNCServerST.h>

/* BeOS (Be Operating System) headers. */

#include <Alert.h>
#include <Application.h>

/* Our source code */

#include "FrameBufferBeOS.h"



/******************************************************************************
 * Global variables, and not-so-variable things too.  Grouped by functionality.
 */

static const char *g_AppSignature =
  "application/x-vnd.agmsmith.vncserver";

static const char *g_AboutText =
  "VNC Server for BeOS, based on VNC 4.0b4\n"
  "Adapted for BeOS by Alexander G. M. Smith\n"
  "$Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0b4-beossrc/beosserver/RCS/ServerMain.cxx,v 1.3 2004/01/25 02:57:42 agmsmith Exp agmsmith $\n"
  "Compiled on " __DATE__ " at " __TIME__ ".";

static rfb::LogWriter vlog("ServerMain");

static rfb::IntParameter http_port("HTTPPortNumber",
  "TCP/IP port on which the server will serve the Java applet VNC Viewer ",
  5800);

static rfb::IntParameter port_number("PortNumber",
  "TCP/IP port on which the server will accept connections",
  5900);

static rfb::StringParameter hosts("Hosts",
  "Filter describing which hosts are allowed access to this server",
  "+");

static rfb::VncAuthPasswdFileParameter vncAuthPasswd;



/******************************************************************************
 * ServerApp is the top level class for this program.  This handles messages
 * from the outside world and does some of the processing.  It also has
 * pointers to important data structures, like the VNC server stuff, or the
 * desktop (screen buffer access thing).
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

public: /* Member variables. */
  rfb::SStaticDesktop *m_FakeDesktopPntr;
    /* Provides access to the frame buffer, mouse, etc for VNC to use. */

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
  m_FakeDesktopPntr (NULL),
  m_TcpListenerPntr (NULL),
  m_VNCServerPntr (NULL)
{
}


ServerApp::~ServerApp ()
{
  delete m_TcpListenerPntr;
  delete m_VNCServerPntr;
  delete m_FakeDesktopPntr;
}


/* Display a box showing information about this program. */

void ServerApp::AboutRequested ()
{
  BAlert *AboutAlertPntr;

  FrameBufferBeOS Bleeble;

  AboutAlertPntr = new BAlert ("About", g_AboutText, "Done");
  if (AboutAlertPntr != NULL)
  {
    AboutAlertPntr->SetShortcut (0, B_ESCAPE);
    AboutAlertPntr->Go ();
  }
}


void ServerApp::MessageReceived (BMessage *MessagePntr)
{
  switch (MessagePntr->what)
  {
  }

  /* Pass the unprocessed message to the inherited function, maybe it knows
  what to do.  This includes replies to messages we sent ourselves. */

  BApplication::MessageReceived (MessagePntr);
}


void ServerApp::Pulse ()
{
  try
  {
    fd_set rfds;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 50*1000;

    FD_ZERO(&rfds);
    FD_SET(m_TcpListenerPntr->getFd(), &rfds);

    std::list<network::Socket*> sockets;
    m_VNCServerPntr->getSockets(&sockets);
    std::list<network::Socket*>::iterator iter;
    for (iter = sockets.begin(); iter != sockets.end(); iter++)
      FD_SET((*iter)->getFd(), &rfds);

    int n = select(FD_SETSIZE, &rfds, 0, 0, &tv);
    if (n < 0) throw rdr::SystemException("select",errno);

    for (iter = sockets.begin(); iter != sockets.end(); iter++) {
      if (FD_ISSET((*iter)->getFd(), &rfds)) {
        m_VNCServerPntr->processSocketEvent(*iter);
      }
    }

    if (FD_ISSET(m_TcpListenerPntr->getFd(), &rfds)) {
      network::Socket* sock = m_TcpListenerPntr->accept();
      m_VNCServerPntr->addClient(sock);
    }

    m_VNCServerPntr->checkIdleTimeouts();
  }
  catch (rdr::Exception &e)
  {
    vlog.error(e.str());
  }
}


/* A quit request message has come in. */

bool ServerApp::QuitRequested ()
{
  return BApplication::QuitRequested ();
}


void ServerApp::ReadyToRun ()
{
  try
  {
    /* VNC Setup. */

    m_FakeDesktopPntr = new rfb::SStaticDesktop (rfb::Point (640, 480));

    m_VNCServerPntr = new rfb::VNCServerST ("MyBeOSVNCServer",
      m_FakeDesktopPntr, NULL);

    m_FakeDesktopPntr->setServer (m_VNCServerPntr);

    network::TcpSocket::initTcpSockets();
    m_TcpListenerPntr = new network::TcpListener ((int)port_number);
    vlog.info("Listening on port %d", (int)port_number);
  }
  catch (rdr::Exception &e)
  {
    vlog.error(e.str());
    PostMessage (B_QUIT_REQUESTED);
  }
  SetPulseRate (1000000);
}


// Display the program usage info, then terminate the program.

static void usage (const char *programName)
{
  fprintf(stderr, g_AboutText);
  fprintf(stderr, "\n\nusage: %s [<parameters>]\n", programName);
  fprintf(stderr,"\n"
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
    vlog.error("Unable to initialise BApplication.");
    return -1;
  }

  try {
    rfb::initStdIOLoggers();
    rfb::LogWriter::setLogParams("*:stderr:1000"); // Normal level is 30.

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

    MyApp.Run (); // Run the main event loop.
    ReturnCode = 0;
  }
  catch (rdr::SystemException &s) {
    vlog.error(s.str());
    ReturnCode = s.err;
  } catch (rdr::Exception &e) {
    vlog.error(e.str());
    ReturnCode = -1;
  }

  return ReturnCode;
}
