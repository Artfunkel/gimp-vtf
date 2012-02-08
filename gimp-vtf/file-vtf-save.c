/*
 * GIMP VTF
 * Copyright (C) 2010 Tom Edwards

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later
 * version.
 */

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "file-vtf.h"

#define LAYERGROUPS_ITERATE layergroups.cur = layergroups.head; layergroups.cur < layergroups.head + layergroups.count; layergroups.cur++

/*
 * Logic
 */

guint*		layer_IDs_root;
guint		num_layers_root =  0;	// for layer group search
guint		exponentU;
guint		exponentV;

gint32		dummy_lg = -1;

typedef struct TabControls
{
	GtkWidget*	Label;
	GtkWidget*	Icon;
	GtkWidget*	ExportCheckbox;

	GtkWidget* Page;

	GtkWidget*	SimpleFormatContainer;
	GtkWidget*	WithAlpha;
	GtkWidget*	Compress;
	GtkWidget*	AdvancedToggle;

	GtkWidget*	FormatsView;
	GtkWidget*	FormatsView_Frame;

	GtkWidget*	DoMips;
	GtkWidget*	NoLOD;
	GtkWidget*	Clamp;
	GtkWidget*	BumpType;
	GSList*		BumpRadioGroup;

	GtkWidget*	LODControlHBox;
	GtkWidget*	LODControlSlider;

	GtkWidget*	ClearOtherFlags;

	GtkWidget*	VtfVersion;

	GtkWidget*	LayerUseCombo;
	GtkWidget*	LayerUseLabel;
	GtkWidget*	LayerUseHBox;

	GtkWidget* AlphaLayerCombo;
	GtkWidget* AlphaLayerEnable;
	GtkWidget* AlphaLayerLabel;
} TabControls_t;

typedef struct LayerGroup
{
	VtfSaveOptions_t VtfOpt;

	gint32	ID, tattoo;

	gint width, height, num_bytes;

	const gchar*	name;
	const gchar*	path;
	const gchar*	filename; // points inside 'path'

	guint			children_count;
	const gint32*	children;

	gboolean	is_main; // output file does not have a suffix

	TabControls_t	UI;
} LayerGroup_t;

struct SaveSettingsManager
{
	guint32	count;

	LayerGroup_t* head;
	LayerGroup_t* cur;
} layergroups;

gint select_vtf_format_index(const VtfSaveOptions_t* opt)
{
	if (opt->AdvancedSetup)
		return layergroups.cur->VtfOpt.PixelFormat;

	if (opt->Compress)
	{
		if (opt->WithAlpha)
			return 3;
		else
			return 0;
	}
	else
	{
		gboolean gray = gimp_image_base_type(image_ID) == GIMP_GRAY;

		if (opt->WithAlpha)
		{
			if (gray) return 10;
			else return 5;
		}
		else
		{
			if (gray) return 8;
			else return 4;
		}
	}
}

gboolean fix_alpha_layer(VtfSaveOptions_t* opt, gint32 image_id)
{
	if ( opt->AlphaLayerTattoo && gimp_image_get_layer_by_tattoo(image_id,opt->AlphaLayerTattoo) == -1 )
	{
		opt->AlphaLayerTattoo = 0;
		return TRUE;
	}
	return FALSE;
}

gboolean is_drawable_full_size(gint32 drawable_ID)
{
	return gimp_drawable_width(drawable_ID) == layergroups.cur->width && gimp_drawable_height(drawable_ID) == layergroups.cur->height;
}

void vtf_size_error(gchar* dimension,gint value)
{
	static gchar buf[256];
	gint next = NextPowerOfTwo(value);
	snprintf(buf,256,_("#image_size_error"),dimension,value,dimension,next >> 1, next);
	record_error(buf,GIMP_PDB_EXECUTION_ERROR);
	return;
}

void remove_dummy_lg()
{
	if (dummy_lg != -1)
	{
		guint i;
		for (i = 0; i < layergroups.cur->children_count; i++)
		{
			gimp_image_reorder_item(image_ID,layergroups.cur->children[i],0,0);
		}
		gimp_image_remove_layer(image_ID,layergroups.cur->ID);
	}
}

gboolean show_options(const gint32 image_ID);
void create_vtf(gint32 layer_group, gboolean is_main_group);

