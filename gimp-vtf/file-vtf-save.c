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
#include <libgimp/gimpexport.h>
#include <libgimp/gimpui.h>

#include "file-vtf.h"

/*
 * Logic
 */

gint	num_layers;
guint	exponentU;
guint	exponentV;

VtfSaveOptions gimpVTFOpt = { 4, FALSE, FALSE, TRUE, 0, FALSE, FALSE, TRUE, NOT_BUMP, VTF_MERGE_VISIBLE, 0, 0, 0, 0 };

gint select_vtf_format_index(const VtfSaveOptions* opt)
{
	if (opt->AdvancedSetup)
		return gimpVTFOpt.PixelFormat;

	if (opt->Compress)
	{
		if (opt->WithAlpha)
			return 3;
		else
			return 0;
	}
	else
	{
		if (opt->WithAlpha)
			return 5;
		else
			return 4;
	}
}

gboolean fix_alpha_layer(VtfSaveOptions* opt, gint32 image_id)
{
	if ( opt->AlphaLayerTattoo && gimp_image_get_layer_by_tattoo(image_id,opt->AlphaLayerTattoo) == -1 )
	{
		opt->AlphaLayerTattoo = 0;
		return TRUE;
	}
	return FALSE;
}

gboolean is_drawable_image_size(gint32 drawable_ID)
{
	return gimp_drawable_width(drawable_ID) == gimp_image_width(image_ID) && gimp_drawable_height(drawable_ID) == gimp_image_height(image_ID);
}

void vtf_size_error(gchar* dimension,gint value)
{
	static gchar buf[256];
	gint next = NextPowerOfTwo(value);
	snprintf(buf,256,_("#image_size_error"),dimension,value,dimension,next >> 1, next);
	quit_error(buf);
	return;
}

gboolean show_options(const gint32 image_ID);

