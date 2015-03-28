**Allows the**<a href='http://www.gimp.org/'>GNU Image Manipulation Program</a> to read and write <a href='http://developer.valvesoftware.com/wiki/Valve_Texture_Format'>Valve Texture Format</a> files on Windows.

A relatively simple save options screen is used. To access VTF's more esoteric features make your textures with <a href='http://nemesis.thewavelength.net/index.php?p=41'>VTFEdit</a>.

There are two GIMP-specific features. You can:

  * Specify a layer to use as the VTF's alpha channel. This bypasses most problems with GIMP's readiness to destroy the RGB data of fully-transparent pixels.
  * Export each layer group to a different VTF file. Store related textures in one XCF!

![http://developer.valvesoftware.com/w/images/8/80/Gimp_vtf_opt.png](http://developer.valvesoftware.com/w/images/8/80/Gimp_vtf_opt.png)

This program uses <a href='http://nemesis.thewavelength.net/index.php?c=149'>VTFLib</a>, by Nem and Wunderboy.