void save(gint nparams, const GimpParam* param, gint* nreturn_vals)
{
	gboolean* root_layer_visibility;
	guint	i,num_to_export=0, num_exported=0;
	
	run_mode	= (GimpRunMode)param[0].data.d_int32;
	image_ID	= param[1].data.d_int32;
	filename	= param[3].data.d_string;

	layer_IDs_root = (guint*)gimp_image_get_layers(image_ID,(gint*)&num_layers_root);

	for (i=0; i < num_layers_root; i++)
	{
		if ( gimp_item_is_group(layer_IDs_root[i]) )
		{
			gint*	children;
			gint	num_children;

			children = gimp_item_get_children(layer_IDs_root[i], &num_children);
			g_free(children);

			if (num_children)
				layergroups.count++;
		}
	}

	if (layergroups.count == 0)
	{
		// Detect images with layer groups but no layers...GIMP doesn't protect us from that
		gint num_layers = 0;
		for (i=0; i < num_layers_root; i++)
		{
			if (!gimp_item_is_group(layer_IDs_root[i]))
				num_layers++;
		}
		if (num_layers == 0)
		{
			record_error(_("#no_data"),GIMP_PDB_EXECUTION_ERROR);
			return;
		}

		dummy_lg = gimp_layer_group_new(image_ID);
		gimp_item_set_name(dummy_lg,_("#dummy_lg_name"));
		gimp_image_insert_layer(image_ID,dummy_lg,0,0);

		for (i=0; i < num_layers_root; i++)
			if (!gimp_item_is_group(layer_IDs_root[i]))
				gimp_image_reorder_item(image_ID, layer_IDs_root[i], dummy_lg, -1);

		layergroups.count = 1;
		g_free(layer_IDs_root);
		layer_IDs_root = (guint*)gimp_image_get_layers(image_ID,(gint*)&num_layers_root);
	}

	layergroups.cur = layergroups.head = g_new(LayerGroup_t,layergroups.count);
	
	gimp_ui_init(PLUG_IN_BINARY, FALSE);

	// generate/load settings (can't use the iterate macro here!)
	filename[strlen(filename) - 4] = 0; // hide file ext (undone after loop)
	for (i=0; i < num_layers_root; i++)
	{
		gint*	children;
		gint	num_children;

		gchar *path_mixed=0; // mixed case
		guint len=0;

		children = gimp_item_get_children(layer_IDs_root[i], &num_children);
		
		if (num_children == 0)		
			continue;

		memset(layergroups.cur,0,sizeof(LayerGroup_t));
		memcpy(&layergroups.cur->VtfOpt,&DefaultSaveOptions,sizeof(VtfSaveOptions_t));

		if (layergroups.cur == layergroups.head)
			layergroups.cur->is_main = TRUE;

		layergroups.cur->tattoo = gimp_item_get_tattoo(layer_IDs_root[i]);
		layergroups.cur->ID = gimp_image_get_layer_by_tattoo(image_ID,layergroups.cur->tattoo);

		layergroups.cur->children = children;
		layergroups.cur->children_count = num_children;

		// Using image dimensions here, as the size of a layergroup can't be set directly
		// Users who want multiple resolutions will have to use multiple XCFs
		layergroups.cur->width = gimp_image_width(image_ID); //gimp_drawable_width(layergroups.cur->ID);
		layergroups.cur->height = gimp_image_height(image_ID); //gimp_drawable_height(layergroups.cur->ID);

		layergroups.cur->num_bytes = layergroups.cur->width * layergroups.cur->height * 4;

		if (layergroups.cur->num_bytes == 0 )
		{
			record_error(_("#no_data"),GIMP_PDB_EXECUTION_ERROR);
			return;
		}
	
		if ( !IsPowerOfTwo(layergroups.cur->width) )
		{
			vtf_size_error(_("#width_word"),layergroups.cur->width);
			return;
		}
		
		if ( !IsPowerOfTwo(layergroups.cur->height) )
		{
			vtf_size_error(_("#height_word"),layergroups.cur->height);
			return;
		}

		// Calculate LOD exponents
		for (exponentU=0; pow(2.0f,(int)exponentU) != layergroups.cur->width; exponentU++) {}
		for (exponentV=0; pow(2.0f,(int)exponentV) != layergroups.cur->height; exponentV++) {}

		// Names
		layergroups.cur->name = gimp_item_get_name(layergroups.cur->ID);
		
		if (layergroups.cur->is_main)
		{
			len = (guint)strlen(filename) + 4 + 1;
			path_mixed = g_new(gchar,len);
			snprintf(path_mixed,len,"%s.vtf",filename);
		}
		else
		{
			len = (guint)(strlen(filename) + 1 + strlen(layergroups.cur->name) + 4 + 1);
			path_mixed = g_new(gchar,len);
			snprintf(path_mixed,len,"%s_%s.vtf",filename,layergroups.cur->name);
		}

		g_assert(path_mixed);

		layergroups.cur->path = g_ascii_strdown(path_mixed,len);
		layergroups.cur->filename = strrchr(layergroups.cur->path,'\\') + 1;
		
		g_free(path_mixed);
			
		// Load last settings
		gimp_get_data( (gchar*)get_vtf_options_ID(dummy_lg == -1 ? layergroups.cur->tattoo : 0), &layergroups.cur->VtfOpt );

		if ( fix_alpha_layer(&layergroups.cur->VtfOpt,image_ID) )
		{
			GtkWidget* MessageBox;
			MessageBox = gtk_message_dialog_new(NULL,(GtkDialogFlags)0,GTK_MESSAGE_WARNING,GTK_BUTTONS_OK,NULL);
			gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(MessageBox),_("#missing_alpha_warning"));
			gtk_window_set_position(GTK_WINDOW(MessageBox),GTK_WIN_POS_CENTER);

			gtk_widget_show(MessageBox);
			gtk_dialog_run( GTK_DIALOG(MessageBox) );
			gtk_widget_destroy (MessageBox);

			run_mode = GIMP_RUN_INTERACTIVE;
		}
		layergroups.cur++;
	}
	filename[strlen(filename)] = '.';

	if (layergroups.count == 0)
	{
		record_error(_("#no_data"),GIMP_PDB_EXECUTION_ERROR);
		return;
	}
	
	switch(run_mode)
	{
	case GIMP_RUN_INTERACTIVE:	
		if ( !show_options(image_ID) )
		{
			vtf_ret_values[0].data.d_status = GIMP_PDB_CANCEL;
			return;
		}

		// Validate all layers in case the user deleted some while the options window was open. Silly user!
		for (i=0; i < layergroups.count; i++)
		{
			if ( gimp_image_get_layer_by_tattoo(image_ID,layergroups.head[i].tattoo) == -1 )
			{
				record_error(_("#layers_changed_error"),GIMP_PDB_EXECUTION_ERROR);
				return;
			}
			fix_alpha_layer(&layergroups.head[i].VtfOpt,image_ID);
		}
		break;
	case GIMP_RUN_NONINTERACTIVE:
		switch(nparams)
		{
		case 16:
			for (LAYERGROUPS_ITERATE)
				layergroups.cur->VtfOpt.Enabled = layergroups.cur->ID == param[16].data.d_layer; // FIXME: add a special error message for the param'ed ID being wrong

			layergroups.cur->VtfOpt.PixelFormat = param[5].data.d_int8;
			layergroups.cur->VtfOpt.AlphaLayerTattoo = param[6].data.d_int32;
			if (fix_alpha_layer(&layergroups.cur->VtfOpt,image_ID))
			{
				record_error(_("#invalid_alpha_error"),GIMP_PDB_EXECUTION_ERROR);
				return;
			}
			layergroups.cur->VtfOpt.LayerUse = (VtfLayerUse_t)param[7].data.d_int8;
			layergroups.cur->VtfOpt.Version = param[8].data.d_int8;
			layergroups.cur->VtfOpt.WithMips = param[9].data.d_int8;
			layergroups.cur->VtfOpt.NoLOD = param[10].data.d_int8;
			layergroups.cur->VtfOpt.Clamp = param[11].data.d_int8;
			layergroups.cur->VtfOpt.BumpType = (VtfBumpType_t)param[12].data.d_int8;
			layergroups.cur->VtfOpt.LodControlU = param[13].data.d_int8;
			layergroups.cur->VtfOpt.LodControlV = param[14].data.d_int8;
			break;
		default:
			record_error("Incorrect number of arguments",GIMP_PDB_CALLING_ERROR);
			return;
		}
		break;		
	case GIMP_RUN_WITH_LAST_VALS:
		break;
	default:
		record_error("Invalid run mode",GIMP_PDB_CALLING_ERROR);
		return;
	}
	
	for (LAYERGROUPS_ITERATE)
	{
		if (run_mode == GIMP_RUN_INTERACTIVE)
				gimp_set_data( get_vtf_options_ID(dummy_lg == -1 ? layergroups.cur->tattoo : 0), &layergroups.cur->VtfOpt, sizeof (VtfSaveOptions_t));

		if (layergroups.cur->VtfOpt.Enabled)
			num_to_export++;
	}

	if (num_to_export == 0)
	{
		record_error(_("#no_data"),GIMP_PDB_CALLING_ERROR);
		return;
	}
	
	gimp_image_undo_freeze(image_ID);

	root_layer_visibility = g_new(gboolean,num_layers_root);
	for (i=0; i < num_layers_root; i++)
		root_layer_visibility[i] = gimp_item_get_visible(layer_IDs_root[i]);

	for (i=0; i < layergroups.count; i++)
	{
		guint j;
		
		layergroups.cur = layergroups.head + i;

		if (!layergroups.cur->VtfOpt.Enabled)
			continue;

		if ( num_to_export == 1)
			gimp_progress_pulse(); // crap, but nothing better to do
		else
			gimp_progress_update( (gdouble)(1.0f/num_to_export) * (num_exported+1) );
		
		for (j=0; j < num_layers_root; j++)
		{
			if (layergroups.head[i].ID == layer_IDs_root[j])
				gimp_item_set_visible(layer_IDs_root[j],TRUE);
			else
				gimp_item_set_visible(layer_IDs_root[j],FALSE);
		}
		
		vtf_ret_values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

		create_vtf(layergroups.cur->ID, layergroups.cur->is_main);

		if ( vtf_ret_values[0].data.d_status == GIMP_PDB_SUCCESS)
		{
			num_exported++;			
		}
		else
		{
			gchar* buf;

			size_t len = strlen(vtf_ret_values[1].data.d_string)
						+ strlen(layergroups.cur->name)
						+ strlen(_("#error_layergroup_wrapper"))
						+1;

			buf = g_new(gchar,len);

			snprintf(buf,len,_("#error_layergroup_wrapper"),layergroups.cur->name,vtf_ret_values[1].data.d_string);

			// Can't free the existing string, it could have been allocated by VTFLib/MSVCRT
			//g_free(vtf_ret_values[1].data.d_string);
			vtf_ret_values[1].data.d_string = buf;

			goto create_loop_exit;
		}
	}

