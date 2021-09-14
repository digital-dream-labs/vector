# Victor OS Developer Tips

Random tips and tricks for people working on Victor OS development

## GitHub repo
https://github.com/anki/vicos-oelinux

Note this repo contains CASE-SENSITIVE file names!  If you clone to a case-insensitive file system (eg MacOS default partition), you will see name collisions and files that appear to have changed by themselves.

Per Daniel Casner, "OS development and really everything for Victor should be happening on a case sensitive file system."

## VM Development Hosts
IT has provisioned some linux VM hosts for OS development:

https://anki.slack.com/files/U03PWFFKB/F9B21CECF/Vic_OS_development_hosts

## ExtFS Filesystem Utilities
OS build artifacts include ext4 filesystem images, stored in Android sparse image format.

Some filesystem utilities "simg2img" and "img2simg" are available in https://www.dropbox.com/home/ext4-utils-osx. 

You can use "simg2img" to convert sparse image format to regular image format for use with MacOS:


```
#!/usr/bin/env bash
$ file apq8009-robot-sysfs.ext4
apq8009-robot-sysfs.ext4: Android sparse image, version: 1.0, Total of 229376 4096-byte output blocks in 8917 input chunks.
 
$ simg2img apq8009-robot-sysfs.ext4 apq8009-robot-sysfs.ext4.img
 
$ file apq8009-robot-sysfs.ext4.img
apq8009-robot-sysfs.ext4.img: Linux rev 1.0 ext4 filesystem data, UUID=57f8f4bc-abf4-655f-bf67-946fc0f9f25b (extents) (large files)
```

If you install an ext4 filesystem extension such as such as Paragon ExtFS for Mac, you can mount the IMG file as a virtual filesystem:

```
#!/usr/bin/env bash
$ open apq8009-robot-sysfs.ext4.img
[apq800q-sysfs.ext4.img mounted as /Volumes/Untitled]
 
$ ls /Volumes/Untitled
WEBSERVER   dev     lost+found  sbin        tmp
bin     etc     media       sdcard      usr
boot        firmware    mnt     share       var
build.prop  home        persist     sys
cache       lib     proc        system
data        linuxrc     run     target
```

You can also mount ext4 images on a linux VM under VMWare or VirtualBox.