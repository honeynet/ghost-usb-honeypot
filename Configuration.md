# Configuration #

Ghost allows you to choose the location of the emulated devices' image files. The configuration is stored in the registry key `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\ghostdrive\Parameters`. By changing the values of `ImageFileName[0-9]`, you can configure where Ghost looks for an image file of the respective device.

The value `ImageFileName9` is also adjustable in the graphical frontend. See the [corresponding wiki page](GraphicalFrontend.md) for details.

**Note:** The files that you specify don't have to exist -- Ghost will create empty image files if they don't. However, you do have to make sure that the containing directory exists. Otherwise, Ghost will not be able to create an image file.