create_loop_exit:

	
	for (i=0; i < num_layers_root; i++)
		gimp_item_set_visible(layer_IDs_root[i],root_layer_visibility[i]);
	g_free(root_layer_visibility);
	root_layer_visibility = 0;
}

void create_vtf(gint32 layer_group, gboolean is_main_group)
{
	SVTFCreateOptions	vlVTFOpt;
	vlByte**			rbgaImages;
			
	gint32		alpha_layer_ID_original = -1;
	gint32		alpha_layer_ID = -1;
	gint32		alpha_layer_parent = -1;
	gint		alpha_layer_position;

	guint		frame=1,face=1,slice=1,dummy=0;
	guint*		layer_iterator;

	gchar*		progress_frame_label;

	GimpImageBaseType	img_type;
	gint				colourmap_size;

	gint32		drawable_ID = -1;

	guint		i;
		
	// Set up creation options
	vlImageCreateDefaultCreateStructure(&vlVTFOpt);
	vlVTFOpt.ImageFormat = vtf_formats[select_vtf_format_index(&layergroups.cur->VtfOpt)].vlFormat;

	vlVTFOpt.uiFlags = layergroups.cur->VtfOpt.GeneralFlags; // Import unhandled flags from a loaded VTF

	vlVTFOpt.uiVersion[1] = layergroups.cur->VtfOpt.Version;
	vlVTFOpt.bMipmaps = layergroups.cur->VtfOpt.WithMips;
	if (layergroups.cur->VtfOpt.Clamp)
		vlVTFOpt.uiFlags |= TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT;
	if (layergroups.cur->VtfOpt.NoLOD)
		vlVTFOpt.uiFlags |= TEXTUREFLAGS_NOLOD;
	switch (layergroups.cur->VtfOpt.BumpType)
	{
	case BUMP:
		vlVTFOpt.uiFlags |= TEXTUREFLAGS_NORMAL;
		vlVTFOpt.uiFlags &= ~TEXTUREFLAGS_SSBUMP;
		break;
	case SSBUMP:
		vlVTFOpt.uiFlags |= TEXTUREFLAGS_SSBUMP;
		vlVTFOpt.uiFlags &= ~TEXTUREFLAGS_NORMAL;
		break;
	case NOT_BUMP:
		vlVTFOpt.uiFlags &= ~TEXTUREFLAGS_SSBUMP;
		vlVTFOpt.uiFlags &= ~TEXTUREFLAGS_NORMAL;
		break;
	}

	// Handle alpha layers
	if ( layergroups.cur->VtfOpt.AlphaLayerTattoo && vtf_format_has_alpha(select_vtf_format_index(&layergroups.cur->VtfOpt)) )
	{
		alpha_layer_ID = gimp_image_get_layer_by_tattoo(image_ID,layergroups.cur->VtfOpt.AlphaLayerTattoo);
		alpha_layer_position = gimp_image_get_item_position(image_ID,alpha_layer_ID);
		alpha_layer_parent = gimp_item_get_parent(alpha_layer_ID);
		
		// Move it outside the layer group...not currently necessary as only top-level layers can be chosen
		// from gimp_layer_combo, but the GIMP team might change their minds about that before 2.8.
		gimp_image_reorder_item(image_ID,alpha_layer_ID,-1,0);
		
		// Resize if needed
		if (!is_drawable_full_size(alpha_layer_ID))
		{
			alpha_layer_ID_original = alpha_layer_ID;
			alpha_layer_ID = gimp_layer_new_from_drawable(alpha_layer_ID,image_ID);
			gimp_image_insert_layer(image_ID,alpha_layer_ID,0,0);
			gimp_layer_resize_to_image_size(alpha_layer_ID);
		}

		layergroups.cur->children = gimp_item_get_children(layergroups.cur->ID,(gint*)&layergroups.cur->children_count);
	}

	// convert image to RGB. It would be nice if this could happen per-drawable!
	img_type = gimp_image_base_type(image_ID);
	switch( img_type )
	{
	case GIMP_RGB:
		break;
	case GIMP_INDEXED:
		gimp_image_get_colormap(image_ID,&colourmap_size);
	case GIMP_GRAY:
		gimp_image_convert_rgb(image_ID);
		break;
	}

	switch(layergroups.cur->VtfOpt.LayerUse)
	{
	case VTF_MERGE_VISIBLE:
		layer_iterator = &dummy;
		break;
	case VTF_ANIMATION:
		layer_iterator = &frame;
		progress_frame_label = _("#anim_frame_word");
		break;
	case VTF_ENVMAP:
		layer_iterator = &face;
		progress_frame_label = _("#envmap_face_word");
		break;
	case VTF_VOLUME:
		layer_iterator = &slice;
		progress_frame_label = _("#volume_slice_word");
		break;
	}
	
	// Progress meter...unfortunately, VTFLib won't tell us its internal progress
	if (layergroups.cur->VtfOpt.LayerUse == VTF_MERGE_VISIBLE)
		if (layergroups.cur->children_count == 1)
			gimp_progress_set_text_printf(_("#save_message_single"),layergroups.cur->name);
		else
			gimp_progress_set_text_printf(_("#save_message_combined"),layergroups.cur->name);
	else
		gimp_progress_set_text_printf(_("#save_message_multi"),layergroups.cur->name,layergroups.cur->children_count,progress_frame_label);
	
	g_assert(layergroups.cur->children_count);

	rbgaImages = g_new(vlByte*,layergroups.cur->children_count);
	if (!rbgaImages)
	{
		record_error_mem();
		return;
	}	

	// the layer iterator pointer increments either the frame, face or slice value (or a dummy in the case of VTF_MERGE_VISIBLE)
	for ( *layer_iterator = 0; *layer_iterator < layergroups.cur->children_count; (*layer_iterator)++)
	{
		rbgaImages[*layer_iterator] = g_new(vlByte,layergroups.cur->num_bytes);
		if (!rbgaImages[*layer_iterator])
		{
			record_error_mem();
			return;
		}
	}
	for ( *layer_iterator = 0; *layer_iterator < layergroups.cur->children_count; (*layer_iterator)++)
	{
		GimpPixelRgn	pixel_rgn;
		GimpDrawable*	drawable;
		gboolean		dupe_layer;
		
		drawable_ID = layergroups.cur->children[*layer_iterator];

		dupe_layer = layergroups.cur->VtfOpt.LayerUse == VTF_MERGE_VISIBLE || alpha_layer_ID != -1 || !is_drawable_full_size(drawable_ID);

		if (dupe_layer)
		{
			if ( layergroups.cur->VtfOpt.LayerUse == VTF_MERGE_VISIBLE )
				drawable_ID = gimp_layer_new_from_visible(image_ID,image_ID,_("#combined_layer_title"));
			else
				drawable_ID = gimp_layer_copy(drawable_ID);

			gimp_image_insert_layer(image_ID,drawable_ID,0,0);
			gimp_layer_resize_to_image_size(drawable_ID);
		}
		
		if (alpha_layer_ID != -1)
		{
			// have to create a new mask every time, unfortunately
			gimp_layer_add_mask( drawable_ID, gimp_layer_create_mask(alpha_layer_ID,GIMP_ADD_COPY_MASK) );
			gimp_layer_remove_mask( drawable_ID, GIMP_MASK_APPLY );
		}

		gimp_layer_add_alpha(drawable_ID); // always want to send VTFLib an alpha channel
	
		// Get drawable info
		drawable = gimp_drawable_get(drawable_ID);

		if (drawable->bpp != 4)
		{
			record_error(_("#internal_4bpp_error"),GIMP_PDB_EXECUTION_ERROR);
			return;
		}

		// Put pixels into array, from last layer to first thanks to GIMP
		gimp_pixel_rgn_init(&pixel_rgn, drawable, 0, 0, layergroups.cur->width, layergroups.cur->height, FALSE, FALSE);
		gimp_pixel_rgn_get_rect(&pixel_rgn, rbgaImages[(layergroups.cur->children_count - 1) - *layer_iterator], 0,0, layergroups.cur->width,layergroups.cur->height);

		// Cleanup
		if (dupe_layer)
			gimp_image_remove_layer(image_ID,drawable_ID);
	}

	if (alpha_layer_ID_original != -1)
	{
		gimp_image_remove_layer(image_ID,alpha_layer_ID);
		alpha_layer_ID = alpha_layer_ID_original;
	}
	if (alpha_layer_ID != -1)
	{
		gimp_image_reorder_item(image_ID, alpha_layer_ID, alpha_layer_parent, alpha_layer_position);
		gimp_displays_flush();
	}

	// Hand off to VTFLib
	if ( vlImageCreateMultiple(layergroups.cur->width,layergroups.cur->height, frame,face,slice, rbgaImages, &vlVTFOpt) )
	{

		if ( vlImageGetSupportsResources() )
		{
			if (layergroups.cur->VtfOpt.WithMips && layergroups.cur->VtfOpt.LodControlU != 0 && layergroups.cur->VtfOpt.LodControlV != 0 )
			{
				gchar buf[4]; // must be 4, not 2
				memset(buf,0,4);
				buf[0] = layergroups.cur->VtfOpt.LodControlU;
				buf[1] = layergroups.cur->VtfOpt.LodControlV;
				vlImageSetResourceData(VTF_RSRC_TEXTURE_LOD_SETTINGS,4,buf);
			}
		}

		// Write!
		if ( vlImageSave(layergroups.cur->path) )
			vtf_ret_values[0].data.d_status = GIMP_PDB_SUCCESS;		
	}
	
	// Failed!
	if ( vtf_ret_values[0].data.d_status != GIMP_PDB_SUCCESS )
	{
      vtf_ret_values[1].type          = GIMP_PDB_STRING;
      vtf_ret_values[1].data.d_string = (gchar*)vlGetLastError();
	}
	else if ( vtf_format_is_compressed(layergroups.cur->VtfOpt.PixelFormat) && layergroups.cur->VtfOpt.BumpType != NOT_BUMP)
	{
		// Passing to the console is crap because it is not visible by default, but until there is a
		// way to specify that you want a GUI message to appear without requiring user interaction
		// is the only sensible choice.
		gimp_message_set_handler(GIMP_CONSOLE);
		gimp_message(_("#compressed_bump_warning"));
		gimp_message_set_handler(GIMP_MESSAGE_BOX);
	}
	
	// Free memory
	for(i=0; i < layergroups.cur->children_count; i++)
		g_free(rbgaImages[i]);
	g_free(rbgaImages);

	// restore image colour type	
	switch( img_type )
	{
	case GIMP_RGB:
		break;
	case GIMP_INDEXED:
		gimp_image_convert_indexed(image_ID,GIMP_NO_DITHER,GIMP_MAKE_PALETTE,colourmap_size,FALSE,FALSE,"null");
		break;
	case GIMP_GRAY:
		gimp_image_convert_grayscale(image_ID);
		break;
	}
}

