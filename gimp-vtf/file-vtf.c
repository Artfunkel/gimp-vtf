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

#ifdef _DEBUG
#include <libgimp/gimpui.h>
#endif

#define COPYRIGHT    "Tom Edwards (contact@steamreview.org)"
#define RELEASE_DATE "February 2012"

#define MAX_PATH 260

static void query();
static void run(const gchar* name, gint nparams, const GimpParam* param, gint* nreturn_vals, GimpParam** return_vals);

const GimpPlugInInfo PLUG_IN_INFO =
{
	NULL,  /* init_proc  */
	NULL,  /* quit_proc  */
	(GimpQueryProc)query,
	(GimpRunProc)run,
};

G_BEGIN_DECLS

MAIN()

static void query()
{
	static const GimpParamDef load_args[] =
	{
		{ GIMP_PDB_INT32,	"run-mode",		"Interactive, non-interactive" },
		{ GIMP_PDB_STRING,	"filename",		"The name of the file to load" },
		{ GIMP_PDB_STRING,	"raw-filename",	"The name of the file to load" }
	};
	static const GimpParamDef load_return_vals[] =
	{
		{ GIMP_PDB_IMAGE,   "image",         "Output image" }
	};

	static const GimpParamDef thumb_args[] =
	{
		{ GIMP_PDB_STRING, "filename",     "The name of the file to load"  },
		{ GIMP_PDB_INT32,  "thumb-size",   "Preferred thumbnail size"      }
	};
	static const GimpParamDef thumb_return_vals[] =
	{
		{ GIMP_PDB_IMAGE,  "image",        "Thumbnail image"               },
		{ GIMP_PDB_INT32,  "image-width",  "Width of full-sized image"     },
		{ GIMP_PDB_INT32,  "image-height", "Height of full-sized image"    }
	};

	static const GimpParamDef save_args[] =
	{
		{ GIMP_PDB_INT32,	"run-mode",		"Interactive, non-interactive" },
		{ GIMP_PDB_IMAGE,	"image",		"Input image" },
		{ GIMP_PDB_DRAWABLE,"drawable",		"Drawable to save" },
		{ GIMP_PDB_STRING,	"filename",		"The name of the file to save the image in" },
		{ GIMP_PDB_STRING,	"raw-filename",	"The name of the file to save the image in" },
		{ GIMP_PDB_INT8,	"compression",	"Pixel format to use" }, // lg-specific args start here
		{ GIMP_PDB_INT32,	"alpha-layer-tattoo",	"Tattoo of the layer to use as the alpha channel (0 for none)" },
		{ GIMP_PDB_INT8,	"layer-mode",	"How should layers be handled? 0 = merge, 1 = frames, 2 = faces, 3 = slices" },
		// new in 1.1
		{ GIMP_PDB_INT8,	"version",		"VTF minor version (7.n)" },
		{ GIMP_PDB_INT8,	"mips",			"Generate mipmaps?" },
		{ GIMP_PDB_INT8,	"nolod",		"Always load at full resolution?" },
		{ GIMP_PDB_INT8,	"clamp",		"Prevent texture repetition?" },
		{ GIMP_PDB_INT8,	"bump-type",	"0 = Not a bump map, 1 = Bump, 2 = SSBump" },
		{ GIMP_PDB_INT8,	"lod-control-u",	"Power of 2 which describes the width of the standard mipmap (0 = undefined)" },
		{ GIMP_PDB_INT8,	"lod-control-v",	"Power of 2 which describes the height of the standard mipmap (0 = undefined)" },
		// new in 1.2
		{ GIMP_PDB_LAYER,	"target-lg",	"The layer group being saved. (-1 = ignore layer groups)" },
	} ;

	// no effect
	//gboolean res = gimp_plugin_domain_register(TEXT_DOMAIN,MO_PATH);

	gimp_install_procedure (LOAD_PROC,
		"Loads Valve Texture Format files",
		"Loads Valve Texture Format files. Uses VTFLib, by Nem and Wunderboy.",
		COPYRIGHT,
		COPYRIGHT,
		RELEASE_DATE,
		"Valve Texture Format",
		NULL,
		GIMP_PLUGIN,
		G_N_ELEMENTS (load_args),
		G_N_ELEMENTS (load_return_vals),
		load_args, load_return_vals);

	gimp_register_file_handler_mime (LOAD_PROC, "image/x-vtf");
	gimp_register_magic_load_handler (LOAD_PROC, "vtf","","0,string,VTF\0");

	gimp_install_procedure (THUMB_PROC,
		"Loads thumbnails for Valve Texture Format files",
		"Loads a small mipmap (if there is one) from a VTF's frames/faces/slices. Uses VTFLib, by Nem and Wunderboy.",
		COPYRIGHT,
		COPYRIGHT,
		RELEASE_DATE,
		NULL,
		NULL,
		GIMP_PLUGIN,
		G_N_ELEMENTS (thumb_args),
		G_N_ELEMENTS (thumb_return_vals),
		thumb_args, thumb_return_vals);

  gimp_register_thumbnail_loader (LOAD_PROC, THUMB_PROC);

	gimp_install_procedure (SAVE_PROC,
	"Export to Valve Texture Format",
	"Writes out Valve Texture Format files with any compression method. Uses VTFLib, by Nem and Wunderboy.",
	COPYRIGHT,
	COPYRIGHT,
	RELEASE_DATE,
	"Valve Texture Format",
	"*",
	GIMP_PLUGIN,
	G_N_ELEMENTS (save_args), 0,
	save_args, 0);
	
	gimp_register_save_handler (SAVE_PROC, "vtf", "");
	gimp_register_file_handler_mime (SAVE_PROC, "image/x-vtf");
}

