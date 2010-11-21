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
	gint32			image_ID;
	GimpDrawable*	drawable;
	GimpPixelRgn	pixel_rgn;

	gchar*			filename;
	guint			width,height;

	vlUInt			vtf_bindcode;
	vlByte*			rgbaBuf;

	// ---------------
	if (thumb)
		filename = param[0].data.d_string;
	else
		filename = param[1].data.d_string;
	
	if ( vlInitialize() )
	{
		vlCreateImage(&vtf_bindcode);
		vlBindImage(vtf_bindcode);
		
		if (vlImageLoad(filename,vlFalse) )
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
					gimp_image_add_layer(image_ID,layer_ID,0);
					gimp_drawable_detach(drawable);
					
					// Alpha doesn't always represent transparency in game engine textures, so eliminate it from thumbs.
					// This causes problems when you've solid-colour RGB that only makes sense when viewed with alpha,
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

				if ( vlImageGetFrameCount() > 1)
					layer_label = "Frame";
				else if ( vlImageGetFaceCount() > 1 )
					layer_label = "Face";
				else if ( vlImageGetDepth() > 1 )
					layer_label = "Slice";
				else
					single = TRUE;

				num_layers = vlImageGetFrameCount() + vlImageGetFaceCount() + vlImageGetDepth() -2; // only one will be valid

				if (single)
					gimp_progress_init("Loading VTF...");
				else
					gimp_progress_init_printf("Loading %i VTF %ss...",num_layers,layer_label);

				// only one of these loops will actually run more than once
				for (frame=0;frame<vlImageGetFrameCount();frame++)
					for (face=0;face<vlImageGetFaceCount();face++)
						for (slice=0;slice<vlImageGetDepth();slice++)
							if ( vlImageConvertToRGBA8888( vlImageGetData(frame,face,slice,0),rgbaBuf,width,height,vlImageGetFormat() ) )
							{
								if ( single )
									strcpy_s(layer_name_buf,sizeof(layer_name_buf),"Background");
								else
									snprintf(layer_name_buf,sizeof(layer_name_buf),"%s #%i",layer_label,frame+face+slice + 1);

								layer_ID = gimp_layer_new(image_ID,layer_name_buf,width,height,GIMP_RGB_IMAGE,100,GIMP_NORMAL_MODE);
								gimp_layer_add_alpha(layer_ID);
								drawable = gimp_drawable_get(layer_ID);

								gimp_pixel_rgn_init(&pixel_rgn, drawable, 0, 0, width, height, TRUE, FALSE);
								gimp_pixel_rgn_set_rect(&pixel_rgn, rgbaBuf, 0,0, width,height);

								if ( vtflib_format_has_alpha(vlImageGetFormat()) )
								{
									// Alpha doesn't always represent transparency, so separate it in the UI. When it is transparency,
									// separation is still good because it reveals the colour of invisible pixels (which can bleed
									// onto visible ones when the texture is being resized in realtime on the GPU, creating ugly outlines).
									gimp_layer_add_mask(layer_ID, gimp_layer_create_mask(layer_ID,GIMP_ADD_ALPHA_TRANSFER_MASK) );
									gimp_layer_set_edit_mask(layer_ID,FALSE);
								}
								else
									gimp_layer_flatten(layer_ID);

								gimp_drawable_update(layer_ID, 0, 0, width, height);
								gimp_image_add_layer(image_ID,layer_ID,0);
								gimp_drawable_detach(drawable);
											
								vtf_ret_values[0].data.d_status = GIMP_PDB_SUCCESS;
								*nreturn_vals = 2;

								gimp_progress_update( (gdouble) (frame+face+slice) / (gdouble) num_layers );
							}
				gimp_progress_end();
			}
			g_free(rgbaBuf);
		}
		vlDeleteImage(vtf_bindcode);
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