/*
 * UI
 */
GtkWidget*	dialog;
GtkWidget*	column_vbox;
GtkWidget*	cur_hbox;
GtkWidget*	cur_label;
GtkWidget*	cur_align;
GtkWidget*	cur_separator;
GtkTreeViewColumn* cur_column;

GtkListStore*	FormatsStore;

GtkWidget*	LayerGroupNotebook;
GtkWidget*	use_layer_groups_button;

const gchar*	BumpRadioLabels[] = { "#not_bump", "#bump_flag", "#ssbump_flag" };
const gchar*	VtfVersions[] = { "7.2", "7.3", "7.4", "7.5" };

static void update_lod_availability()
{
	gtk_widget_set_sensitive(layergroups.cur->UI.NoLOD,layergroups.cur->VtfOpt.WithMips);
	gtk_widget_set_sensitive(layergroups.cur->UI.LODControlHBox,layergroups.cur->VtfOpt.Version >= 3 && layergroups.cur->VtfOpt.WithMips && !layergroups.cur->VtfOpt.NoLOD);
}

static void set_layer_alpha_active(gboolean active)
{
	gtk_widget_set_sensitive(layergroups.cur->UI.AlphaLayerLabel, active );
	gtk_widget_set_sensitive(layergroups.cur->UI.AlphaLayerEnable, active );
	gtk_widget_set_sensitive(layergroups.cur->UI.AlphaLayerCombo, active && gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(layergroups.cur->UI.AlphaLayerEnable) ) );	
}

static void update_alpha_layer_availability()
{	
	if (layergroups.cur->VtfOpt.AdvancedSetup)
		set_layer_alpha_active(vtf_format_has_alpha(layergroups.cur->VtfOpt.PixelFormat));
	else
		set_layer_alpha_active(layergroups.cur->VtfOpt.WithAlpha);
}

void select_compression(GtkTreeSelection* selection, gpointer user_data)
{
	if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(layergroups.cur->UI.AdvancedToggle)) )
	{
		GtkTreeModel* model;
		GtkTreeIter iter;
				
		if ( gtk_tree_selection_get_selected(selection,&model,&iter) )
		{
			layergroups.cur->VtfOpt.PixelFormat = gtk_tree_path_get_indices( gtk_tree_model_get_path(model,&iter) )[0];
			update_alpha_layer_availability();

			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(layergroups.cur->UI.Compress),vtf_format_is_compressed(layergroups.cur->VtfOpt.PixelFormat));
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(layergroups.cur->UI.WithAlpha),vtf_format_has_alpha(layergroups.cur->VtfOpt.PixelFormat));
		}
	}
}