G_END_DECLS

void save(gint nparams, const GimpParam* param, gint* nreturn_vals);
void load(gint nparams, const GimpParam* param, gint* nreturn_vals, gboolean thumb);
void remove_dummy_lg();

#ifdef _WIN32
void install_locale(gboolean saving);
void register_filetype();
#endif

static void run(const gchar* name, gint nparams, const GimpParam* param, gint* nreturn_vals, GimpParam** return_vals)
{
	vlUInt vtf_bindcode = 0;
	image_ID = -1;
	
#ifdef _DEBUG
	{
		// Poor man's JIT debugger!
		GtkWidget* MessageBox;
	
		gimp_ui_init(PLUG_IN_BINARY, FALSE);

		MessageBox = gtk_message_dialog_new(NULL,(GtkDialogFlags)0,GTK_MESSAGE_INFO,GTK_BUTTONS_OK,NULL);
		gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(MessageBox),"<b><big>Attach debugger now</big></b>");
		gtk_window_set_position(GTK_WINDOW(MessageBox),GTK_WIN_POS_CENTER);

		gtk_widget_show(MessageBox);
		gtk_dialog_run( GTK_DIALOG(MessageBox) );
		gtk_widget_destroy (MessageBox);
	}
#endif
	
	plugin_dir = g_new(gchar,MAX_PATH);
	snprintf(plugin_dir,MAX_PATH,"%s\\plug-ins",gimp_directory());
	plugin_locale_dir = g_new(gchar,MAX_PATH);
	snprintf(plugin_locale_dir,MAX_PATH,"%s\\locale",plugin_dir);

#ifdef _WIN32
	install_locale(strcmp(name, SAVE_PROC) == 0);
	register_filetype();
#endif
	
	bindtextdomain( TEXT_DOMAIN, plugin_locale_dir );
	textdomain(TEXT_DOMAIN);

	// Fall back on English if needs be (thank god this is a separate process!)
	// You're supposed to embed the English text in the code so that a fallback isn't needed,
	// but for me half the point of localising was to offload long tooltip strings to a separate file.
	if ( _("#lod_control_label")[0] == '#' )
		g_setenv("LANGUAGE","en",TRUE);

	*nreturn_vals = 2;
	*return_vals  = vtf_ret_values;
	vtf_ret_values[0].type              = GIMP_PDB_STATUS;
	vtf_ret_values[0].data.d_status     = GIMP_PDB_EXECUTION_ERROR;


	if (vlInitialize() && vlCreateImage( &vtf_bindcode ) && vlBindImage(vtf_bindcode) )
	{
		if (strcmp (name, SAVE_PROC) == 0)
			save(nparams,param,nreturn_vals);
		else if (strcmp (name, LOAD_PROC) == 0)
			load(nparams,param,nreturn_vals,FALSE);
		else if (strcmp (name, THUMB_PROC) == 0)
			load(nparams,param,nreturn_vals,TRUE);
	}		
	else
		record_error((gchar*)vlGetLastError(),GIMP_PDB_EXECUTION_ERROR);
	
	vlDeleteImage(vtf_bindcode);
	vlShutdown();

	remove_dummy_lg();

	if (image_ID != -1)
		gimp_image_undo_thaw(image_ID);
}

void record_error(gchar* message, GimpPDBStatusType type)
{
	if ( vtf_ret_values[1].data.d_string )
		return;
	
	if ( !message || strlen(message) == 0)
		message = _("#unknown_error");

	vtf_ret_values[0].data.d_status = type;
	vtf_ret_values[1].type          = GIMP_PDB_STRING;
	vtf_ret_values[1].data.d_string = message;

	// Can't actually terminate the process or GIMP will think it has crashed!
}

void record_error_mem()
{
	record_error(_("#no_memory_error"),GIMP_PDB_EXECUTION_ERROR);
}

// These two functions copied over from VTFLib
vlBool IsPowerOfTwo(vlUInt uiSize)
{
	return uiSize > 0 && (uiSize & (uiSize - 1)) == 0;
}

vlUInt NextPowerOfTwo(vlUInt uiSize)
{
	vlUInt i;

	if(uiSize == 0)
	{
		return 1;
	}

	if(IsPowerOfTwo(uiSize))
	{
		return uiSize;
	}

	uiSize--;
	for(i = 1; i <= sizeof(vlUInt) * 4; i <<= 1)
	{
		uiSize = uiSize | (uiSize >> i);
	}
	uiSize++;

	return uiSize;
}
