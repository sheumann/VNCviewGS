VNCview GS
==========

VNCview GS is a [Virtual Network Computing][0] client (viewer) for the Apple IIgs.  You can use it to display and interact with the graphical desktop of another computer through your Apple IIgs.

[0]: https://en.wikipedia.org/wiki/Virtual_Network_Computing

__Binary downloads are on the [releases page][releases].__

[releases]: https://github.com/sheumann/VNCviewGS/releases

System Requirements
-------------------
* An Apple IIgs running System Software 6.0.1 or later
* [Marinetti][1] ([version 3.0b9 or later][2] recommended for best performance)
* Andrew Roughan's [Crypto Tool Set][3] (needed for password functionality)
* A computer running a VNC server to which you have access  

Strongly recommended:

* An accelerator
* A high-bandwidth, low-latency network connection from the IIgs to the server

A IIgs connected to the network via an Ethernet connection and a server connected to your LAN will probably provide the best performance.

Under Sweet16, there is a known issue where the networking code within Sweet16 may drop the connection to the VNC server.  This generally occurs at relatively low emulated speeds; you can usually avoid it by using high or unlimited speed.

[1]: http://www.apple2.org/marinetti/
[2]: http://www.a2retrosystems.com/Marinetti.htm
[3]: http://members.iinet.net.au/~kalandi/apple/crypto.html


Usage
-----
To start VNCview GS, simply run the __VNCview.GS__ program.  The New VNC Connection dialog box will be displayed, allowing you to configure and start a VNC connection.

Usually, you need to enter the VNC server (using a hostname or IP address and a VNC display number, as in other VNC programs) and a password.  Some servers may not require a password, but most do and it is probably advisable to set up your own servers with a password to provide some security.  Also note that the VNC display number can be omitted if it is 0, and that it can be negative.

You can select a 320x200 color or 640x200 grayscale display mode.  The 320x200 mode better corresponds to the server's normal display, since it displays colors and uses near-square pixels.  640x200 mode will cause greater distortion and does not display colors, but it does allow a larger portion of the server's desktop to be displayed on the IIgs.

Other options are also available.
* __Request shared session__ indicates the server should allow multiple clients to connect to it simultaneously.  The server is not required to honor this request.
* __Emulate 3-Button Mouse__ causes option-clicks and OA-clicks to be  treated as clicks of the second and third mouse buttons.
* __View Only Mode__ allows you to see the server's display but not to send any keyboard, mouse, or clipboard input to the server.
* __Allow Clipboard Transfers from Server__ indicates that the server is allowed to send its clipboard contents, which will be transferred to the IIgs clipboard.

The __Preferred Encoding__ is the method that will be used to represent pixels on the server's display when sending them to the IIgs; the available options are Raw and Hextile.  The Raw encoding sends lines of pixel values to the IIgs, while Hextile represents the display as a collection of small rectangular areas.  Raw is generally faster than Hextile in the current version of VNCview GS, but Hextile may be faster on very slow network links.  Some servers may not support Hextile encoding, in which case Raw will be used regardless of this setting.

The __Tune Marinetti for high throughput__ option configures Marinetti to process a larger amount of incoming data at once than it does by default.  This generally improves performance.

When you have configured your new VNC connection, simply click __Connect__, and if all goes well you will be connected to the VNC server.  You can interact with it with the mouse and keyboard and scroll your view of its display.  The option and Open-Apple keys are sent as "meta" and "alt," respectively; their exact interpretation depends on the server.  Keyboard shortcuts for menu items are disabled when connected so that these key combinations can be sent to the server.  Select _Close_ or _Quit_ in the File menu when you are done with the connection.

Menu Items
----------
__Apple->About VNCview GS...__: Displays information about the program  
__File->New Connection__: Allows you to configure and establish a new connection  
__File->Close__: Closes current window or connection.  You can connect again afterward.  
__File->Quit__: Quits VNCview GS, closing any open connection  
__Edit->Undo__: Only used by NDAs  
__Edit->Cut/Copy/Paste/Clear__: Used in New VNC Connection window input boxes and NDAs  
__Edit->Show Clipboard__: Displays the current contents of the IIgs clipboard  
__Edit->Send Clipboard__: Transfers the contents of the IIgs clipboard to the server  

VNC Server Interoperability
---------------------------
VNCview GS should be able to connect with any VNC server that fully implements the RFB protocol as defined in RFC 6143 or earlier compatible specifications.  Here is a list of some VNC servers; many others are also available.

* Microsoft Windows, *nix (X11), and macOS: __RealVNC__, http://www.realvnc.com/
* macOS: __Vine Server (OSXvnc)__, http://www.testplant.com/dlds/vine/
* *nix (X11) and macOS: __x11vnc__, http://www.karlrunge.com/x11vnc/

x11vnc is a good option, because it supports server-side display scaling, which is useful to fit more information on the IIgs screen.

VNCview GS does not work with the Screen Sharing functionality in macOS, because the Screen Sharing server does not support the pixel format that VNCview GS requires.  The above servers can be used instead.

Version History
---------------
##### 1.0.2
* Support for typing non-ASCII characters
* Smoother transitions between 640 mode and 320 mode
* Improved UI responsiveness during raw pixel decoding

##### 1.0.1
* Improved clipboard transfer routines, with support for non-ASCII characters
* Improvements to help the display "catch up" when content is changing rapidly
* Fix to avoid long pauses or hangs when closing the connection in some cases
* Fix to ensure Marinetti resources are properly cleaned up in error cases

##### 1.0
* Optimized raw pixel decoding routines
* Raw pixels can be decoded and displayed incrementally while receiving data
* Added option to tune Marinetti for high throughput

##### 1.0b2
* First open source release
* Can display the mouse cursor from the server locally on the IIgs
* Detects some errors more quickly, avoiding long periods of unresponsiveness
* Mapping of OA and Option keys reversed to match Mac VNC implementations
* Raw pixel decoding performance improved slightly

##### 1.0b1
* Faster display updates when using Raw encoding
* Servers with screen dimensions smaller that the IIgs screen are now supported.
* The server's display can be resized while connected to it (certain servers only)

##### 1.0a2
* Hextile encoding support added
* Faster display update when scrolling while using Raw encoding
* Bug causing problems with 2nd and subsequent connections fixed

##### 1.0a1
* First public release

License
-------
Copyright (c) 2002-2016 Stephen Heumann  
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
This program contains material from the ORCA/C Run-Time Libraries, copyright 1987-1996 by Byte Works, Inc.  Used with permission.
