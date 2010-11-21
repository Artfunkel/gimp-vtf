/*
 * GIMP VTF
 * Copyright (C) 2010 Tom Edwards

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later
 * version.
 */

#ifndef FILE_VTF_H
#define FILE_VTF_H

#include "VTFLib.h"

#include <string.h>

#include "config.h.win32"
#include <libgimp/gimp.h>
#include <glib/gstdio.h>
#define HAVE_BIND_TEXTDOMAIN_CODESET
#include "libgimp/stdplugins-intl.h"

#define SAVE_PROC		"file-vtf-save"
#define LOAD_PROC		"file-vtf-load"
#define THUMB_PROC		"file-vtf-load-thumb"
#define PLUG_IN_BINARY	"file-vtf"

GimpParam	vtf_ret_values[4];

typedef struct
{
	gchar*			label;
	VTFImageFormat	vlFormat;
	gchar*			alpha_label;
} s_vtfFormat;

static s_vtfFormat vtf_formats[] = {
	{ "DXT1",		IMAGE_FORMAT_DXT1,		"-"	},
	{ "DXT5",		IMAGE_FORMAT_DXT5,		"8-bit"	},
	{ "RGB888",		IMAGE_FORMAT_RGB888,	"-"	},
	{ "RGBA8888",	IMAGE_FORMAT_RGBA8888,	"8-bit"	},
	{ "ABGR8888",	IMAGE_FORMAT_ABGR8888,	"8-bit"	},
	{ "BGR888",		IMAGE_FORMAT_BGR888,	"-"	},
	{ "RGB565",		IMAGE_FORMAT_RGB565,	"-"	},
	{ "I8",			IMAGE_FORMAT_I8,		"-"	},
	{ "IA88",		IMAGE_FORMAT_IA88,		"8-bit"	},
//	{ "P8",			IMAGE_FORMAT_P8,		"-"	},
	{ "A8",			IMAGE_FORMAT_A8,		"8-bit"	},
	{ "RGB888 Bluescreen", IMAGE_FORMAT_RGB888_BLUESCREEN, "-" },
	{ "BGR888 Bluescreen", IMAGE_FORMAT_BGR888_BLUESCREEN, "-" },
	{ "ARGB8888",	IMAGE_FORMAT_ARGB8888,	"8-bit"	},
	{ "BGRA8888",	IMAGE_FORMAT_BGRA8888,	"8-bit"	},
	{ "DXT3",		IMAGE_FORMAT_DXT3,		"8-bit"	},
	{ "BGRX8888",	IMAGE_FORMAT_BGRX8888,	"-"	},
	{ "BGR565",		IMAGE_FORMAT_BGR565,	"-"	},
	{ "BGRX5551",	IMAGE_FORMAT_BGRX5551,	"-"	},
	{ "BGRA4444",	IMAGE_FORMAT_BGRA4444,	"-"	},
	{ "DXT1n", IMAGE_FORMAT_DXT1_ONEBITALPHA, "1-bit" },
	{ "BGRA5551",	IMAGE_FORMAT_BGRA5551,	"8-bit"	},
	{ "UV88",		IMAGE_FORMAT_UV88,		"-"	},
	{ "UVWQ8888",	IMAGE_FORMAT_UVWQ8888,	"8-bit"	},
	{ "RGBA16161616F",	IMAGE_FORMAT_RGBA16161616F,	"8-bit" },
	{ "RGBA16161616",	IMAGE_FORMAT_RGBA16161616,	"8-bit" },
	{ "UVLX8888",	IMAGE_FORMAT_UVLX8888,	"8-bit"	}
};

static guint num_vtf_formats = sizeof(vtf_formats)/sizeof(s_vtfFormat);

static gboolean vtf_format_has_alpha(guint index)
{
	return vtf_formats[index].alpha_label[0] != '-';
}
static gboolean vtflib_format_has_alpha(VTFImageFormat format)
{
	guint i;
	for (i=0; i<num_vtf_formats; i++)
		if ( vtf_formats[i].vlFormat == format)
			return vtf_formats[i].alpha_label[0] != '-';
	return FALSE;
}

#endif