void save(gint nparams, const GimpParam* param, gint* nreturn_vals)
{
	gint			width;
	gint			height;
	const gchar*	filename;
	const gchar*	settings_ID;
	
	GimpRunMode		run_mode;
	gint32			drawable_ID;
	gint32*			layer_IDs;
	
	gint32			alpha_layer_ID_original = -1;
	gint32			alpha_layer_ID = -1;
	gint			alpha_layer_position;
	gboolean		alpha_layer_started_visible = FALSE;

	guint			frame=1,face=1,slice=1,dummy=0,layers_to_export=0;
	guint*			layer_iterator;

	gchar*			progress_frame_label;

	GimpImageBaseType	img_type;
	gint				colourmap_size;	
	
	SVTFCreateOptions	vlVTFOpt;
	vlByte**			rbgaImages;
	guint				imageBytes;

	guint			i;
	
	// ------------
	run_mode = (GimpRunMode)param[0].data.d_int32;

	image_ID	= param[1].data.d_int32;
	drawable_ID	= param[2].data.d_int32;
	filename	= param[3].data.d_string;
		
	width = gimp_image_width(image_ID);
	height = gimp_image_height(image_ID);
	imageBytes = width * height * 4;
	
	if ( !IsPowerOfTwo(width) )
	{
		vtf_size_error(_("#width_word"),width);
		return;
	}
		
	if ( !IsPowerOfTwo(height) )
	{
		vtf_size_error(_("#height_word"),height);
		return;
	}

	// Calculate LOD exponents
	for (exponentU=2; pow(2.0f,(int)exponentU) != width; exponentU++) {}
	for (exponentV=2; pow(2.0f,(int)exponentV) != height; exponentV++) {}
	
	layer_IDs = gimp_image_get_layers(image_ID,&num_layers);
	settings_ID = get_vtf_options_ID(image_ID);
	
	gimp_get_data(settings_ID, &gimpVTFOpt);
	gimp_ui_init(PLUG_IN_BINARY, FALSE);
	if ( fix_alpha_layer(&gimpVTFOpt,image_ID) )
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
	
	switch(run_mode)
	{
	case GIMP_RUN_INTERACTIVE:	
		if ( !show_options(image_ID) )
		{
			vtf_ret_values[0].data.d_status = GIMP_PDB_CANCEL;
			return;
		}
		// Validate the alpha layer again, just in case
		fix_alpha_layer(&gimpVTFOpt,image_ID);
		break;
	case GIMP_RUN_NONINTERACTIVE:
		if (nparams == 15)
		{
			gimpVTFOpt.PixelFormat = param[5].data.d_int8;
			gimpVTFOpt.AlphaLayerTattoo = param[6].data.d_int32;
			if (fix_alpha_layer(&gimpVTFOpt,image_ID))
			{
				quit_error(_("#invalid_alpha_error"));
				return;
			}
			gimpVTFOpt.LayerUse = (VtfLayerUse)param[7].data.d_int8;
			gimpVTFOpt.Version = param[8].data.d_int8;
			gimpVTFOpt.WithMips = param[9].data.d_int8;
			gimpVTFOpt.NoLOD = param[10].data.d_int8;
			gimpVTFOpt.Clamp = param[11].data.d_int8;
			gimpVTFOpt.BumpType = (VtfBumpType)param[12].data.d_int8;
			gimpVTFOpt.LodControlU = param[13].data.d_int8;
			gimpVTFOpt.LodControlV = param[14].data.d_int8;
		}
		else
		{
			vtf_ret_values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
			return;
		}
		break;
	}

	// Set up creation options
	vlImageCreateDefaultCreateStructure(&vlVTFOpt);
	vlVTFOpt.ImageFormat = vtf_formats[select_vtf_format_index(&gimpVTFOpt)].vlFormat;

	vlVTFOpt.uiFlags = gimpVTFOpt.GeneralFlags; // Import unhandled flags from a loaded VTF

	vlVTFOpt.uiVersion[1] = gimpVTFOpt.Version;
	vlVTFOpt.bMipmaps = gimpVTFOpt.WithMips;
	if (gimpVTFOpt.Clamp)
		vlVTFOpt.uiFlags |= TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT;
	if (gimpVTFOpt.NoLOD)
		vlVTFOpt.uiFlags |= TEXTUREFLAGS_NOLOD;
	switch (gimpVTFOpt.BumpType)
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

	// Start working on the image
	gimp_image_undo_freeze(image_ID);

	if ( gimpVTFOpt.AlphaLayerTattoo && vtf_format_has_alpha(select_vtf_format_index(&gimpVTFOpt)) )
	{
		alpha_layer_ID = gimp_image_get_layer_by_tattoo(image_ID,gimpVTFOpt.AlphaLayerTattoo);
		alpha_layer_position = gimp_image_get_layer_position(image_ID,alpha_layer_ID);
		alpha_layer_started_visible = gimp_drawable_get_visible(alpha_layer_ID);
		
		// resize the alpha layer if needed
		if (!is_drawable_image_size(alpha_layer_ID))
		{
			alpha_layer_ID_original = alpha_layer_ID;
			alpha_layer_ID = gimp_layer_new_from_drawable(alpha_layer_ID,image_ID);
			gimp_image_add_layer(image_ID,alpha_layer_ID,0);
			gimp_layer_resize_to_image_size(alpha_layer_ID);
			gimp_drawable_set_visible(alpha_layer_ID,FALSE);
		}

		// Cannot remove the layers, as that also deletes them
		gimp_image_lower_layer_to_bottom(image_ID,alpha_layer_ID);
		if (alpha_layer_ID_original != -1)
			gimp_image_lower_layer_to_bottom(image_ID,alpha_layer_ID_original);

		layer_IDs = gimp_image_get_layers(image_ID,&num_layers);
	}
	
	layers_to_export = num_layers;
	if (alpha_layer_ID != -1) layers_to_export--;
	if (alpha_layer_ID_original != -1) layers_to_export--;

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

	switch(gimpVTFOpt.LayerUse)
	{
	case VTF_MERGE_VISIBLE:
		// Hide alpha layers
		if (alpha_layer_started_visible)
		{
			gimp_drawable_set_visible(alpha_layer_ID,FALSE);
			if (alpha_layer_ID_original != -1) gimp_drawable_set_visible(alpha_layer_ID_original,FALSE);			
		}
		// merge visible
		drawable_ID = gimp_layer_new_from_visible(image_ID,image_ID,_("#combined_layer_title"));
		// restore alpha visibility
		if (alpha_layer_started_visible)
		{
			gimp_drawable_set_visible(alpha_layer_ID,TRUE);
			if (alpha_layer_ID_original != -1) gimp_drawable_set_visible(alpha_layer_ID_original,TRUE);
		}
		
		// Set things up so that we only export the new layer
		layer_IDs[0] = drawable_ID;
		layers_to_export = 1;
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
	if (gimpVTFOpt.LayerUse == VTF_MERGE_VISIBLE)
		if (num_layers == 1)
			gimp_progress_init(_("#save_message_single"));
		else
			gimp_progress_init(_("#save_message_combined"));
	else
		gimp_progress_init_printf(_("#save_message_multi"),layers_to_export,progress_frame_label);
	
	rbgaImages = g_new(vlByte*,num_layers);
	if (!rbgaImages)
	{
		quit_error_mem();
		return;
	}	

	// the layer iterator pointer increments either the frame, face or slice value (or a dummy in the case of VTF_MERGE_VISIBLE)
	for ( *layer_iterator = 0; *layer_iterator < layers_to_export; (*layer_iterator)++)
	{
		rbgaImages[*layer_iterator] = g_new(vlByte,imageBytes);
		if (!rbgaImages[*layer_iterator])
		{
			quit_error_mem();
			return;
		}
	}
	for ( *layer_iterator = 0; *layer_iterator < layers_to_export; (*layer_iterator)++)
	{
		GimpPixelRgn	pixel_rgn;
		GimpDrawable*	drawable;
		gboolean		layer_duplicated = FALSE;

		drawable_ID = layer_IDs[*layer_iterator];
		g_assert(drawable_ID != alpha_layer_ID);

		if ( alpha_layer_ID != -1 || !is_drawable_image_size(drawable_ID) )
		{
			// we've not changed the layer yet, so just overwrite the drawable ID
			drawable_ID = gimp_layer_new_from_drawable(layer_IDs[*layer_iterator],image_ID);

			gimp_image_add_layer(image_ID,drawable_ID,-1);
			gimp_layer_resize_to_image_size(drawable_ID);
			layer_duplicated = TRUE;
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
			quit_error(_("#internal_4bbp_error"));
			return;
		}

		// Put pixels into array, from last layer to first thanks to GIMP
		gimp_pixel_rgn_init(&pixel_rgn, drawable, 0, 0, width, height, FALSE, FALSE);
		gimp_pixel_rgn_get_rect(&pixel_rgn, rbgaImages[(layers_to_export - 1) - *layer_iterator], 0,0, width,height);

		// Cleanup
		if (layer_duplicated)
			gimp_image_remove_layer(image_ID,drawable_ID);
	}

	if (alpha_layer_ID_original != -1)
	{
		gimp_image_remove_layer(image_ID,alpha_layer_ID);
		alpha_layer_ID = alpha_layer_ID_original;
	}
	if (alpha_layer_ID != -1)
	{
		while (gimp_image_get_layer_position(image_ID,alpha_layer_ID) != alpha_layer_position)
			gimp_image_raise_layer(image_ID,alpha_layer_ID);
		gimp_displays_flush();
	}

	gimp_progress_pulse(); // crap, but nothing better to do
	
	// Hand off to VTFLib
	if ( vlImageCreateMultiple(width,height,frame,face,slice,rbgaImages,&vlVTFOpt) )
	{
		if ( vlImageGetSupportsResources() )
		{
			if (gimpVTFOpt.WithMips && gimpVTFOpt.LodControlU != 0 && gimpVTFOpt.LodControlV != 0 )
			{
				gchar buf[4]; // must be 4, not 2
				memset(buf,0,4);
				buf[0] = gimpVTFOpt.LodControlU;
				buf[1] = gimpVTFOpt.LodControlV;
				vlImageSetResourceData(VTF_RSRC_TEXTURE_LOD_SETTINGS,4,buf);
			}
		}
		if ( vlImageSave(filename) )
			vtf_ret_values[0].data.d_status = GIMP_PDB_SUCCESS;
	}
	
	// Failed!
	if ( vtf_ret_values[0].data.d_status != GIMP_PDB_SUCCESS )
	{
      vtf_ret_values[1].type          = GIMP_PDB_STRING;
      vtf_ret_values[1].data.d_string = (gchar*)vlGetLastError();
	}
	else if ( vtf_format_is_compressed(gimpVTFOpt.PixelFormat) && gimpVTFOpt.BumpType != NOT_BUMP)
	{
		// Passing to the console is crap because it is not visible by default, but until there is a
		// way to specify that you want a GUI message to appear without requiring user interaction
		// is the only sensible choice.
		gimp_message_set_handler(GIMP_CONSOLE);
		gimp_message(_("#compressed_bump_warning"));
		gimp_message_set_handler(GIMP_MESSAGE_BOX);
	}

	// Save settings
	if (run_mode == GIMP_RUN_INTERACTIVE)
		gimp_set_data (settings_ID, &gimpVTFOpt, sizeof (VtfSaveOptions));

	// Free memory (not strictly needed as we're about to terminate, but hey)
	for(i=0; i < layers_to_export; i++)
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
GtkWidget*	main_vbox;
GtkWidget*	cur_hbox;
GtkWidget*	cur_label;
GtkWidget*	cur_align;
GtkWidget*	cur_separator;

GtkWidget*	SimpleFormatContainer;
GtkWidget*	WithAlpha;
GtkWidget*	Compress;
GtkWidget*	AdvancedToggle;
GtkCList*	comp_list;
GtkWidget*	comp_list_widget;

GtkWidget*	DoMips;
GtkWidget*	NoLOD;
GtkWidget*	Clamp;
GtkWidget*	BumpType;
GSList*		BumpRadioGroup = NULL;
gchar*		BumpRadioLabels[] = { "#not_bump", "#bump_flag", "#ssbump_flag" };

GtkWidget*	LODControlHBox;
GtkWidget*	LODControlSlider;

GtkWidget*	ClearOtherFlags;

GtkWidget*	VtfVersion;
gchar*		VtfVersions[] = { "7.2", "7.3", "7.4", "7.5" };

GtkWidget*	LayerUseCombo;
GtkWidget*	LayerUseLabel;
GtkWidget*	LayerUseHBox;

GtkWidget* alpha_layer_select;
GtkWidget* alpha_layer_chbx;
GtkWidget* alpha_layer_label;

static void update_lod_availability()
{
	gtk_widget_set_sensitive(NoLOD,gimpVTFOpt.WithMips);
	gtk_widget_set_sensitive(LODControlHBox,gimpVTFOpt.Version >= 3 && gimpVTFOpt.WithMips && !gimpVTFOpt.NoLOD);
}

static void set_layer_alpha_active(gboolean active)
{
	gtk_widget_set_sensitive(alpha_layer_label, active );

	if ( num_layers < 2 )
		active = FALSE; // _possible_ to re-use the only RGB layer for alpha, but unsupported

	gtk_widget_set_sensitive(alpha_layer_chbx, active );
	gtk_widget_set_sensitive(alpha_layer_select, active && gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(alpha_layer_chbx) ) );	
}

static void update_alpha_layer_availability()
{	
	if (gimpVTFOpt.AdvancedSetup)
		set_layer_alpha_active(vtf_format_has_alpha(gimpVTFOpt.PixelFormat));
	else
		set_layer_alpha_active(gimpVTFOpt.WithAlpha);
}

static void select_compression(GtkWidget *widget, gint row, gint column, GdkEventButton *event, gpointer user_data) 
{
	if ( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(AdvancedToggle) ) )
		return;

	gimpVTFOpt.PixelFormat = row;
	update_alpha_layer_availability();

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Compress),vtf_format_is_compressed(row));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(WithAlpha),vtf_format_has_alpha(row));
}

