/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifndef Q3MAP_UNZIP_LAYOUT_H
#define Q3MAP_UNZIP_LAYOUT_H

#include <stdio.h>
#include <stddef.h>
#include "../../libs/zlib/zlib.h"

typedef struct tm_unz_s {
	unsigned int tm_sec;
	unsigned int tm_min;
	unsigned int tm_hour;
	unsigned int tm_mday;
	unsigned int tm_mon;
	unsigned int tm_year;
} tm_unz;

typedef struct unz_global_info_s {
	unsigned long number_entry;
	unsigned long size_comment;
} unz_global_info;

typedef struct unz_file_info_s {
	unsigned long version;
	unsigned long version_needed;
	unsigned long flag;
	unsigned long compression_method;
	unsigned long dosDate;
	unsigned long crc;
	unsigned long compressed_size;
	unsigned long uncompressed_size;
	unsigned long size_filename;
	unsigned long size_file_extra;
	unsigned long size_file_comment;
	unsigned long disk_num_start;
	unsigned long internal_fa;
	unsigned long external_fa;
	tm_unz tmu_date;
} unz_file_info;

typedef struct unz_file_info_internal_s {
	unsigned long offset_curfile;
} unz_file_info_internal;

typedef struct file_in_zip_read_info_s {
	char *read_buffer;
	z_stream stream;
	unsigned long pos_in_zipfile;
	unsigned long stream_initialised;
	unsigned long offset_local_extrafield;
	unsigned int size_local_extrafield;
	unsigned long pos_local_extrafield;
	unsigned long crc32;
	unsigned long crc32_wait;
	unsigned long rest_read_compressed;
	unsigned long rest_read_uncompressed;
	FILE *file;
	unsigned long compression_method;
	unsigned long byte_before_the_zipfile;
} file_in_zip_read_info_s;

typedef char unz_read_crc_offset_check[(offsetof(file_in_zip_read_info_s, crc32) == 80) ? 1 : -1];
typedef char unz_read_remaining_offset_check[(offsetof(file_in_zip_read_info_s, rest_read_uncompressed) == 92) ? 1 : -1];

struct unz_s {
	FILE *file;
	unz_global_info gi;
	unsigned long byte_before_the_zipfile;
	unsigned long num_file;
	unsigned long pos_in_central_dir;
	unsigned long current_file_ok;
	unsigned long central_pos;
	unsigned long size_central_dir;
	unsigned long offset_central_dir;
	unz_file_info cur_file_info;
	unz_file_info_internal cur_file_info_internal;
	struct file_in_zip_read_info_s *pfile_in_zip_read;
};

typedef char unz_file_info_size_check[(sizeof(unz_file_info) == 80) ? 1 : -1];
typedef char unz_state_size_check[(sizeof(struct unz_s) == 128) ? 1 : -1];
typedef char unz_file_info_offset_check[(offsetof(struct unz_s, cur_file_info) == 40) ? 1 : -1];
typedef char unz_length_offset_check[(offsetof(struct unz_s, cur_file_info) + offsetof(unz_file_info, uncompressed_size) == 68) ? 1 : -1];
typedef char unz_internal_info_offset_check[(offsetof(struct unz_s, cur_file_info_internal) == 120) ? 1 : -1];
typedef char unz_read_state_offset_check[(offsetof(struct unz_s, pfile_in_zip_read) == 124) ? 1 : -1];

#endif
