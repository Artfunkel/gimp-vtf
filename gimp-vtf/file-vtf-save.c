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

gint		num_layers;

typedef enum
{
	VTF_MERGE_VISIBLE = 0,
	VTF_ANIMATION,
	VTF_ENVMAP,
	VTF_VOLUME
} VtfLayerUse;

typedef struct
{
	guint8		CompressionMode;
	VtfLayerUse	FrameMode;
	gint32		AlphaLayerTattoo;
	gboolean	DoMips;
} VtfSaveOptions;

VtfSaveOptions gimpVTFOpt = { 0, VTF_MERGE_VISIBLE, 0, TRUE };

gboolean show_options(gint32 image_ID);

void save(gint nparams, const GimpParam* param, gint* nreturn_vals)
{
	gint			width;
	gint			height;
	gchar*			filename;
	
	GimpRunMode		run_mode;
	gint32			image_ID;
	gint32			drawable_ID;
	gint32*			layer_IDs;
	
	gint32			alpha_layer_ID = 0;
	gboolean		alpha_was_visible;
	gint			alpha_layer_position;
	guint			frame=1,face=1,slice=1,dummy=0,layers_to_export=0;
	guint*			layer_iterator;

	gchar*			progress_frame_label;
	
	vlUInt				vtf_bindcode;
	SVTFCreateOptions	vlVTFOpt;
	vlByte**			rbgaImages;

	guint			i;
	
	// ------------
	run_mode = (GimpRunMode)param[0].data.d_int32;

	image_ID	= param[1].data.d_int32;
	drawable_ID	= param[2].data.d_int32;
	filename	= param[3].data.d_string;
	
	layer_IDs = gimp_image_get_layers(image_ID,&num_layers);
	
	switch (run_mode)
	{
	case GIMP_RUN_INTERACTIVE:
		gimp_get_data(SAVE_PROC, &gimpVTFOpt);
		if ( !show_options(image_ID) )
		{
			vtf_ret_values[0].data.d_status = GIMP_PDB_CANCEL;
			return;
		}
		break;

	case GIMP_RUN_NONINTERACTIVE:
		if (nparams == 6)
		{
			gimpVTFOpt.CompressionMode = param[6].data.d_int8;
			gimpVTFOpt.AlphaLayerTattoo = param[7].data.d_int32;
		}
		else
		{
			vtf_ret_values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
			return;
		}	
		break;

	case GIMP_RUN_WITH_LAST_VALS:
		gimp_get_data(SAVE_PROC, &gimpVTFOpt);
		break;
	}

	// Init VTFLib
	if ( !vlInitialize() )
		return;

	vlCreateImage( &vtf_bindcode );
	if ( !vlBindImage(vtf_bindcode) )
		return;

	// Set up creation options
	vlImageCreateDefaultCreateStructure(&vlVTFOpt);
	vlVTFOpt.ImageFormat = vtf_formats[gimpVTFOpt.CompressionMode].vlFormat;
	vlVTFOpt.bMipmaps = gimpVTFOpt.DoMips;

	// Start working on the image
	gimp_image_undo_freeze(image_ID);

	if (gimpVTFOpt.AlphaLayerTattoo)
	{
		alpha_layer_ID = gimp_image_get_layer_by_tattoo(image_ID,gimpVTFOpt.AlphaLayerTattoo);
		alpha_layer_position = gimp_image_get_layer_position(image_ID,alpha_layer_ID);
		gimp_image_lower_layer_to_bottom(image_ID,alpha_layer_ID);
		layer_IDs = gimp_image_get_layers(image_ID,&num_layers);
	}
	
	layers_to_export = num_layers;
	if (alpha_layer_ID)
		layers_to_export--;

	switch(gimpVTFOpt.FrameMode)
	{
	case VTF_MERGE_VISIBLE:
		if (alpha_layer_ID)
		{
			alpha_was_visible = gimp_drawable_get_visible(alpha_layer_ID);
			gimp_drawable_set_visible(alpha_layer_ID,FALSE);
		}
		
		drawable_ID = gimp_layer_new_from_visible(image_ID,image_ID,"VTF combined");
		
		if (alpha_layer_ID && alpha_was_visible)
			gimp_drawable_set_visible(alpha_layer_ID,TRUE);
		
		layers_to_export = 1; // Only want to export the one layer
		layer_iterator = &dummy;
		break;
	case VTF_ANIMATION:
		layer_iterator = &frame;
		progress_frame_label = "animation frame";
		break;
	case VTF_ENVMAP:
		layer_iterator = &face;
		progress_frame_label = "environment map face";
		break;
	case VTF_VOLUME:
		layer_iterator = &slice;
		progress_frame_label = "volumetric slice";
		break;
	}
	
	// Progress meter...unfortunately, VTFLib won't tell us its internal progress
	if (gimpVTFOpt.FrameMode == VTF_MERGE_VISIBLE)
		if (num_layers == 1)
			gimp_progress_init("Saving to VTF...");
		else
			gimp_progress_init("Saving combined layers to VTF...");
	else
		gimp_progress_init_printf("Saving %i VTF %ss...",layers_to_export,progress_frame_label);
	
	rbgaImages = g_new(vlByte*,num_layers);
		
#ifdef _DEBUG
	// Poor man's JIT debugger!
	while(1) {}
#endif

	// the layer iterator pointer increments either the frame, face or slice value (or a dummy in the case of VTF_MERGE_VISIBLE)
	for ( *layer_iterator = 0; *layer_iterator < (guint)layers_to_export; (*layer_iterator)++)
	{
		GimpPixelRgn	pixel_rgn;
		GimpDrawable*	drawable;
		vlByte*			rgbaBuf;

		if ( layer_IDs[*layer_iterator] == alpha_layer_ID )
			return;

		if (gimpVTFOpt.FrameMode != VTF_MERGE_VISIBLE)
			drawable_ID = gimp_layer_new_from_drawable(layer_IDs[*layer_iterator],image_ID);
		
		if (alpha_layer_ID)
		{
			gimp_image_add_layer(image_ID,drawable_ID,0);

			gimp_layer_add_mask( drawable_ID, gimp_layer_create_mask(alpha_layer_ID,GIMP_ADD_COPY_MASK) );
			gimp_layer_remove_mask( drawable_ID, GIMP_MASK_APPLY );
		}

		switch( gimp_drawable_type(drawable_ID) )
		{
		default:
			gimp_image_convert_rgb(drawable_ID);
		case GIMP_RGBA_IMAGE:
		case GIMP_RGB_IMAGE:
			break;
		}

		gimp_layer_add_alpha(drawable_ID); // always want to send VTFLib an alpha channel
	
		// Get drawable info
		drawable = gimp_drawable_get(drawable_ID);

		width	= drawable->width;
		height	= drawable->height;

		if (drawable->bpp != 4)
		{
			vtf_ret_values[1].type          = GIMP_PDB_STRING;
			vtf_ret_values[1].data.d_string = "Internal error: image was not 4bpp";
			return;
		}

		rgbaBuf = g_new(vlByte,width*height*4);

		// Get pixels
		gimp_pixel_rgn_init(&pixel_rgn, drawable, 0, 0, width, height, FALSE, FALSE);
		gimp_pixel_rgn_get_rect(&pixel_rgn, rgbaBuf, 0,0, width,height);

		// Assign to image pointer array...backwards thanks to GIMP
		rbgaImages[(guint)layers_to_export - *layer_iterator - 1] = rgbaBuf;
		gimp_drawable_detach(drawable);		

		if (alpha_layer_ID)
			gimp_image_remove_layer(image_ID,drawable_ID);
	}

	if (alpha_layer_ID)
	{
		while (gimp_image_get_layer_position(image_ID,alpha_layer_ID) != alpha_layer_position)
		{
			// no "set layer position" call, doh!
			gimp_image_raise_layer(image_ID,alpha_layer_ID);
		}
		gimp_displays_flush();
	}

	gimp_progress_update(0.5); // crap, but nothing better to do
	
	// Hand off to VTFLib
	if ( vlImageCreateMultiple(width,height,frame,face,slice,rbgaImages,&vlVTFOpt) && vlImageSave(filename) )
		vtf_ret_values[0].data.d_status = GIMP_PDB_SUCCESS;
	
	// Failed!
	if ( vtf_ret_values[0].data.d_status != GIMP_PDB_SUCCESS )
	{
      vtf_ret_values[1].type          = GIMP_PDB_STRING;
      vtf_ret_values[1].data.d_string = (gchar*)vlGetLastError();
	}

	// Save settings
	if (run_mode == GIMP_RUN_INTERACTIVE)
		gimp_set_data (SAVE_PROC, &gimpVTFOpt, sizeof (VtfSaveOptions));

	// Free memory
	for(i=0; i < (guint)layers_to_export; i++)
		g_free(rbgaImages[i]);
	g_free(rbgaImages);
	vlDeleteImage(vtf_bindcode);
	
	// loose UI ends
	gimp_progress_end();
	gimp_image_undo_thaw(image_ID);
}

