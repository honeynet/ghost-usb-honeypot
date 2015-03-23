# GhostLib or how to control Ghost programmatically #

Would you like to control Ghost from your application, possibly to make it collaborate with your analysis setup? We offer a DLL that exports functions to mount virtual devices and obtain results from the honeypot. By the way, the graphical frontend for Ghost is based on that library.

If you downloaded and extracted a precompiled version of Ghost, then you'll find the files `ghostlib.{lib,dll,h}` in your Ghost folder. The DLL contains the code proper, while the LIB is the corresponding import library. You can use them with any programming language that supports the stdcall calling convention. Provide your linker with the LIB and make sure that the DLL is accessible at runtime.

The file `ghostlib.h` documents the library's interface. Have a look at the comments for details on each function's operation. The basic scheme is as follows:
  * Call GhostMountDevice and register a callback function. Ghost will mount a virtual device and invoke your callback in a separate thread whenever a yet unknown process writes data to the device.
  * Within your callback, you may use !GhostGetProcessID, !GhostGetThreadID and GhostGetModuleName to obtain information about the writing process.
  * Finally, call GhostUmountDevice to remove the virtual device.

Please refer to [my blog post on GhostLib](https://honeynet.org/node/908) for basic sample code. Note that I've written some code to use GhostLib from C# for the graphical frontend (see `Ghost.cs` in the source code) -- it should be quite easy to create bindings for other languages as well, so stay tuned for updates...

_If you should write bindings for any language and would like to support the community, I'll be glad to publish your results here!_