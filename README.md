# gimp-jxr
GIMP plugin for reading and writing of JPEG XR image files. The plugin is also available on the [GIMP Plugin Registry](http://registry.gimp.org/node/25508). 

[![license](https://img.shields.io/github/license/chausner/gimp-jxr.svg)](https://github.com/chausner/gimp-jxr/blob/master/LICENSE)

**NOTE:** Version 2.0 currently has a bug and writes non-standard files (that won't open in any other application) when the source image has an alpha channel. This will be fixed in a follow-up release.

Features
--------
Almost all pixel formats supported by JPEG XR can be loaded. Incompatible formats, however, will first be converted to a representation that GIMP understands (this means you'll loose HDR data, for example). All RGB pixel formats are converted to 24bpp RGB, all RGBA formats to 32bpp BGRA, and all grayscale formats to 8bpp Gray. Black-white images are imported as indexed images.

Images are saved in one of the following pixel formats:
* 1bpp BlackWhite, if image mode is set to Indexed and the color map has exactly two entries black and white
* 8bpp Grayscale, for grayscale images
* 24bpp RGB, for color images without alpha channel
* 32bpp RGBA, for color images with alpha channel

Save options include:
* Image quality 
* Alpha channel quality 
* Overlap¹
* Chroma subsampling¹
* Tiling¹

¹ see [jxrlib](http://jxrlib.codeplex.com) documentation for more information

Installation
------------
The plugin is designed to run with GIMP version 2.8.x.

### Windows
Take the [pre-compiled binary](https://github.com/chausner/gimp-jxr/releases/latest) and put it into "%USERPROFILE%\\.gimp-2.8\plug-ins" (create the folder if it doesn't exist). Alternatively, run ```<GIMP installation dir>\bin\gimptool-2.0.exe --install-bin <path to plugin binary>```.

### Ubuntu
1. Make sure GIMP 2.8.x is installed and you have all required development files:
   ```
   sudo apt-get install libgimp2.0-dev libjxr0 libjxr-dev
   ```
   
2. Grab the gimp-jxr source code via git:
   ```
   git clone https://github.com/chausner/gimp-jxr.git
   ```
   
3. Compile and install gimp-jxr:
   ```
   cd gimp-jxr
   make
   make install
   ```