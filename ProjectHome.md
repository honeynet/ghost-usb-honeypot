# Ghost USB honeypot #

Ghost is a honeypot for malware that spreads via USB storage devices. It detects infections with such malware without the need of any further information. If you would like to see a video introduction to the project, have a look at [this Youtube video](http://www.youtube.com/watch?v=9G9oo3b9qR4).

The honeypot was first developed for a bachelor thesis at Bonn University in Germany. Now development is continued by the same developer within the [Honeynet Project](https://honeynet.org/).

Ghost was recently selected for Rapid7's Magnificent7 program (see the [press release](http://www.rapid7.com/news-events/press-releases/2012/2012-second-round-magnificent7-program.jsp)). Our goal for the next year is to extend the honeypot to a USB protection system, i.e. a system that protects networked computer environments from the threat of USB malware.

![http://wiki.ghost-usb-honeypot.googlecode.com/git/Screenshot.png](http://wiki.ghost-usb-honeypot.googlecode.com/git/Screenshot.png)

## How does it work? ##

Basically, the honeypot emulates a USB storage device. If your machine is infected by malware that uses such devices for propagation, the honeypot will trick it into infecting the emulated device. See the wiki for details.

## What do I need to run it? ##

Ghost supports Windows XP 32 bit and Windows 7 32 bit. You can either download a binary distribution or compile the code yourself. If you choose to build the code, you will need the Windows Driver Kit. For detailed instructions on how to do so, refer to the [build](BuildGuide.md) and [install](InstallGuide.md) guides in the wiki.

## Contact ##

You can [follow me on Twitter](https://twitter.com/poeplau) to get the latest updates on the project. If you have questions or suggestions, feel free to post to the project's [mailing list](https://public.honeynet.org/mailman/listinfo/usb-honeypot). I really appreciate feedback!

If you'd like to report a bug, please use the project's [issue tracker](http://code.google.com/p/ghost-usb-honeypot/issues/list). I will look at the issue as soon as possible.

## Credits ##

The project's logo was created by Mark Eibes. The project is supported by Rapid7 as a member of their Magnificent7 program.

[![](http://wiki.ghost-usb-honeypot.googlecode.com/git/powered-by-rapid7.png)](https://community.rapid7.com/community/open_source/magnificent7)