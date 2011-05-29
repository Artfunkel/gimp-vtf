           GIMP VTF PLUG-IN 1.0.3
         Tom Edwards, 29th May 2011
       *******************************

To install, extract both the EXE and DLL to your
GIMP plug-ins folder. This is typically at 
C:\Users\<USER>\.gimp-x.x\plug-ins\.

If you prefer, there is also a plug-ins folder in 
your main GIMP installation. This is at 
<install dir>\lib\gimp\2.0\plug-ins\.

To get the latest version of the plugin and download 
source code, visit <http://code.google.com/p/gimp-vtf/>.

Changes
*******

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