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
#define TEXT_DOMAIN		"file-vtf"

GimpParam	vtf_ret_values[4];

gint32 image_ID;

vlBool IsPowerOfTwo(vlUInt uiSize);
vlUInt NextPowerOfTwo(vlUInt uiSize);

void quit_error(gchar* message);
void quit_error_mem();

typedef struct
{
	gchar*			label;
	VTFImageFormat	vlFormat;
	gchar*			alpha_label;
	gboolean		compressed;
} s_vtfFormat;

static const s_vtfFormat vtf_formats[] = {
	{ "DXT1",		IMAGE_FORMAT_DXT1,		"-", TRUE },
	{ "DXT1 with alpha", IMAGE_FORMAT_DXT1_ONEBITALPHA, "1-bit", TRUE },
	{ "DXT3",		IMAGE_FORMAT_DXT3,		"8-bit", TRUE },
	{ "DXT5",		IMAGE_FORMAT_DXT5,		"8-bit", TRUE },
	{ "RGB888",		IMAGE_FORMAT_RGB888,	"-", FALSE },
//	{ "RGB888 Bluescreen", IMAGE_FORMAT_RGB888_BLUESCREEN, "-", FALSE },
	{ "RGBA8888",	IMAGE_FORMAT_RGBA8888,	"8-bit", FALSE },
	{ "BGRA4444",	IMAGE_FORMAT_BGRA4444,	"4-bit", FALSE },
	{ "RGB565",		IMAGE_FORMAT_RGB565,	"-", FALSE },
	{ "I8",			IMAGE_FORMAT_I8,		"-", FALSE },
	{ "A8",			IMAGE_FORMAT_A8,		"8-bit", FALSE },
	{ "IA88",		IMAGE_FORMAT_IA88,		"8-bit", FALSE },
//	{ "P8",			IMAGE_FORMAT_P8,		"-", FALSE }, // Palleted. Unsupported by VTFLib.
//	{ "BGRX8888",	IMAGE_FORMAT_BGRX8888,	"-", FALSE },
//	{ "BGR565",		IMAGE_FORMAT_BGR565,	"-", FALSE },
//	{ "BGRX5551",	IMAGE_FORMAT_BGRX5551,	"-", FALSE },
//	{ "BGRA5551",	IMAGE_FORMAT_BGRA5551,	"8-bit", FALSE },
//	{ "UV88",		IMAGE_FORMAT_UV88,		"-", FALSE },
//	{ "UVWQ8888",	IMAGE_FORMAT_UVWQ8888,	"8-bit", FALSE },
	{ "RGBA16161616F",	IMAGE_FORMAT_RGBA16161616F,	"16-bit", FALSE },
	{ "RGBA16161616",	IMAGE_FORMAT_RGBA16161616,	"16-bit", FALSE },
//	{ "UVLX8888",	IMAGE_FORMAT_UVLX8888,	"8-bit", FALSE },
};

static const guint num_vtf_formats = sizeof(vtf_formats)/sizeof(s_vtfFormat);

static gboolean vtf_format_has_alpha(guint index)
{
	return vtf_formats[index].alpha_label[0] != '-';
}
static gboolean vtf_format_is_compressed(guint index)
{
	return index < 4;
}
static gboolean vtflib_format_has_alpha(VTFImageFormat format)
{
	guint i;
	for (i=0; i<num_vtf_formats; i++)
		if ( vtf_formats[i].vlFormat == format)
			return vtf_format_has_alpha(i);
	return FALSE;
}

// Save options

typedef enum
{
	VTF_MERGE_VISIBLE = 0,
	VTF_ANIMATION,
	VTF_ENVMAP,
	VTF_VOLUME
} VtfLayerUse;

typedef enum
{
	NOT_BUMP = 0,
	BUMP,
	SSBUMP
} VtfBumpType;

typedef struct
{
	guint8		Version; // 7.n
	gboolean	AdvancedSetup;

	// Simple
	gboolean	WithAlpha;
	gboolean	Compress;
	//Advanced
	guint8		PixelFormat;

	// Universal
	gboolean	Clamp;
	gboolean	NoLOD;
	gboolean	WithMips;
	VtfBumpType	BumpType;
	VtfLayerUse	LayerUse;
	gint32		AlphaLayerTattoo;

	guint32		GeneralFlags; // loaded from an existing VTF

	// Resources
	gchar	LodControlU;
	gchar	LodControlV;
} VtfSaveOptions;

static const gchar* get_vtf_options_ID(gint32 image_ID)
{
	static gchar settings_ID[64];
	snprintf(settings_ID,64,"%s_%i",SAVE_PROC,image_ID);
	return settings_ID;
}

#endif