static void update_layer_use_availability()
{
	gtk_widget_set_sensitive(LayerUseHBox, gimpVTFOpt.AlphaLayerTattoo ? num_layers > 2 : num_layers > 1);
}

static void set_alpha_layer()
{
	gint alpha_id;
	gimp_int_combo_box_get_active(GIMP_INT_COMBO_BOX(alpha_layer_select), &alpha_id);
	gimpVTFOpt.AlphaLayerTattoo = gimp_drawable_get_tattoo(alpha_id);
	update_layer_use_availability();
}

static void toggle_alpha_layer(GtkToggleButton *togglebutton, gpointer user_data)
{		
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(togglebutton) ) )
	{
		gint alpha_id = -1;
		gimp_int_combo_box_get_active(GIMP_INT_COMBO_BOX(alpha_layer_select), &alpha_id);
		if (alpha_id != -1)
			set_alpha_layer();
		gtk_widget_set_sensitive(alpha_layer_select,TRUE);
	}
	else
	{
		gimpVTFOpt.AlphaLayerTattoo = 0;
		gtk_widget_set_sensitive(alpha_layer_select,FALSE);
	}
	update_layer_use_availability();
}

static void choose_alpha_layer(GtkComboBox* combo, gpointer user_data)
{
	set_alpha_layer();
}