static void update_layer_use_availability()
{
	gtk_widget_set_sensitive(layergroups.cur->UI.LayerUseHBox, num_layers_root > 2);
}

static void set_alpha_layer()
{
	gint new_alpha_id;

	gimp_int_combo_box_get_active(GIMP_INT_COMBO_BOX(layergroups.cur->UI.AlphaLayerCombo), &new_alpha_id);

	layergroups.cur->VtfOpt.AlphaLayerTattoo = gimp_drawable_get_tattoo(new_alpha_id);
	update_layer_use_availability();
}

static void toggle_alpha_layer(GtkToggleButton *togglebutton, gpointer user_data)
{		
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(togglebutton) ) )
	{
		gint alpha_id = -1;
		gimp_int_combo_box_get_active(GIMP_INT_COMBO_BOX(layergroups.cur->UI.AlphaLayerCombo), &alpha_id);
		if (alpha_id != -1)
			set_alpha_layer();
		gtk_widget_set_sensitive(layergroups.cur->UI.AlphaLayerCombo,TRUE);
	}
	else
	{
		layergroups.cur->VtfOpt.AlphaLayerTattoo = 0;
		gtk_widget_set_sensitive(layergroups.cur->UI.AlphaLayerCombo,FALSE);
	}
	update_layer_use_availability();
}

static void choose_alpha_layer(GtkComboBox* combo, gpointer user_data)
{
	set_alpha_layer();
}

static void choose_simple_alpha(GtkCheckButton* chbx, gpointer user_data)
{
	layergroups.cur->VtfOpt.WithAlpha = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chbx));
	update_alpha_layer_availability();
}

static void choose_vtf_version(GtkComboBox* combo, gpointer user_data)
{
	layergroups.cur->VtfOpt.Version = gtk_combo_box_get_active(GTK_COMBO_BOX(combo)) + 2;
	update_lod_availability();
}

static void choose_bump_type(GtkComboBox* combo, gpointer user_data)
{
	layergroups.cur->VtfOpt.BumpType = (VtfBumpType_t)gtk_combo_box_get_active(combo);
}

static void change_format_select_mode(GtkToggleButton* toggle, gpointer user_data)
{
	LayerGroup_t* _cur;
	gboolean any_advanced=FALSE;
	
	_cur = layergroups.cur;
	layergroups.cur->VtfOpt.AdvancedSetup = gtk_toggle_button_get_active(toggle);

	gtk_widget_set_sensitive(layergroups.cur->UI.SimpleFormatContainer,!layergroups.cur->VtfOpt.AdvancedSetup);

	for (LAYERGROUPS_ITERATE)
	{
		if (layergroups.cur->VtfOpt.AdvancedSetup)
		{
			any_advanced = TRUE;
			break;
		}
	}

	for (LAYERGROUPS_ITERATE)
	{
		gtk_widget_set_sensitive(layergroups.cur->UI.FormatsView,layergroups.cur->VtfOpt.AdvancedSetup);

		if (any_advanced)
		{
			GtkTreeIter iter;
			GtkTreeSelection* select;
			GtkTreePath* path;
		
			path = gtk_tree_path_new();
			gtk_tree_path_append_index(path,select_vtf_format_index(&layergroups.cur->VtfOpt)); // new_from_indices crashes unpredictably
		
			select = gtk_tree_view_get_selection(GTK_TREE_VIEW(layergroups.cur->UI.FormatsView));
		
			gtk_widget_show(layergroups.cur->UI.FormatsView_Frame);
			if (layergroups.cur == _cur)
				gtk_widget_grab_focus(layergroups.cur->UI.FormatsView);

			if (gtk_tree_model_get_iter(GTK_TREE_MODEL(FormatsStore),&iter,path) )
			{
				gtk_tree_selection_select_iter(select,&iter);
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(layergroups.cur->UI.FormatsView),path,NULL,FALSE); 
			}
		
			gtk_tree_path_free(path);
		}
		else
			gtk_widget_hide(layergroups.cur->UI.FormatsView_Frame);
	}
	layergroups.cur = _cur;

	update_alpha_layer_availability();
}

