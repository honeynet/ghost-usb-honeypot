# How to use GhostTool #

If you have successfully installed the Ghost USB honeypot, you may use the command line tool GhostTool to control its operation. Basically, it enables you to mount and unmount emulated USB flash drives. If you unmount a device that has been written to, GhostTool will inform you that your machine might be infected with USB malware.

In order to mount a virtual device, simply execute
```
ghosttool mount n
```
where n is a number between 0 and 9. If you want to unmount the device again, run
```
ghosttool umount n
```

When device n is mounted for the first time, you will have to format it (unless you placed a preformatted image file in the corresponding location). Also, the hardware wizard might ask you for a driver -- just choose automatic installation and everything will be fine.

After a virtual device has been infected with malware, you can copy the image to another machine and mount it for analysis. You might want to use a non-Windows machine, though...