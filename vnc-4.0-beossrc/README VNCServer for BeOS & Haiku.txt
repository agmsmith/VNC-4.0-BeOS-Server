Short: VNC Remote Screen and Keyboard access for BeOS and Haiku OS.
Author: agmsmith@ncf.ca (Alexander G. M. Smith)
Uploader: agmsmith@ncf.ca (Alexander G. M. Smith)
Website: http://web.ncf.ca/au829/
Version: 4.0-BeOS-AGMS-1.30
Type: Internet & Network / Servers / Remote Access
Requires: BeOS R5 (Intel or PowerPC) or Haiku OS R1 (x86 32 bit, gcc2 build)

VNCServer lets you use your BeOS or Haiku computer from anywhere there is an Internet connection.  You can think of it as a really long keyboard, mouse and video cable.  A VNC client (available elsewhere for Windows, Mac, Linux, others) shows you the BeOS screen and sends keystrokes and mouse actions to your BeOS system over the Internet.  The VNCServer software running on BeOS/Haiku takes that data from the client, and simulates button presses on a fake keyboard and movements of an imaginary mouse.  In the opposite direction, it scans your BeOS/Haiku screen for changes, compresses the resulting graphics data and transmits it to the client.

This is a port of VNC using RealVNC's version 4.0 final source code (which has an extremely well designed class structure, making it easy to do this port).  There are lots of VNC clients out there, but I can recommend the RealVNC ones as working very well under Windows.  You can get their clients, servers and source code at http://www.realvnc.com/

VNCServer is released under the GNU General Public License.  See LICENSE.TXT for details.

Installation and Use

To install it do these steps.  To uninstall, remove the files you had installed and reboot.  To use it after having installed it, just do the last step:

1.  Unzip the archive file.

2.  Look in the Installables folder for a folder containing the install files for your computer's operating system version.  Move the relevant files to the related places.  In more detail:

2a For BeOS and Haiku OS up to R1 Alpha 4.1, you need to manually move three files.

Move the "InputEventInjector" file into the /boot/home/config/add-ons/input_server/devices/ folder.  You can just drag and drop it into the conveniently included symbolic link to that directory.  This small program is the fake keyboard and mouse simulator, which can also be used by other programs to inject other user action events into the BeOS/Haiku Input Server.

Move the vncpasswd and vncserver files to /boot/home/config/bin (or wherever you keep command line utilities).

2b For Haiku OS after R1 Alpha 4.1 (the package manager system exists), you need to manually move one file.  Move the "vncserver-VNC4_0...hpkg" file into the /boot/system/packages/ folder.  You can just drag and drop it into the conveniently included symbolic link to that directory.  Or double click on it and use HaikuDepot to install it.

3.  Start up Terminal and run the "vncpasswd" command.  It will prompt you for a password for people to use when they want to access your BeOS/Haiku computer over the Internet.  Without a password, anyone could take control of your computer!  It saves the password in encrypted form in the /boot/home/config/settings/VNCServer/passwd file.

4.  Restart the input server so that it notices the new InputEventInjector fake keyboard/mouse thing.  If you're using Haiku the only way to do this is to reboot.  For BeOS you can reboot or type in this command:
/system/servers/input_server -q

5.  Start up the "vncserver" program whenever you want to allow people to connect.  If you want to see error messages (such as one telling you that your video card isn't supported), or specify extra startup options (such as showing a more visible cursor or changing the network port number) or just want to see its help file, start it in a Terminal window (use "vncserver -h" for help).  When you're done, use the Process Controller Deskbar add-on to send a "Quit Application" command to vncserver (it's running as a background program so you don't see it listed in the regular Deskbar) to make it shut down.

Notes:

To see all the debugging output, use the --log command line option like this:
vncserver --log=*:stdout:1000

Change History:

Version 1.30, October 2018: Fixed to work with Haiku R1B1.  The select() call is fussier, so now count up the number of FDs we're actually using rather than asking it to select() the maximum number possible.

Version 1.27, September 2015: Added keypad support and fixed things like control-home getting mangled (recognised control keys reduced to just aA-zZ[{\|]}6^-_2@ backspace delete and space).  Improved BScreen srcreen grab method to grab every half second (previously it grabbed only after it had sent the full screen), so you can now scrub with the mouse and see much more recent screen contents on very slow (dial-up) connections.

Version 1.26, October 2014: Updated documentation for using with Haiku and package manager.  Also build an .hpkg file for it.

Version 1.25, July 2014: Added a ThreadPriority command line option.  Also made the default 5 (low priority), while previously it had effectively been 10 (normal priority).  So now it won't slow down other running apps as much, which in one case was causing click noises in audio broadcasting software.

Version 1.24, April 2013: Fixed a security annoyance and updated build system to use Jam and work on Haiku.  Now you can build it for BeOS PPC, x86 and Haiku x86 gcc2 and gcc4.  The security problem was a CPU hogging infinite loop after the idle timeout happened, triggered by someone connecting and doing nothing.

Version 1.23, February 2013: Check for errors when doing networking operations and shut down all network connections when that happens.  Then a few seconds later, restart networking operations.  This means it can now survive a BeOS "restart networking"  operation.

As well, show a dummy bitmap (vertical stripes) if the screen is unavailable and also break the bitmap mutual exclusion lock if the time is getting close to the 0.5 second limit in Haiku OS.  That avoids problems with Haiku killing VNC when switching screen resolutions, at the cost of some garbage on the screen.

Fixed a bug with some B_UNMAPPED_KEY messages not being sent, for modifier keys and function keys, due to an array size miscalculation.

Implement automatic shifting for characters that come in with incorrect shift states.  Happens for the iPad where you can type an uppercase letter without the client sending the shift key press before the letter.  Now it searches all the keymaps (plain, shift, option, capslock, option-shift, caps-shift, etc) to find the symbol and then temporarily presses/releases the appropriate modifier keys to type the symbol in BeOS.

Fake cursor now defaults to off, because Haiku R1A4 doesn't have hardware cursors so it draws it on the screen so VNC copies it.  If you're using BeOS with good video hardware drivers, you may wish to turn it on so you can see the cursor position more easily.

Version 1.22, November 2011: The fake cross cursor is now on by default ("vncserver ShowCheapCursor=1") and it has been changed to a larger and more colourful shape for better visibility when using an iPad VNC client.

- Alex (Ottawa, October 2018)
