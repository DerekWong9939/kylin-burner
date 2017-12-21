/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libburner-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libburner-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libburner-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libburner-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libburner-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libburner-media is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <errno.h>
#include <glib.h>

#include "burner-media.h"
#include "scsi-base.h"

#ifndef _BURN_UTILS_H
#define _BURN_UTILS_H

G_BEGIN_DECLS

#define BURNER_GET_BCD(data)		((((uchar)(data)&0xF0)>>4)*10+((uchar)(data)&0x0F))
#define BURNER_GET_16(data)		(((uchar)(data)[0]<<8)+(uchar)(data)[1])
#define BURNER_GET_24(data)		(((uchar)(data)[0]<<16)+((uchar)(data)[1]<<8)+((uchar)(data)[2]))
#define BURNER_GET_32(data)		(((uchar)(data)[0]<<24)+((uchar)(data)[1]<<16)+((uchar)(data)[2]<<8)+(uchar)(data)[3])

#define BURNER_SET_BCD(data, num)	(uchar)(data)=((((num)/10)<<4)&0xF0)|((num)-(((num)/10)*10))
#define BURNER_SET_16(data, num)	(data)[0]=(((num)>>8)&0xFF);(data)[1]=(uchar)((num)&0xFF)
#define BURNER_SET_24(data, num)	(data)[0]=(uchar)(((num)>>16)&0xFF);(data)[1]=(uchar)(((num)>>8)&0xFF);(data)[2]=(uchar)((num)&0xFF);
#define BURNER_SET_32(data, num)	(data)[0]=(uchar)(((num)>>24)&0xFF);(data)[1]=(uchar)(((num)>>16)&0xFF);(data)[2]=(uchar)(((num)>>8)&0xFF);(data)[3]=(uchar)((num)&0xFF)

#define BURNER_MSF_TO_LBA(minute, second, frame)	(((minute)*60+(second))*75+frame)

#define BURNER_IS_BCD_VALID(number)	((((uchar)(number) & 0x0f) < 0x09) && (((uchar)(number) & 0xF0) <= 0x90))

/**
 * Used to report errors and have some sort of debug output easily
 */

#define BURNER_SCSI_SET_ERRCODE(err, code)					\
{										\
	if (code == BURNER_SCSI_ERRNO)	 {					\
		int errsv = errno;						\
		BURNER_MEDIA_LOG ("SCSI command error: %s",			\
				   g_strerror (errsv));				\
	} else {								\
		BURNER_MEDIA_LOG ("SCSI command error: %s",			\
				   burner_scsi_strerror (code));		\
	}									\
	if (err)								\
		*(err) = code;							\
}

G_END_DECLS

#endif /* _BURN_UTILS_H */

 