/*
 * UI
 */

GtkWidget* alpha_layer_select;
GtkWidget* alpha_layer_chbx;

static void select_compression(GtkWidget *widget, gint row, gint column, GdkEventButton *event, gpointer user_data) 
{
	gimpVTFOpt.CompressionMode = row;

	gtk_widget_set_sensitive(alpha_layer_chbx, vtf_format_has_alpha(row) );
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(alpha_layer_chbx) ) )
		gtk_widget_set_sensitive(alpha_layer_select, vtf_format_has_alpha(row) );
}

void set_alpha_layer()
{
	gint alpha_id;
	gimp_int_combo_box_get_active(GIMP_INT_COMBO_BOX(alpha_layer_select), &alpha_id);
	gimpVTFOpt.AlphaLayerTattoo = gimp_drawable_get_tattoo(alpha_id);
}

static void toggle_alpha_layer(GtkToggleButton *togglebutton, gpointer user_data)
{		
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(togglebutton) ) )
	{
		gint alpha_id;
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
}

static void choose_alpha_layer(GtkComboBox* combo, gpointer user_data)
{
	set_alpha_layer();
}

static void change_frame_use(GtkWidget* combo, gpointer user_data)
{
	gimpVTFOpt.FrameMode = (VtfLayerUse)gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static gboolean alpha_channel_suitable(gint32 image_id, gint32 drawable_id, gpointer user_data)
{
	return num_layers > 1 && image_id == *(gint32*)user_data;
	/*return ( !gimp_drawable_has_alpha(drawable_id)
		&& gimp_drawable_height(drawable_id) == gimp_image_height(image_id)
		&& gimp_drawable_width(drawable_id) == gimp_image_width(image_id) );*/
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
static gboolean show_options(gint32 image_ID)
{
	GtkWidget*	dialog;
	GtkWidget*	main_vbox;
	GtkWidget*	alignment;
	
	//GtkWidget*	scrollbox;
	GtkWidget*	comp_list_widget;
	GtkCList*	comp_list;

	GtkWidget*	DoMips;

	GtkWidget*	LayerUseCombo;
	GtkWidget*	LayerUseLabel;
	GtkWidget*	LayerUseHBox;
	
	guint		i;
	gboolean	run;


	gimp_ui_init(PLUG_IN_BINARY, FALSE);
	
	dialog = gimp_dialog_new ("VTF save options", PLUG_IN_BINARY, NULL, (GtkDialogFlags)0, HELP_FUNC, "file-vtf",

	GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	GTK_STOCK_OK,     GTK_RESPONSE_OK,

	NULL);

	// Padding
	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_widget_show (alignment);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), alignment);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 6, 6, 6);

	// Vertical align
	main_vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (alignment), main_vbox);
	gtk_widget_show (main_vbox);

	// List scrollbox
	/*scrollbox = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollbox), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_widget_set_size_request(scrollbox,-1,250);

	gtk_container_add (GTK_CONTAINER (main_vbox), scrollbox);
	gtk_widget_show (scrollbox);*/
		
	// List of compression formats
	comp_list_widget = gtk_clist_new(2);
	comp_list = (GtkCList*)comp_list_widget;
	
	gtk_clist_set_selection_mode(comp_list,GTK_SELECTION_SINGLE);
	gtk_clist_set_shadow_type(comp_list,GTK_SHADOW_ETCHED_IN);
	
	gtk_clist_set_column_title(comp_list,0,"Alpha");
	gtk_clist_set_column_title(comp_list,1,"Pixel format");
	gtk_clist_column_titles_passive(comp_list);
	gtk_clist_column_titles_show(comp_list);

	gtk_clist_set_column_justification(comp_list,0,GTK_JUSTIFY_CENTER);

	for (i=0;i<num_vtf_formats;i++)
	{
		gchar* cur_item[] = { vtf_formats[i].alpha_label, vtf_formats[i].label };
		gtk_clist_append(comp_list,cur_item);
	}

	gtk_signal_connect(GTK_OBJECT(comp_list_widget), "select_row", GTK_SIGNAL_FUNC(select_compression), NULL);

	// Select last used setting
	gtk_clist_select_row( comp_list, gimpVTFOpt.CompressionMode, 0 );
	comp_list->focus_row = gimpVTFOpt.CompressionMode;

	gtk_widget_show(comp_list_widget);
	gtk_container_add( GTK_CONTAINER(main_vbox), comp_list_widget );
	
	// Layer use
	LayerUseHBox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show(LayerUseHBox);
	gtk_container_add( GTK_CONTAINER(main_vbox), LayerUseHBox );

	LayerUseLabel = gtk_label_new_with_mnemonic("Use _layers as:");
	gtk_widget_show(LayerUseLabel);
	gtk_box_pack_start (GTK_BOX (LayerUseHBox), LayerUseLabel, FALSE, FALSE, 0);

	LayerUseCombo = gimp_int_combo_box_new (
		_("Nothing (merge visible)"), VTF_MERGE_VISIBLE,
		_("Animation frames"),    VTF_ANIMATION,
		_("Environment map faces"), VTF_ENVMAP,
		_("Volumetrix texture slices"), VTF_VOLUME,
		NULL);

	g_signal_connect (LayerUseCombo, "changed", G_CALLBACK (change_frame_use), NULL);
	gtk_widget_set_sensitive(LayerUseCombo, num_layers > 1 );

	gtk_combo_box_set_active( GTK_COMBO_BOX(LayerUseCombo),gimpVTFOpt.FrameMode);
	gtk_widget_show(LayerUseCombo);
	gtk_container_add( GTK_CONTAINER(LayerUseHBox), LayerUseCombo );

	// Mips?
	DoMips = gtk_check_button_new_with_mnemonic (_("Generate _Mipmaps"));
	g_signal_connect (DoMips, "toggled", G_CALLBACK (gimp_toggle_button_update), &gimpVTFOpt.DoMips);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (DoMips), gimpVTFOpt.DoMips);
	gtk_widget_show(DoMips);
	gtk_container_add( GTK_CONTAINER(main_vbox), DoMips );

	// LOD? Has no effect due to VTFLib bug
	/*NoLOD = gtk_check_button_new_with_mnemonic (_("Disable _LOD"));;
	g_signal_connect (NoLOD, "toggled", G_CALLBACK (gimp_toggle_button_update), &gimpVTFOpt.NoLOD);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (NoLOD), gimpVTFOpt.NoLOD);
	gtk_widget_show(NoLOD);
	gtk_container_add( GTK_CONTAINER(main_vbox), NoLOD );*/


	// Use layer as alpha channel
	alpha_layer_chbx = gtk_check_button_new_with_mnemonic (_("Use layer as _alpha channel:"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alpha_layer_chbx), gimpVTFOpt.AlphaLayerTattoo);
	gtk_widget_set_sensitive(alpha_layer_chbx, vtf_format_has_alpha(gimpVTFOpt.CompressionMode) );
	gtk_misc_set_alignment( GTK_MISC(alpha_layer_chbx),0,0.5);
	gtk_widget_show(alpha_layer_chbx);
	gtk_container_add (GTK_CONTAINER (main_vbox), alpha_layer_chbx);

	// Layer-as-alpha selection
	alpha_layer_select = gimp_drawable_combo_box_new(alpha_channel_suitable, &image_ID);
	gtk_widget_set_sensitive(alpha_layer_select,gimpVTFOpt.AlphaLayerTattoo);
	gtk_widget_show(alpha_layer_select);
	gtk_container_add (GTK_CONTAINER (main_vbox), alpha_layer_select);

	if (gimpVTFOpt.AlphaLayerTattoo)
		gimp_int_combo_box_set_active( GIMP_INT_COMBO_BOX(alpha_layer_select), gimp_image_get_layer_by_tattoo(image_ID,gimpVTFOpt.AlphaLayerTattoo) );
		
	g_signal_connect (alpha_layer_chbx, "toggled", G_CALLBACK (toggle_alpha_layer), NULL);
	g_signal_connect (alpha_layer_select, "changed", G_CALLBACK (choose_alpha_layer), NULL);
	
	// Render window
	gtk_widget_show (dialog);

	run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

	gtk_widget_destroy (dialog);

	return run;
}