# gimp-jxr
GIMP plugin for reading and writing of JPEG XR image files. 

[![license](https://img.shields.io/github/license/chausner/gimp-jxr.svg)](https://github.com/chausner/gimp-jxr/blob/master/LICENSE)

Features
--------
Almost all pixel formats supported by JPEG XR can be loaded. Incompatible formats, however, will first be converted to a representation that GIMP understands (this means you'll loose HDR data, for example). All RGB pixel formats are converted to 24bpp RGB, all RGBA formats to 32bpp BGRA, and all grayscale formats to 8bpp Gray. Black-white images are imported as indexed images.

Images are saved in one of the following pixel formats:
* 1bpp BlackWhite, if image mode is set to Indexed and the color map has exactly two entries black and white
* 8bpp Grayscale, for grayscale images
* 24bpp RGB, for color images without alpha channel
* 32bpp BGRA, for color images with alpha channel

Save options include:
* Image quality 
* Alpha channel quality 
* Overlap¹
* Chroma subsampling¹
* Tiling¹

¹ see [jxrlib](http://jxrlib.codeplex.com) documentation for more information

The plugin supports reading and writing of images with embedded color profiles and XMP metadata.

Installation
------------
The plugin is designed to run with GIMP version 2.8.x.

### Windows
Take the [pre-compiled binary](https://github.com/chausner/gimp-jxr/releases/latest) and put it into "%USERPROFILE%\\.gimp-2.8\plug-ins" (create the folder if it doesn't exist). Make sure you are using the x64 version of the plugin if your GIMP installation is 64-bit, otherwise use the x86 version. If you get the error code 0xc000007b on GIMP startup, you are using the wrong version of the plugin.

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