static void choose_simple_alpha(GtkCheckButton* chbx, gpointer user_data)
{
	gimpVTFOpt.WithAlpha = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chbx));
	update_alpha_layer_availability();
}

static void choose_vtf_version(GtkComboBox* combo, gpointer user_data)
{
	gimpVTFOpt.Version = gtk_combo_box_get_active(GTK_COMBO_BOX(combo)) + 2;
	update_lod_availability();
}

static void choose_bump_type(GtkComboBox* combo, gpointer user_data)
{
	gimpVTFOpt.BumpType = (VtfBumpType)gtk_combo_box_get_active(combo);
}

static void change_format_select_mode(GtkToggleButton* toggle, gpointer user_data)
{
	gboolean to_advanced = gtk_toggle_button_get_active(toggle);

	gtk_widget_set_sensitive(SimpleFormatContainer,!to_advanced);

	if (to_advanced)
	{
		gtk_widget_show(comp_list_widget);
		gtk_clist_select_row( comp_list, select_vtf_format_index(&gimpVTFOpt), 0 ); // must do this *before* setting gimpVTFOpt.AdvancedSetup
	}
	else
		gtk_widget_hide(comp_list_widget);

	gimpVTFOpt.AdvancedSetup = to_advanced;
	update_alpha_layer_availability();
}

