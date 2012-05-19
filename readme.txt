GIMP VTF PLUG-IN 1.2.1
Tom Edwards, 19th May 2012
*******************************

To install, extract both the EXE and DLL to your
GIMP plug-ins folder. This is typically at 
C:\Users\<USER>\.gimp-2.8\plug-ins\.

If you prefer, there is also a plug-ins folder in 
your main GIMP installation. This is at 
<gimp install dir>\lib\gimp\2.0\plug-ins\.

To get the latest version of the plugin and download 
source code visit <http://code.google.com/p/gimp-vtf/>.

Changes
*******

1.2.1
 * Fixed errors on Windows XP

1.2
 * Upgraded to GIMP 2.8
 * Added 64-bit support
 * Now saves top-level layer groups to separate VTF
   files. Store albedos, bumps, masks, and anything
   else in the same XCF!
 * Export settings are now remembered after GIMP is
   closed
 * Now writes I8/IA8 if the image is grayscale and the
   user wants an uncompressed format
 * Fixed non-localised layer names when loading
   animated VTFs
 * Removed HDR pixel formats. Neither GIMP nor VTFLib
   support this yet.

1.1
 * Upgraded to VTFLib 1.3.2 for VTF 7.5 support
 * Each open image now has its own save settings
 * Save settings are now filled in when loading a VTF
 * Flags not covered by the UI are now preserved when
   saving an opened file
 * Added a simplified format selection (do you want
   compression, do you want alpha)
 * Added options to save with LOD disabled, Clamping
   and/or the Bump/SSBump flags
 * Added a VTF version option when saving
 * Added LOD Control setting (human-configurable!)
 * Added explanatory tooltips
 * Added localisation support (no actual translations,
   but creating them is now possible)
 * Fixed errors when saving an image after its designated
   alpha layer has been removed  
 * Fixed error when alpha layer doesn't fit the image
 * Graceful out-of-memory handling

1.0.4
 * Fixed embarrassing error when saving a single-frame
   image. It took 300 downloads to get one bug report...

1.0.3
 * Fixed errors when saving grayscale or indexed images
 * Layers are now resized and offset as appropriate when
   saved as frames/faces/slices
  
1.0.2
 * Fixed GIMP error report on toggling "Use layer as alpha
   channel" when there are no suitable layers
 * Further tweaks to format list: wierder options removed,
   remainder better sorted
 * Corrected reported alpha bits for BGRA4444
 * Renamed "Compression" header to "Pixel format", as only
   the DXTs are really compressed

1.0.1
 * Fixed save failure when using the only layer in the
   image as the alpha channel (can no longer do this)
 * Fixed error messages not making it from the file loader
   to GIMP
 * Sorted format list so that all the DXTs are at the top
 * Corrected alpha bit depth for RGBA16161616(F)
 * Code elegance, and fixed VS post-build event for users
   not called Tom