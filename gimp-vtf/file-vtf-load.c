/*
 * GIMP VTF
 * Copyright (C) 2010 Tom Edwards

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later
 * version.
 */

#include "file-vtf.h"

void load(gint nparams, const GimpParam* param, gint* nreturn_vals, gboolean thumb)
{
	gint32			layer_ID;
	GimpDrawable*	drawable;
	GimpPixelRgn	pixel_rgn;

	gchar*			filename;
	guint			width,height;

	vlByte*			rgbaBuf;

	// ---------------
	if (thumb)
		filename = param[0].data.d_string;
	else
		filename = param[1].data.d_string;
	
	if ( vlImageLoad(filename,vlFalse) )
	{
		width = vlImageGetWidth();
		height = vlImageGetHeight();
			
		if (thumb)
		{
			gint32	thumb_size;
			guint	thumb_mip = 0;

			gint32 mip_width = width;
			gint32 mip_height = height;

			vlByte*	mip_data;
				
			thumb_size = param[1].data.d_int32;
				
			while  (mip_width > thumb_size
					&& thumb_mip < vlImageGetMipmapCount() - 1 )
			{
				thumb_mip++;
				mip_width /= 2;
				mip_height /= 2;
			}

			rgbaBuf = g_new(vlByte, mip_height*mip_width*4 );
			if (!rgbaBuf)
			{
				record_error_mem();
				return;
			}
				
			mip_data = vlImageGetData( (vlUInt)floor(vlImageGetFrameCount()/2.0f), (vlUInt)floor(vlImageGetFaceCount()/2.0f), (vlUInt)floor(vlImageGetDepth()/2.0f), thumb_mip );
				
			if ( vlImageConvertToRGBA8888( mip_data,rgbaBuf,mip_width,mip_height,vlImageGetFormat() ) )
			{
				image_ID = gimp_image_new(mip_width,mip_height,GIMP_RGB);
					
				layer_ID = gimp_layer_new(image_ID,"VTF Thumb",mip_width,mip_height,GIMP_RGB_IMAGE,100,GIMP_NORMAL_MODE);
				gimp_layer_add_alpha(layer_ID);
				drawable = gimp_drawable_get(layer_ID);

				gimp_pixel_rgn_init(&pixel_rgn, drawable, 0, 0, mip_width, mip_height, TRUE, FALSE);
				gimp_pixel_rgn_set_rect(&pixel_rgn, rgbaBuf, 0,0, mip_width,mip_height);
					
				gimp_drawable_update(layer_ID, 0, 0, mip_width, mip_height);
				gimp_image_insert_layer(image_ID,layer_ID,0,0);
				gimp_drawable_detach(drawable);
					
				// Alpha doesn't always represent transparency in game engine textures, so eliminate it from thumbs.
				// This causes problems if the image is solid-colour RGB and only makes sense when viewed with alpha,
				// but that's comparatively rare.
				gimp_layer_add_mask(layer_ID, gimp_layer_create_mask(layer_ID,GIMP_ADD_ALPHA_TRANSFER_MASK) );
				gimp_layer_remove_mask( layer_ID, GIMP_MASK_DISCARD );

				*nreturn_vals = 2;
				vtf_ret_values[0].data.d_status = GIMP_PDB_SUCCESS;
					
				// value 1 (the image) is set at the end of the function
					
				vtf_ret_values[2].type = GIMP_PDB_INT32;
				vtf_ret_values[2].data.d_int32 = width;

				vtf_ret_values[3].type = GIMP_PDB_INT32;
				vtf_ret_values[3].data.d_int32 = height;
			}
		}
		else // regular image load
		{
			guint			frame,face,slice,num_layers;
			gboolean		single = FALSE;
			gchar*			layer_label;
			gchar			layer_name_buf[32];

			image_ID = gimp_image_new(width,height,GIMP_RGB);
			gimp_image_set_filename(image_ID,filename);
		
			rgbaBuf = g_new(vlByte, vlImageGetWidth()*vlImageGetHeight()*4 );
			if (!rgbaBuf)
			{
				record_error_mem();
				return;
			}

			if ( vlImageGetFrameCount() > 1)
				layer_label = _("#anim_frame_word");
			else if ( vlImageGetFaceCount() > 1 )
				layer_label = _("#face_word");
			else if ( vlImageGetDepth() > 1 )
				layer_label = _("#slice_word");
			else
				single = TRUE;

			num_layers = vlImageGetFrameCount() + vlImageGetFaceCount() + vlImageGetDepth() -2; // only one will be valid

			if (single)
				gimp_progress_init(_("#load_message_single"));
			else
				gimp_progress_init_printf(_("#load_message_multi"),num_layers,layer_label);

			// only one of these loops will actually run more than once
			for (frame=0;frame<vlImageGetFrameCount();frame++)
				for (face=0;face<vlImageGetFaceCount();face++)
					for (slice=0;slice<vlImageGetDepth();slice++)
						if ( vlImageConvertToRGBA8888( vlImageGetData(frame,face,slice,0),rgbaBuf,width,height,vlImageGetFormat() ) )
						{
							if ( single )
							{
								textdomain(""); // reset to GIMP default to get the localised name
#if _MSC_VER
								strcpy_s(layer_name_buf, _countof(layer_name_buf), _("Background"));
#else
								strncpy(layer_name_buf, _("Background"), sizeof(layer_name_buf));
								layer_name_buf[31] = 0;
#endif
								textdomain(TEXT_DOMAIN);
							}
							else
								snprintf(layer_name_buf,sizeof(layer_name_buf),"%s #%i",layer_label,frame+face+slice + 1);

							layer_ID = gimp_layer_new(image_ID,layer_name_buf,width,height,GIMP_RGB_IMAGE,100,GIMP_NORMAL_MODE);
							gimp_layer_add_alpha(layer_ID);
							drawable = gimp_drawable_get(layer_ID);

							gimp_pixel_rgn_init(&pixel_rgn, drawable, 0, 0, width, height, TRUE, FALSE);
							gimp_pixel_rgn_set_rect(&pixel_rgn, rgbaBuf, 0,0, width,height);

							if ( vtflib_format_has_alpha(vlImageGetFormat()) )
							{
								// Alpha doesn't always represent transparency, so separate it out. When it *is* transparency,
								// separation is still good because it reveals the colour of invisible pixels (which can bleed
								// onto visible ones when the texture is being resized in realtime on the GPU, creating ugly outlines).
								gimp_layer_add_mask(layer_ID, gimp_layer_create_mask(layer_ID,GIMP_ADD_ALPHA_TRANSFER_MASK) );
								gimp_layer_set_edit_mask(layer_ID,FALSE);
							}
							else
								gimp_layer_flatten(layer_ID);

							gimp_drawable_update(layer_ID, 0, 0, width, height);
							gimp_image_insert_layer(image_ID,layer_ID,0,0);
							gimp_drawable_detach(drawable);
											
							vtf_ret_values[0].data.d_status = GIMP_PDB_SUCCESS;
							*nreturn_vals = 2;

							gimp_progress_update( (gdouble) (frame+face+slice) / (gdouble) num_layers );
						}
			{
				// Generate image settings
				VtfSaveOptions_t	gimpVtfOpt;
				VTFImageFormat	format;
				guint i;

				gimpVtfOpt = DefaultSaveOptions;
			
				// Version
				gimpVtfOpt.Version = vlImageGetMinorVersion();

				// Format
				format = vlImageGetFormat();

				switch(format)
				{			
				case IMAGE_FORMAT_RGBA8888:
				case IMAGE_FORMAT_BGRA8888:
				case IMAGE_FORMAT_ABGR8888:
				case IMAGE_FORMAT_ARGB8888:
					gimpVtfOpt.WithAlpha = TRUE;
				case IMAGE_FORMAT_RGB888:
				case IMAGE_FORMAT_BGR888:
					gimpVtfOpt.Compress = FALSE;
					break;
				case IMAGE_FORMAT_DXT5:
					gimpVtfOpt.WithAlpha = TRUE;
				case IMAGE_FORMAT_DXT1:
					gimpVtfOpt.Compress = TRUE;
					break;
				case IMAGE_FORMAT_IA88:
					gimpVtfOpt.WithAlpha = TRUE;
				case IMAGE_FORMAT_I8:
					gimpVtfOpt.Compress = FALSE;
					gimp_image_convert_grayscale(image_ID);
					break;
				default:
					gimpVtfOpt.AdvancedSetup = TRUE;
					for (i=0; i < num_vtf_formats; i++)
					{
						if (vtf_formats[i].vlFormat == format)
						{
							gimpVtfOpt.PixelFormat = i;
							gimpVtfOpt.WithAlpha = vtf_format_has_alpha(i);
							gimpVtfOpt.Compress = vtf_format_is_compressed(i);
							break;
						}
					}
				}

				if (gimpVtfOpt.Compress)
				{
					// Passing to the console is crap because it is not visible by default, but until there is a
					// way to specify that you want a GUI message to appear without requiring user interaction
					// is the only sensible choice.
					gimp_message_set_handler(GIMP_CONSOLE);
					gimp_message(_("#opening_dxt_warning"));
					gimp_message_set_handler(GIMP_MESSAGE_BOX);
				}

				// Flags
				gimpVtfOpt.GeneralFlags = vlImageGetFlags();

				gimpVtfOpt.Clamp = gimpVtfOpt.GeneralFlags & TEXTUREFLAGS_CLAMPS && gimpVtfOpt.GeneralFlags & TEXTUREFLAGS_CLAMPT;
				gimpVtfOpt.NoLOD = gimpVtfOpt.GeneralFlags & TEXTUREFLAGS_NOLOD;
				gimpVtfOpt.WithMips = !(gimpVtfOpt.GeneralFlags & TEXTUREFLAGS_NOMIP);

				if ( vlImageGetFrameCount() > 1 )
					gimpVtfOpt.LayerUse = VTF_ANIMATION;
				else if ( vlImageGetFaceCount() == 6 )
					gimpVtfOpt.LayerUse = VTF_ENVMAP;
				else if ( vlImageGetDepth() > 1 )
					gimpVtfOpt.LayerUse = VTF_VOLUME;
				else
					gimpVtfOpt.LayerUse = VTF_MERGE_VISIBLE;

				if (gimpVtfOpt.GeneralFlags & TEXTUREFLAGS_NORMAL)
					gimpVtfOpt.BumpType = BUMP;
				else if (gimpVtfOpt.GeneralFlags & TEXTUREFLAGS_SSBUMP)
					gimpVtfOpt.BumpType = SSBUMP;

				if ( vlImageGetSupportsResources() )
				{
					if ( vlImageGetHasResource(VTF_RSRC_TEXTURE_LOD_SETTINGS) )
					{
						vlUInt size;
						vlVoid* data = vlImageGetResourceData(VTF_RSRC_TEXTURE_LOD_SETTINGS,&size);
						if (size > 2)
						{
							gimpVtfOpt.LodControlU = *(gchar*)data;
							gimpVtfOpt.LodControlV = *((gchar*)data + 1);
						}
					}
				}

				// Store
				gimp_set_data (vtf_get_data_id(FALSE), &gimpVtfOpt, sizeof (VtfSaveOptions_t));
			}
		}

		g_free(rgbaBuf);		
	}
	
	if (vtf_ret_values[0].data.d_status == GIMP_PDB_EXECUTION_ERROR)
	{
		vtf_ret_values[1].type          = GIMP_PDB_STRING;
		vtf_ret_values[1].data.d_string = (gchar*)vlGetLastError();
	}
	else
	{
		vtf_ret_values[1].type			= GIMP_PDB_IMAGE;
		vtf_ret_values[1].data.d_image	= image_ID;
	}
}