static void change_frame_use(GtkWidget* combo, gpointer user_data)
{
	gimpVTFOpt.LayerUse = (VtfLayerUse)gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void change_withmips(GtkToggleButton* toggle, gpointer user_data)
{
	gimpVTFOpt.WithMips = gtk_toggle_button_get_active(toggle);
	update_lod_availability();
}

static void change_nolod(GtkToggleButton* toggle, gpointer user_data)
{
	gimpVTFOpt.NoLOD = gtk_toggle_button_get_active(toggle);
	update_lod_availability();
}

static gboolean alpha_channel_suitable(gint32 image_id, gint32 drawable_id, gpointer user_data)
{
	return num_layers > 1 && image_id == *(gint32*)user_data;
	/*return ( !gimp_drawable_has_alpha(drawable_id)
		&& gimp_drawable_height(drawable_id) == gimp_image_height(image_id)
		&& gimp_drawable_width(drawable_id) == gimp_image_width(image_id) );*/
}

static gchar* change_lod_control(GtkScale* scale, gdouble value, gpointer user_data)
{	
	gimpVTFOpt.LodControlU = exponentU - (gint)value;
	gimpVTFOpt.LodControlV = exponentV - (gint)value;
	return g_strdup_printf("%ix%i", (gint)pow(2.0f,(int)gimpVTFOpt.LodControlU), (gint)pow(2.0f,(int)gimpVTFOpt.LodControlV) ); // casting to hide bogus Intellisense warnings
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
		
	gint width,height;

	//gint num_general_flags = 0;
	//gchar clearflags_label[32];

	gchar title[256];
	snprintf(title,256,_("#window_title"),gimp_image_get_name(image_ID));

	width = gimp_image_width(image_ID);
	height = gimp_image_height(image_ID);

	dialog = gimp_dialog_new (title, PLUG_IN_BINARY, NULL, (GtkDialogFlags)0,
	
	HELP_FUNC,			"file-vtf",
	GTK_STOCK_CANCEL,	GTK_RESPONSE_CANCEL,
	GTK_STOCK_OK,		GTK_RESPONSE_OK,

	NULL);

	gtk_window_set_resizable(GTK_WINDOW(dialog),FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog),GTK_WIN_POS_CENTER);

	// Padding
	cur_align = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_widget_show (cur_align);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), cur_align);
	gtk_alignment_set_padding (GTK_ALIGNMENT(cur_align), 6, 6, 6, 6);

	// Vertical align
	main_vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (cur_align), main_vbox);
	gtk_widget_show (main_vbox);

	// Format selection	
	cur_align = gtk_alignment_new(0,0.5,0,0);
	gtk_container_add (GTK_CONTAINER (main_vbox), cur_align);
	gtk_widget_show(cur_align);

	cur_label = gtk_label_new(_("#format_title"));
	gtk_label_set_use_markup( GTK_LABEL(cur_label), TRUE);
	gtk_container_add (GTK_CONTAINER (cur_align), cur_label);
	gtk_widget_show(cur_label);

	// Layer use
	LayerUseHBox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show(LayerUseHBox);
	gtk_container_add( GTK_CONTAINER(main_vbox), LayerUseHBox );

	LayerUseLabel = gtk_label_new_with_mnemonic(_("#layers_use_label"));
	gtk_widget_show(LayerUseLabel);
	gtk_widget_set_sensitive(LayerUseLabel, num_layers > 1 );
	gtk_box_pack_start (GTK_BOX (LayerUseHBox), LayerUseLabel, FALSE, FALSE, 0);

	LayerUseCombo = gimp_int_combo_box_new (
		_("#layers_merge_visible_label"), VTF_MERGE_VISIBLE,
		_("#layers_animation_label"),    VTF_ANIMATION,
		_("#layers_envmap_label"), VTF_ENVMAP,
		_("#layers_volume_label"), VTF_VOLUME,
		NULL);

	g_signal_connect (LayerUseCombo, "changed", G_CALLBACK (change_frame_use), NULL);
	gtk_widget_set_sensitive(LayerUseCombo, num_layers > 1 );

	gtk_combo_box_set_active( GTK_COMBO_BOX(LayerUseCombo),gimpVTFOpt.LayerUse);
	gtk_widget_show(LayerUseCombo);
	gtk_container_add( GTK_CONTAINER(LayerUseHBox), LayerUseCombo );
	
	// Second row
	cur_hbox = gtk_hbox_new(FALSE,0);
	gtk_container_add (GTK_CONTAINER (main_vbox), cur_hbox);
	gtk_widget_show(cur_hbox);

	// Simple format selection
	SimpleFormatContainer = gtk_hbox_new(FALSE,0);
	gtk_container_add (GTK_CONTAINER (cur_hbox), SimpleFormatContainer);
	gtk_widget_show(SimpleFormatContainer);
	gtk_widget_set_sensitive(SimpleFormatContainer,!gimpVTFOpt.AdvancedSetup);

	Compress = gtk_check_button_new_with_mnemonic(_("#with_compression_label"));
	gtk_widget_set_tooltip_markup(Compress,_("#with_compression_tip"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Compress),gimpVTFOpt.Compress);
	g_signal_connect (Compress, "toggled", G_CALLBACK (gimp_toggle_button_update), &gimpVTFOpt.Compress);
	gtk_container_add (GTK_CONTAINER (SimpleFormatContainer), Compress);
	gtk_widget_show(Compress);

	WithAlpha = gtk_check_button_new_with_mnemonic(_("#with_alpha_label"));
	gtk_widget_set_tooltip_markup(WithAlpha,_("#with_alpha_tip"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(WithAlpha),gimpVTFOpt.WithAlpha);
	g_signal_connect (WithAlpha, "toggled", G_CALLBACK (choose_simple_alpha), NULL);
	gtk_container_add (GTK_CONTAINER (SimpleFormatContainer), WithAlpha);
	gtk_widget_show(WithAlpha);

	// Advanced format selection toggle
	AdvancedToggle = gtk_toggle_button_new_with_mnemonic(_("#advanced_mode_label"));
	//gtk_widget_set_tooltip_text(AdvancedToggle,_("#advanced_mode_tip"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(AdvancedToggle),gimpVTFOpt.AdvancedSetup);
	g_signal_connect(AdvancedToggle, "toggled", G_CALLBACK (change_format_select_mode), NULL);
	gtk_container_add (GTK_CONTAINER (cur_hbox), AdvancedToggle);
	gtk_widget_show(AdvancedToggle);

	// Advanced format selection appears on the next row

	// Vertical separator
	cur_separator = gtk_vseparator_new();
	gtk_container_add (GTK_CONTAINER (cur_hbox), cur_separator);
	gtk_widget_show(cur_separator);

	// VTF Version
	VtfVersion = gtk_combo_box_new_text();
	gtk_widget_set_tooltip_markup(VtfVersion,_("#vtf_version_label"));
	for (i=0; i < 4; i++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(VtfVersion),VtfVersions[i]);
	g_signal_connect (VtfVersion, "changed", G_CALLBACK (choose_vtf_version), NULL);

	gtk_combo_box_set_active(GTK_COMBO_BOX(VtfVersion),gimpVTFOpt.Version - 2); // bit of a hack really!
	gtk_widget_show(VtfVersion);
	gtk_container_add (GTK_CONTAINER (cur_hbox), VtfVersion);

	// Advanced format selection
	comp_list_widget = gtk_clist_new(2);
	comp_list = (GtkCList*)comp_list_widget;
	
	gtk_clist_set_selection_mode(comp_list,GTK_SELECTION_SINGLE);
	gtk_clist_set_shadow_type(comp_list,GTK_SHADOW_ETCHED_IN);
	
	gtk_clist_set_column_title(comp_list,0,_("#alpha_channel_title"));
	gtk_clist_set_column_title(comp_list,1,_("#pixel_format_title"));
	gtk_clist_column_titles_passive(comp_list);
	gtk_clist_column_titles_show(comp_list);

	gtk_clist_set_column_justification(comp_list,0,GTK_JUSTIFY_CENTER);

	for (i=0;i<num_vtf_formats;i++)
	{
		gchar* cur_item[] = { vtf_formats[i].alpha_label, vtf_formats[i].label };
		gtk_clist_append(comp_list,cur_item);
	}

	// Select last used setting
	gtk_clist_select_row( comp_list, gimpVTFOpt.PixelFormat, 0 );
	comp_list->focus_row = gimpVTFOpt.PixelFormat;
	
	// Hook this AFTER restoring the last setting!
	gtk_signal_connect(GTK_OBJECT(comp_list_widget), "select_row", GTK_SIGNAL_FUNC(select_compression), NULL);

	if (gimpVTFOpt.AdvancedSetup)
	{
		gtk_widget_show(comp_list_widget);		
	}
	gtk_container_add( GTK_CONTAINER(main_vbox), comp_list_widget );
	
	cur_separator = gtk_hseparator_new();
	gtk_widget_show(cur_separator);
	gtk_container_add( GTK_CONTAINER(main_vbox), cur_separator );

	//--------
	// Flags
	//--------

	cur_align = gtk_alignment_new(0,0.5,0,0);
	gtk_container_add (GTK_CONTAINER (main_vbox), cur_align);
	gtk_widget_show(cur_align);

	cur_label = gtk_label_new(_("#settings_label"));
	gtk_label_set_use_markup( GTK_LABEL(cur_label), TRUE);
	gtk_container_add (GTK_CONTAINER (cur_align), cur_label);
	gtk_widget_show(cur_label);

	cur_hbox = gtk_hbox_new(FALSE,3);
	gtk_container_add (GTK_CONTAINER (main_vbox), cur_hbox);
	gtk_widget_show(cur_hbox);

	// Mips?
	DoMips = gtk_check_button_new_with_mnemonic (_("#mipmap_label"));
	gtk_widget_set_tooltip_markup(DoMips,_("#mipmap_tip"));
	g_signal_connect (DoMips, "toggled", G_CALLBACK (change_withmips), NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (DoMips), gimpVTFOpt.WithMips);
	gtk_widget_show(DoMips);
	gtk_container_add( GTK_CONTAINER(cur_hbox), DoMips );

	// LOD?
	NoLOD = gtk_check_button_new_with_mnemonic(_("#nolod_label"));
	gtk_widget_set_tooltip_markup(NoLOD,_("#nolod_tip"));
	g_signal_connect (NoLOD, "toggled", G_CALLBACK (change_nolod), NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (NoLOD), gimpVTFOpt.NoLOD);
	gtk_widget_show(NoLOD);
	gtk_container_add( GTK_CONTAINER(cur_hbox), NoLOD );

	// Clamp?
	Clamp = gtk_check_button_new_with_mnemonic(_("#clamp_label"));
	gtk_widget_set_tooltip_markup(Clamp,_("#clamp_tip"));
	g_signal_connect (Clamp, "toggled", G_CALLBACK (gimp_toggle_button_update), &gimpVTFOpt.Clamp);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (Clamp), gimpVTFOpt.Clamp);
	gtk_widget_show(Clamp);
	gtk_container_add( GTK_CONTAINER(cur_hbox), Clamp );

	// Bump map?
	BumpType = gtk_combo_box_new_text();
	gtk_widget_set_tooltip_markup(BumpType,_("#bump_map_tip"));
	for (i=0; i < 3; i++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(BumpType),_(BumpRadioLabels[i]));
	gtk_combo_box_set_active(GTK_COMBO_BOX(BumpType),gimpVTFOpt.BumpType);
	g_signal_connect (BumpType, "changed", G_CALLBACK (choose_bump_type), NULL);
	gtk_widget_show(BumpType);
	gtk_container_add( GTK_CONTAINER(cur_hbox), BumpType );

	cur_hbox = gtk_hbox_new(FALSE,3);
	gtk_container_add (GTK_CONTAINER (main_vbox), cur_hbox);
	gtk_widget_show(cur_hbox);
	
	// LOD control
	LODControlHBox = gtk_hbox_new(FALSE,3);
	gtk_container_add (GTK_CONTAINER (cur_hbox), LODControlHBox);
	gtk_widget_set_tooltip_markup(LODControlHBox,_("#lod_control_tip"));
	gtk_widget_show(LODControlHBox);

	cur_label = gtk_label_new(_("#lod_control_label"));
	gtk_container_add (GTK_CONTAINER (LODControlHBox), cur_label);
	gtk_box_set_child_packing(GTK_BOX(LODControlHBox),cur_label,FALSE,TRUE,2,GTK_PACK_START);
	gtk_widget_show(cur_label);

	LODControlSlider = gtk_hscale_new_with_range(0,3,1); // limit to four steps for UI sanity
	gtk_scale_set_value_pos(GTK_SCALE(LODControlSlider),GTK_POS_LEFT);
	gtk_range_set_inverted( GTK_RANGE(LODControlSlider), TRUE);
	gtk_range_set_increments( GTK_RANGE(LODControlSlider), 1,1);
	if (gimpVTFOpt.LodControlU)
		gtk_range_set_value(GTK_RANGE(LODControlSlider),exponentU - gimpVTFOpt.LodControlU);	
	g_signal_connect(LODControlSlider, "format-value", G_CALLBACK(change_lod_control), NULL);
	gtk_container_add (GTK_CONTAINER (LODControlHBox), LODControlSlider);
	gtk_widget_show(LODControlSlider);

	/*
	// Vertical separator
	cur_separator = gtk_vseparator_new();
	gtk_container_add (GTK_CONTAINER (cur_hbox), cur_separator);
	gtk_box_set_child_packing(GTK_BOX(cur_hbox),cur_separator,FALSE,TRUE,0,GTK_PACK_START);
	gtk_widget_show(cur_separator);

	// Count the unhandled flags
	for (i=0; i < sizeof(gimpVTFOpt.GeneralFlags); i++)
	{
		guint32 flag = 1 << i;
		switch(flag)
		{
		case TEXTUREFLAGS_CLAMPS:
		case TEXTUREFLAGS_CLAMPT:
		case TEXTUREFLAGS_NOLOD:
		case TEXTUREFLAGS_NORMAL:
		case TEXTUREFLAGS_SSBUMP:
			continue;
		default:
			if (gimpVTFOpt.GeneralFlags & flag)
				num_general_flags++;
		}
	}
	snprintf(clearflags_label,32,"Clear %i other flags",num_general_flags);
	ClearOtherFlags = gtk_button_new_with_label(clearflags_label);	
	gtk_container_add (GTK_CONTAINER (cur_hbox), ClearOtherFlags);
	gtk_box_set_child_packing(GTK_BOX(cur_hbox),ClearOtherFlags,FALSE,TRUE,0,GTK_PACK_START);
	gtk_widget_show(ClearOtherFlags);*/
	
	cur_separator = gtk_hseparator_new();
	gtk_widget_show(cur_separator);
	gtk_container_add( GTK_CONTAINER(main_vbox), cur_separator );
	
	//----------------------------
	// Use layer as alpha channel
	//----------------------------
	
	cur_align = gtk_alignment_new(0,0.5,0,0);
	gtk_container_add (GTK_CONTAINER (main_vbox), cur_align);
	gtk_widget_show(cur_align);

	alpha_layer_label = gtk_label_new(_("#layer_an_alpha_label"));
	gtk_label_set_use_markup( GTK_LABEL(alpha_layer_label), TRUE);
	gtk_container_add (GTK_CONTAINER (cur_align), alpha_layer_label);
	gtk_widget_show(alpha_layer_label);
	gtk_widget_set_sensitive(alpha_layer_label, vtf_format_has_alpha(gimpVTFOpt.PixelFormat) );

	cur_hbox = gtk_hbox_new(FALSE,0);
	gtk_widget_set_tooltip_markup(cur_hbox,_("#layer_as_alpha_tip"));
	gtk_container_add (GTK_CONTAINER (main_vbox), cur_hbox);
	gtk_widget_show(cur_hbox);

	// Checkbox
	alpha_layer_chbx = gtk_check_button_new();
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alpha_layer_chbx), gimpVTFOpt.AlphaLayerTattoo);
	gtk_widget_show(alpha_layer_chbx);
	gtk_container_add (GTK_CONTAINER (cur_hbox), alpha_layer_chbx);
	gtk_box_set_child_packing(GTK_BOX(cur_hbox),alpha_layer_chbx,FALSE,TRUE,2,GTK_PACK_START);
	g_signal_connect (alpha_layer_chbx, "toggled", G_CALLBACK (toggle_alpha_layer), NULL);

	// Selection combo
	alpha_layer_select = gimp_drawable_combo_box_new(alpha_channel_suitable, (void*)&image_ID);
	gtk_widget_show(alpha_layer_select);
	gtk_container_add (GTK_CONTAINER (cur_hbox), alpha_layer_select);
	if (gimpVTFOpt.AlphaLayerTattoo)
		gimp_int_combo_box_set_active( GIMP_INT_COMBO_BOX(alpha_layer_select), gimp_image_get_layer_by_tattoo(image_ID,gimpVTFOpt.AlphaLayerTattoo) );		
	g_signal_connect (alpha_layer_select, "changed", G_CALLBACK (choose_alpha_layer), NULL);
	

	// Configure feature availability
	update_lod_availability();
	update_alpha_layer_availability();


	// Render window
	gtk_widget_show (dialog);

	run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

	gtk_widget_destroy (dialog);

	return run;
}