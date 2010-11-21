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

#include <libgimp/gimpexport.h>

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
		{ GIMP_PDB_INT8,	"compression",	"Compression method to use" },
		{ GIMP_PDB_INT32,	"alpha-layer-tattoo",	"Tattoo of the layer to use as the alpha channel (0 for none)" },
	} ;

	gimp_install_procedure (LOAD_PROC,
		N_("Loads Valve Texture Format files"),
		"Reads Valve Texture Format files. Uses VTFLib, by Nem and Wunderboy.",
		"Tom Edwards (contact@steamreview.org)",
		"Tom Edwards (contact@steamreview.org)",
		"2010",
		N_("Valve Texture Format"),
		NULL,
		GIMP_PLUGIN,
		G_N_ELEMENTS (load_args),
		G_N_ELEMENTS (load_return_vals),
		load_args, load_return_vals);

	gimp_register_file_handler_mime (LOAD_PROC, "image/x-vtf");
	gimp_register_magic_load_handler (LOAD_PROC, "vtf","","0,string,VTF\0");

	gimp_install_procedure (THUMB_PROC,
		N_("Loads thumbnails for Valve Texture Format files"),
		"Loads a low mipmap from halfway through the VTF's frames/faces/slices. Uses VTFLib, by Nem and Wunderboy.",
		"Tom Edwards (contact@steamreview.org)",
		"Tom Edwards (contact@steamreview.org)",
		"2010",
		NULL,
		NULL,
		GIMP_PLUGIN,
		G_N_ELEMENTS (thumb_args),
		G_N_ELEMENTS (thumb_return_vals),
		thumb_args, thumb_return_vals);

  gimp_register_thumbnail_loader (LOAD_PROC, THUMB_PROC);

	gimp_install_procedure (SAVE_PROC,
	N_("Export to Valve Texture Format"),
	"Writes out Valve Texture Format files with any compression method. Uses VTFLib, by Nem and Wunderboy.",
	"Tom Edwards (contact@steamreview.org)",
	"Tom Edwards (contact@steamreview.org)",
	"2010",
	N_("Valve Texture Format"),
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

static void run(const gchar* name, gint nparams, const GimpParam* param, gint* nreturn_vals, GimpParam** return_vals)
{
	INIT_I18N ();
	
	*nreturn_vals = 1;
	*return_vals  = vtf_ret_values;
	vtf_ret_values[0].type              = GIMP_PDB_STATUS;
	vtf_ret_values[0].data.d_status     = GIMP_PDB_EXECUTION_ERROR;
	
	if (strcmp (name, SAVE_PROC) == 0)
		save(nparams,param,nreturn_vals);
	else if (strcmp (name, LOAD_PROC) == 0)
		load(nparams,param,nreturn_vals,FALSE);
	else if (strcmp (name, THUMB_PROC) == 0)
		load(nparams,param,nreturn_vals,TRUE);
}