static void change_frame_use(GtkWidget* combo, gpointer user_data)
{
	layergroups.cur->VtfOpt.LayerUse = (VtfLayerUse_t)gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void change_withmips(GtkToggleButton* toggle, gpointer user_data)
{
	layergroups.cur->VtfOpt.WithMips = gtk_toggle_button_get_active(toggle);
	update_lod_availability();
}

static void change_nolod(GtkToggleButton* toggle, gpointer user_data)
{
	layergroups.cur->VtfOpt.NoLOD = gtk_toggle_button_get_active(toggle);
	update_lod_availability();
}

static gboolean alpha_channel_suitable(gint32 image_id, gint32 drawable_id, gpointer user_data)
{
	return drawable_id != layergroups.cur->ID && *(gint32*)user_data == image_id;
}

static gchar* change_lod_control(GtkScale* scale, gdouble value, gpointer user_data)
{	
	layergroups.cur->VtfOpt.LodControlU = exponentU - (gint)value;
	layergroups.cur->VtfOpt.LodControlV = exponentV - (gint)value;
	return g_strdup_printf("%ix%i", (gint)pow(2.0f,(int)layergroups.cur->VtfOpt.LodControlU), (gint)pow(2.0f,(int)layergroups.cur->VtfOpt.LodControlV) ); // casting to hide bogus Intellisense warnings
}

static void change_tab(GtkNotebook *notebook, gpointer page, guint page_num, gpointer user_data)
{
	layergroups.cur = layergroups.head + page_num;
}

static void toggle_export(GtkToggleButton *togglebutton, gpointer user_data )
{
	LayerGroup_t* LG = (LayerGroup_t*)user_data;

	LG->VtfOpt.Enabled = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(LG->UI.Page, LG->VtfOpt.Enabled);
	gtk_widget_set_sensitive(LG->UI.Icon, LG->VtfOpt.Enabled);
}

#ifdef _WIN32
#include "windows.h"
#define HELP_FUNC vtf_help
void vtf_help(const gchar* help_id, gpointer help_data)
{
	ShellExecute(0,"open","http://developer.valvesoftware.com/wiki/Valve_Texture_Format",0,0,0);
}
#else
#define HELP_FUNC gimp_standard_help_func // no good way of opening an arbitrary URL outside Windows
#endif


// GTK is horrid
static gboolean show_options(const gint32 image_ID)
{
	guint		i;
	gboolean	run;

	GtkTreeIter iter;

	enum FormatColumns
	{
		FORMAT_COLUMN_ALPHA = 0,
		FORMAT_COLUMN_NAME,

		FORMAT_COLUMN_COUNT,
	};
	
	struct FormatColumn
	{
		GtkCellRenderer* render;
		gchar*	label;

	} columns[FORMAT_COLUMN_COUNT];

	gchar*	title;
	gint	title_len;

	title_len = (gint)(strlen(_("#window_title")) + strlen(strrchr(filename,'\\')+1));
	title = g_new(gchar,title_len);
	snprintf(title,title_len,_("#window_title"), strrchr(filename,'\\')+1);
	
	dialog = gimp_dialog_new (title, PLUG_IN_BINARY, NULL, (GtkDialogFlags)0,
	
	HELP_FUNC,			"file-vtf",
	GTK_STOCK_CANCEL,	GTK_RESPONSE_CANCEL,
	GTK_STOCK_OK,		GTK_RESPONSE_OK,

	NULL);

	g_free(title);

	gtk_window_set_resizable(GTK_WINDOW(dialog),FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog),GTK_WIN_POS_CENTER);

	// Padding
	cur_align = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_widget_show (cur_align);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), cur_align);
	gtk_alignment_set_padding (GTK_ALIGNMENT(cur_align), 6, 6, 6, 6);

	// Create the pixel formats data model
	FormatsStore = gtk_list_store_new(FORMAT_COLUMN_COUNT,G_TYPE_STRING,G_TYPE_STRING);
	for (i=0;i<num_vtf_formats;i++)
	{
		gtk_list_store_insert_with_values(FormatsStore,&iter,-1,
			FORMAT_COLUMN_ALPHA, vtf_formats[i].alpha_label,
			FORMAT_COLUMN_NAME,  vtf_formats[i].label,
			-1);
	}
	
	columns[FORMAT_COLUMN_ALPHA].render = gtk_cell_renderer_text_new();	
	columns[FORMAT_COLUMN_ALPHA].render->xalign = 0.5;
	columns[FORMAT_COLUMN_ALPHA].label = _("#alpha_channel_title");
	
	columns[FORMAT_COLUMN_NAME].render = gtk_cell_renderer_text_new();
	columns[FORMAT_COLUMN_NAME].render->xpad = 4;
	columns[FORMAT_COLUMN_NAME].label = _("#pixel_format_title");
		
	
	// ------------ TABS --------------

	LayerGroupNotebook = gtk_notebook_new();

	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(LayerGroupNotebook),GTK_POS_LEFT);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(LayerGroupNotebook),dummy_lg == -1);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(LayerGroupNotebook),dummy_lg == -1);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(LayerGroupNotebook),TRUE);
	g_signal_connect(LayerGroupNotebook, "switch-page", G_CALLBACK(change_tab), NULL);

	gtk_container_add (GTK_CONTAINER (cur_align), LayerGroupNotebook);
	gtk_widget_show (LayerGroupNotebook);	

	for (LAYERGROUPS_ITERATE)
	{
		GtkWidget*	label_frame;
		GtkWidget*	label_text;
		TabControls_t* Tab;
		gint		tab_padding;

		tab_padding = dummy_lg == -1 ? 6 : 0;

		Tab = &layergroups.cur->UI;
			
		Tab->Label = gtk_hbox_new(FALSE,0);
		{
			gtk_widget_set_tooltip_text( Tab->Label, layergroups.cur->filename );
		}
			
		// Export checkbox
		// FIXME: can't be selected with keyboard in this location...arguably a GTK bug though
		Tab->ExportCheckbox = gtk_check_button_new();
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Tab->ExportCheckbox),layergroups.cur->VtfOpt.Enabled);
		gtk_box_pack_start (GTK_BOX(Tab->Label), Tab->ExportCheckbox,FALSE,FALSE,0);
		gtk_widget_show (Tab->ExportCheckbox);

		// Thumbnail border
		label_frame = gtk_frame_new(NULL);
		gtk_frame_set_shadow_type(GTK_FRAME(label_frame),GTK_SHADOW_ETCHED_IN);
		gtk_box_pack_start (GTK_BOX(Tab->Label), label_frame,FALSE,FALSE,0);
		gtk_widget_show(label_frame);

		// Thumbnail
		Tab->Icon = gtk_image_new();
		gtk_image_set_from_pixbuf(GTK_IMAGE(Tab->Icon), gimp_drawable_get_thumbnail(layergroups.cur->ID,32,32,GIMP_PIXBUF_SMALL_CHECKS) );
		gtk_container_add (GTK_CONTAINER (label_frame), Tab->Icon);
		gtk_widget_show (Tab->Icon);

		// Text
		label_text = gtk_label_new(layergroups.cur->name);
		gtk_box_pack_start (GTK_BOX(Tab->Label), label_text,TRUE,FALSE,6);
		gtk_widget_show (label_text);

		// ------------ CONFIGURATION TAB --------------

		// Tab padding
		Tab->Page = gtk_alignment_new (0.5, 0, 1, 0);
		gtk_widget_show (Tab->Page);
		gtk_alignment_set_padding (GTK_ALIGNMENT(Tab->Page), tab_padding, tab_padding, tab_padding, tab_padding);

		// Tab vertical box
		column_vbox = gtk_vbox_new (FALSE, 6);		
		gtk_widget_show (column_vbox);
		gtk_container_add (GTK_CONTAINER(Tab->Page), column_vbox);

		// Add the tab to the notebook
		gtk_notebook_append_page(GTK_NOTEBOOK(LayerGroupNotebook),Tab->Page,Tab->Label);
		
		// Format selection	
		cur_align = gtk_alignment_new(0,0,0,0);
		gtk_container_add (GTK_CONTAINER (column_vbox), cur_align);
		gtk_widget_show(cur_align);

		cur_label = gtk_label_new(_("#format_title"));
		gtk_label_set_use_markup( GTK_LABEL(cur_label), TRUE);
		gtk_container_add (GTK_CONTAINER (cur_align), cur_label);
		gtk_widget_show(cur_label);

		// Layer use
		Tab->LayerUseHBox = gtk_hbox_new (FALSE, 6);
		gtk_widget_show(Tab->LayerUseHBox);
		gtk_container_add( GTK_CONTAINER(column_vbox), Tab->LayerUseHBox );

		Tab->LayerUseLabel = gtk_label_new_with_mnemonic(_("#layers_use_label"));
		gtk_widget_show(Tab->LayerUseLabel);
		gtk_widget_set_sensitive(Tab->LayerUseLabel, layergroups.cur->children_count > 1 );
		gtk_box_pack_start (GTK_BOX (Tab->LayerUseHBox), Tab->LayerUseLabel, FALSE, FALSE, 0);

		Tab->LayerUseCombo = gimp_int_combo_box_new (
			_("#layers_merge_visible_label"), VTF_MERGE_VISIBLE,
			_("#layers_animation_label"),    VTF_ANIMATION,
			_("#layers_envmap_label"), VTF_ENVMAP,
			_("#layers_volume_label"), VTF_VOLUME,
			NULL);
			
		gtk_widget_set_sensitive(Tab->LayerUseCombo, layergroups.cur->children_count > 1 );
			
		gtk_combo_box_set_active( GTK_COMBO_BOX(Tab->LayerUseCombo),layergroups.cur->VtfOpt.LayerUse);
		gtk_widget_show(Tab->LayerUseCombo);
		gtk_container_add( GTK_CONTAINER(Tab->LayerUseHBox), Tab->LayerUseCombo );
	
		// Second row
		cur_hbox = gtk_hbox_new(FALSE,0);
		gtk_container_add (GTK_CONTAINER (column_vbox), cur_hbox);
		gtk_widget_show(cur_hbox);
			
		// Simple format selection
		Tab->SimpleFormatContainer = gtk_hbox_new(FALSE,0);
		gtk_container_add (GTK_CONTAINER (cur_hbox), Tab->SimpleFormatContainer);
		gtk_widget_show(Tab->SimpleFormatContainer);
		gtk_widget_set_sensitive(Tab->SimpleFormatContainer,!layergroups.cur->VtfOpt.AdvancedSetup);

		Tab->Compress = gtk_check_button_new_with_mnemonic(_("#with_compression_label"));
		gtk_widget_set_tooltip_markup(Tab->Compress,_("#with_compression_tip"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Tab->Compress),layergroups.cur->VtfOpt.Compress);
			
		gtk_container_add (GTK_CONTAINER (Tab->SimpleFormatContainer), Tab->Compress);
		gtk_widget_show(Tab->Compress);

		Tab->WithAlpha = gtk_check_button_new_with_mnemonic(_("#with_alpha_label"));
		gtk_widget_set_tooltip_markup(Tab->WithAlpha,_("#with_alpha_tip"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Tab->WithAlpha),layergroups.cur->VtfOpt.WithAlpha);
			
		gtk_container_add (GTK_CONTAINER (Tab->SimpleFormatContainer), Tab->WithAlpha);
		gtk_widget_show(Tab->WithAlpha);

		// Advanced format selection toggle
		Tab->AdvancedToggle = gtk_toggle_button_new_with_mnemonic(_("#advanced_mode_label"));
		//gtk_widget_set_tooltip_text(Tab->AdvancedToggle,_("#advanced_mode_tip"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Tab->AdvancedToggle),layergroups.cur->VtfOpt.AdvancedSetup);
			
		gtk_container_add (GTK_CONTAINER (cur_hbox), Tab->AdvancedToggle);
		gtk_widget_show(Tab->AdvancedToggle);

		// Advanced format selection appears on the next row

		// Vertical separator
		cur_separator = gtk_vseparator_new();
		gtk_container_add (GTK_CONTAINER (cur_hbox), cur_separator);
		gtk_widget_show(cur_separator);
			
		// VTF Version
 		Tab->VtfVersion = gtk_combo_box_text_new();
		gtk_widget_set_tooltip_markup(Tab->VtfVersion,_("#vtf_version_label"));
		for (i=0; i < 4; i++)
			gtk_combo_box_append_text(GTK_COMBO_BOX(Tab->VtfVersion),VtfVersions[i]);
			
		gtk_combo_box_set_active(GTK_COMBO_BOX(Tab->VtfVersion),(layergroups.cur->VtfOpt.Version) - 2); // bit of a hack really!
		gtk_widget_show(Tab->VtfVersion);
		gtk_container_add (GTK_CONTAINER (cur_hbox), Tab->VtfVersion);
			
		// Advanced format selection			
			
		// Create the TreeView
		Tab->FormatsView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(FormatsStore));
	
		// Create columns and hook to data
		for(i = 0; i < FORMAT_COLUMN_COUNT; i++)
		{
			// Hey GTK, why not expose header padding values?
			cur_align = gtk_alignment_new(0.5,0,1,1);
			gtk_alignment_set_padding(GTK_ALIGNMENT(cur_align),1,2,5,5);
			gtk_widget_show(cur_align);

			cur_label = gtk_label_new(columns[i].label);
			gtk_container_add(GTK_CONTAINER(cur_align),cur_label);
			gtk_widget_show(cur_label);

			cur_column = gtk_tree_view_column_new_with_attributes(NULL, columns[i].render, "text",i, NULL);
			gtk_tree_view_column_set_widget(cur_column,cur_align);
			gtk_tree_view_append_column(GTK_TREE_VIEW(Tab->FormatsView),cur_column);
		}
				
		// Wrap a frame around the view
		Tab->FormatsView_Frame = gtk_frame_new(NULL);
		gtk_frame_set_shadow_type(GTK_FRAME(Tab->FormatsView_Frame),GTK_SHADOW_ETCHED_IN);
		gtk_container_add (GTK_CONTAINER (column_vbox), Tab->FormatsView_Frame);

		if (layergroups.cur->VtfOpt.AdvancedSetup)
		{
			gtk_widget_show(Tab->FormatsView_Frame);
			gtk_widget_grab_focus(Tab->FormatsView);
		}
		gtk_widget_show(Tab->FormatsView);
		gtk_container_add( GTK_CONTAINER(Tab->FormatsView_Frame), Tab->FormatsView );
	
		cur_separator = gtk_hseparator_new();
		gtk_widget_show(cur_separator);
		gtk_container_add( GTK_CONTAINER(column_vbox), cur_separator );

		//--------
		// Flags
		//--------

		cur_align = gtk_alignment_new(0,0.5,0,0);
		gtk_container_add (GTK_CONTAINER (column_vbox), cur_align);
		gtk_widget_show(cur_align);

		cur_label = gtk_label_new(_("#settings_label"));
		gtk_label_set_use_markup( GTK_LABEL(cur_label), TRUE);
		gtk_container_add (GTK_CONTAINER (cur_align), cur_label);
		gtk_widget_show(cur_label);

		cur_hbox = gtk_hbox_new(FALSE,3);
		gtk_container_add (GTK_CONTAINER (column_vbox), cur_hbox);
		gtk_widget_show(cur_hbox);

		// Mips?
		Tab->DoMips = gtk_check_button_new_with_mnemonic (_("#mipmap_label"));
		gtk_widget_set_tooltip_markup(Tab->DoMips,_("#mipmap_tip"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (Tab->DoMips), layergroups.cur->VtfOpt.WithMips);
		gtk_widget_show(Tab->DoMips);
		gtk_container_add( GTK_CONTAINER(cur_hbox), Tab->DoMips );

		// LOD?
		Tab->NoLOD = gtk_check_button_new_with_mnemonic(_("#nolod_label"));
		gtk_widget_set_tooltip_markup(Tab->NoLOD,_("#nolod_tip"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (Tab->NoLOD), layergroups.cur->VtfOpt.NoLOD);
		gtk_widget_show(Tab->NoLOD);
		gtk_container_add( GTK_CONTAINER(cur_hbox), Tab->NoLOD );

		// Clamp?
		Tab->Clamp = gtk_check_button_new_with_mnemonic(_("#clamp_label"));
		gtk_widget_set_tooltip_markup(Tab->Clamp,_("#clamp_tip"));
			
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (Tab->Clamp), layergroups.cur->VtfOpt.Clamp);
		gtk_widget_show(Tab->Clamp);
		gtk_container_add( GTK_CONTAINER(cur_hbox), Tab->Clamp );

		// Bump map?
		Tab->BumpType = gtk_combo_box_new_text();
		gtk_widget_set_tooltip_markup(Tab->BumpType,_("#bump_map_tip"));
		for (i=0; i < 3; i++)
			gtk_combo_box_append_text(GTK_COMBO_BOX(Tab->BumpType),_(BumpRadioLabels[i]));
		gtk_combo_box_set_active(GTK_COMBO_BOX(Tab->BumpType),layergroups.cur->VtfOpt.BumpType);
			
		gtk_widget_show(Tab->BumpType);
		gtk_container_add( GTK_CONTAINER(cur_hbox), Tab->BumpType );

		cur_hbox = gtk_hbox_new(FALSE,3);
		gtk_container_add (GTK_CONTAINER (column_vbox), cur_hbox);
		gtk_widget_show(cur_hbox);
	
		// LOD control
		Tab->LODControlHBox = gtk_hbox_new(FALSE,3);
		gtk_container_add (GTK_CONTAINER (cur_hbox), Tab->LODControlHBox);
		gtk_widget_set_tooltip_markup(Tab->LODControlHBox,_("#lod_control_tip"));
		gtk_widget_show(Tab->LODControlHBox);

		cur_label = gtk_label_new(_("#lod_control_label"));
		gtk_container_add (GTK_CONTAINER (Tab->LODControlHBox), cur_label);
		gtk_box_set_child_packing(GTK_BOX(Tab->LODControlHBox),cur_label,FALSE,TRUE,2,GTK_PACK_START);
		gtk_widget_show(cur_label);

		Tab->LODControlSlider = gtk_hscale_new_with_range(0,3,1); // limit to four steps for UI sanity
		gtk_scale_set_value_pos(GTK_SCALE(Tab->LODControlSlider),GTK_POS_LEFT);
		gtk_range_set_inverted( GTK_RANGE(Tab->LODControlSlider), TRUE);
		gtk_range_set_increments( GTK_RANGE(Tab->LODControlSlider), 1,1);
		if (layergroups.cur->VtfOpt.LodControlU)
			gtk_range_set_value(GTK_RANGE(Tab->LODControlSlider),exponentU - layergroups.cur->VtfOpt.LodControlU);	
			
		gtk_container_add (GTK_CONTAINER (Tab->LODControlHBox), Tab->LODControlSlider);
		gtk_widget_show(Tab->LODControlSlider);
	
		cur_separator = gtk_hseparator_new();
		gtk_widget_show(cur_separator);
		gtk_container_add( GTK_CONTAINER(column_vbox), cur_separator );
	
		//----------------------------
		// Use layer as alpha channel
		//----------------------------
	
		cur_align = gtk_alignment_new(0,0.5,0,0);
		gtk_container_add (GTK_CONTAINER (column_vbox), cur_align);
		gtk_widget_show(cur_align);

		Tab->AlphaLayerLabel = gtk_label_new(_("#layer_an_alpha_label"));
		gtk_label_set_use_markup( GTK_LABEL(Tab->AlphaLayerLabel), TRUE);
		gtk_container_add (GTK_CONTAINER (cur_align), Tab->AlphaLayerLabel);
		gtk_widget_show(Tab->AlphaLayerLabel);
		gtk_widget_set_sensitive(Tab->AlphaLayerLabel, vtf_format_has_alpha(layergroups.cur->VtfOpt.PixelFormat) );

		cur_hbox = gtk_hbox_new(FALSE,0);
		gtk_widget_set_tooltip_markup(cur_hbox,_("#layer_as_alpha_tip"));
		gtk_container_add (GTK_CONTAINER (column_vbox), cur_hbox);
		gtk_widget_show(cur_hbox);

		// Checkbox
		Tab->AlphaLayerEnable = gtk_check_button_new();
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (Tab->AlphaLayerEnable), layergroups.cur->VtfOpt.AlphaLayerTattoo);
		gtk_widget_show(Tab->AlphaLayerEnable);
		gtk_container_add (GTK_CONTAINER (cur_hbox), Tab->AlphaLayerEnable);
		gtk_box_set_child_packing(GTK_BOX(cur_hbox),Tab->AlphaLayerEnable,FALSE,TRUE,2,GTK_PACK_START);
			
		// Selection combo
		Tab->AlphaLayerCombo = gimp_layer_combo_box_new(alpha_channel_suitable, (void*)&image_ID);
		gtk_widget_show(Tab->AlphaLayerCombo);
		gtk_container_add (GTK_CONTAINER (cur_hbox), Tab->AlphaLayerCombo);
		if (layergroups.cur->VtfOpt.AlphaLayerTattoo)
			gimp_int_combo_box_set_active( GIMP_INT_COMBO_BOX(Tab->AlphaLayerCombo), gimp_image_get_layer_by_tattoo(image_ID,layergroups.cur->VtfOpt.AlphaLayerTattoo) );		
			
			
		// Configure feature availability
		update_lod_availability();
		update_alpha_layer_availability();
	}

	for (LAYERGROUPS_ITERATE)
	{
		// Creating these in the loop above leads to race condition crashes as the active tab changes
		TabControls_t* Tab = &layergroups.cur->UI;

		g_signal_connect(Tab->ExportCheckbox,		"toggled",		G_CALLBACK(toggle_export),				layergroups.cur);
		g_signal_connect(Tab->LayerUseCombo,		"changed",		G_CALLBACK(change_frame_use),			NULL);
		g_signal_connect(Tab->Compress,				"toggled",		G_CALLBACK(gimp_toggle_button_update),	&layergroups.cur->VtfOpt.Compress);
		g_signal_connect(Tab->WithAlpha,			"toggled",		G_CALLBACK(choose_simple_alpha),		NULL);
		g_signal_connect(Tab->AdvancedToggle,		"toggled",		G_CALLBACK(change_format_select_mode),	NULL);
		g_signal_connect(Tab->VtfVersion,			"changed",		G_CALLBACK(choose_vtf_version),			NULL);
		g_signal_connect(Tab->DoMips,				"toggled",		G_CALLBACK(change_withmips),			NULL);
		g_signal_connect(Tab->NoLOD,				"toggled",		G_CALLBACK(change_nolod),				NULL);
		g_signal_connect(Tab->Clamp,				"toggled",		G_CALLBACK(gimp_toggle_button_update),	&layergroups.cur->VtfOpt.Clamp);
		g_signal_connect(Tab->BumpType,				"changed",		G_CALLBACK(choose_bump_type),			NULL);
		g_signal_connect(Tab->LODControlSlider,		"format-value",	G_CALLBACK(change_lod_control),			NULL);
		g_signal_connect(Tab->AlphaLayerEnable,		"toggled",		G_CALLBACK(toggle_alpha_layer),			NULL);
		g_signal_connect(Tab->AlphaLayerCombo,		"changed",		G_CALLBACK(choose_alpha_layer),		NULL);

		{
			GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(Tab->FormatsView));
			gtk_tree_selection_set_mode(selection,GTK_SELECTION_BROWSE);
			g_signal_connect(selection, "changed", GTK_SIGNAL_FUNC(select_compression), NULL);
		}

		change_format_select_mode(GTK_TOGGLE_BUTTON(Tab->AdvancedToggle),NULL);
	}

	layergroups.cur = layergroups.head;
	
	// Render window
	gtk_widget_show (dialog);

	run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

	gtk_widget_destroy (dialog);

	return run;
}