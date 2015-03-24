# Ghost USB honeypot #

Ghost is a honeypot for malware that spreads via USB storage devices. It detects infections with such malware without the need of any further information. If you would like to see a video introduction to the project, have a look at [this Youtube video](http://www.youtube.com/watch?v=9G9oo3b9qR4).

The honeypot was first developed for a bachelor thesis at Bonn University in Germany. Now development is continued by the same developer within the [Honeynet Project](https://honeynet.org/).

Ghost was one of the projects supported by Rapid7's Magnificent7 program (see the [press release](http://www.rapid7.com/news-events/press-releases/2012/2012-second-round-magnificent7-program.jsp)).

![http://wiki.ghost-usb-honeypot.googlecode.com/git/Screenshot.png](http://wiki.ghost-usb-honeypot.googlecode.com/git/Screenshot.png)

## How does it work? ##

Basically, the honeypot emulates a USB storage device. If your machine is infected by malware that uses such devices for propagation, the honeypot will trick it into infecting the emulated device. See the wiki for details.

## What do I need to run it? ##

Ghost supports Windows XP 32 bit and Windows 7 32 bit. You can either download a binary distribution from the [old website](https://code.google.com/p/ghost-usb-honeypot/downloads/list) or compile the code yourself. If you choose to build the code, you will need the Windows Driver Kit. For detailed instructions on how to do so, refer to the [build](wiki/BuildGuide) and [install](wiki/InstallGuide) guides in the wiki.

## Credits ##

The project's logo was created by Mark Eibes. The project is supported by Rapid7 as a member of their Magnificent7 program.

[![](http://wiki.ghost-usb-honeypot.googlecode.com/git/powered-by-rapid7.png)](https://community.rapid7.com/community/open_source/magnificent7)
