//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Zlib-ng extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Command implementations for the Rebol/Zlib-ng extension module.
//
// One function per command; the enum, the declarations, the dispatch table
// and the `_init` handler are all generated from zlib-ng.reb.
//

#pragma warning(disable : 4267)

#include "gen-zlib-ng.h"
#include <stdio.h>

static const REBYTE* ERR_INVALID_HANDLE = (const REBYTE*)"Invalid Zlib encoder or decoder handle!";
static const REBYTE* ERR_NO_STREAM      = (const REBYTE*)"Failed to create Zlib stream!";
static const REBYTE* ERR_NO_COMPRESS    = (const REBYTE*)"Failed to compress using the Zlib encoder!";
static const REBYTE* ERR_NO_DECOMPRESS  = (const REBYTE*)"Failed to decompress using the Zlib decoder!";


typedef enum {
	DEFLATE_MODE_DEFLATE,
	DEFLATE_MODE_ZLIB,
	DEFLATE_MODE_GZIP
} deflate_mode_t;


//== handle callbacks =========================================================

int ZlibNg_mold(REBHOB *hob, REBSER *str) {
	int len;
	if (!str) return 0;
	SERIES_TAIL(str) = 0;
	APPEND_STRING(str, "0#%lx", (unsigned long)(uintptr_t)hob->handle);
	return len;
}

int ZlibEncHandle_free(void *hndl) {
	if (!hndl) return 0;
	REBHOB *hob = (REBHOB*)hndl;
	zng_stream *stream = (zng_stream*)hob->data;
	zng_deflateEnd(stream);
	UNMARK_HOB(hob);
	return 0;
}

int ZlibDecHandle_free(void *hndl) {
	if (!hndl) return 0;
	REBHOB *hob = (REBHOB*)hndl;
	zng_stream *stream = (zng_stream*)hob->data;
	zng_inflateEnd(stream);
	UNMARK_HOB(hob);
	return 0;
}


//== memory ===================================================================

static void* ZlibNg_alloc(void *opaque, unsigned int num, unsigned int size) {
	return RL_MEM_ALLOC(opaque, num * size);
}
static void ZlibNg_free(void *opaque, void *address) {
	RL_MEM_FREE(opaque, address);
}


//== basic commands ===========================================================

COMMAND cmd_zlib_ng_version(RXIFRM *frm, void *ctx) {
	RXA_TYPE(frm, 1) = RXT_TUPLE;
	RXA_TUPLE(frm, 1)[0] = ZLIBNG_VER_MAJOR;
	RXA_TUPLE(frm, 1)[1] = ZLIBNG_VER_MINOR;
	RXA_TUPLE(frm, 1)[2] = ZLIBNG_VER_REVISION;
	RXA_TUPLE_LEN(frm, 1) = 3;
	return RXR_VALUE;
}


//== one-shot de/compression ==================================================

static int CompressCommon(deflate_mode_t mode, const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error) {
	zng_stream stream;
	stream.zalloc = &ZlibNg_alloc;
	stream.zfree  = &ZlibNg_free;
	stream.opaque = NULL;
	REBINT result;
	uLongf size;

	int windowBits = -15;
	if (mode == DEFLATE_MODE_GZIP) windowBits = 31;
	else if (mode == DEFLATE_MODE_ZLIB) windowBits = 15;

	if (level == NO_LIMIT) level = Z_DEFAULT_COMPRESSION;
	else if (level > Z_BEST_COMPRESSION)
		level = Z_BEST_COMPRESSION;

	result = zng_deflateInit2(&stream, level, Z_DEFLATED, windowBits, 8, Z_DEFAULT_STRATEGY);
	if (result != Z_OK) {
		zng_deflateEnd(&stream);
		*error = result;
		return FALSE;
	}

	size = 1 + zng_deflateBound(&stream, len); // one more byte for trailing null byte -> SET_STR_END

	stream.avail_in = len;
	stream.next_in = input;

	*output = RL_MAKE_BINARY((REBLEN)size);
	stream.avail_out = SERIES_AVAIL(*output);
	stream.next_out = BIN_HEAD(*output);

	for (;;) {
		result = zng_deflate(&stream, Z_FINISH);
		if (result == Z_STREAM_END)
			break; // Finished or we have enough data.
		if (result != Z_OK) {
			zng_deflateEnd(&stream);
			*error = result;
			return FALSE;
		}
		if (stream.avail_out == 0) {
			// expand output buffer...
			SERIES_TAIL(*output) = stream.total_out;
			RL_EXPAND_SERIES(*output, AT_TAIL, len);
			stream.next_out = BIN_SKIP(*output, stream.total_out);
			stream.avail_out = SERIES_REST(*output) - stream.total_out;
		}
	}
	SERIES_TAIL(*output) = stream.total_out;
	zng_deflateEnd(&stream);
	return TRUE;
}

int ZlibNg_CompressDeflate(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error) {
	return CompressCommon(DEFLATE_MODE_DEFLATE, input, len, level, output, error);
}
int ZlibNg_CompressGzip(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error) {
	return CompressCommon(DEFLATE_MODE_GZIP, input, len, level, output, error);
}
int ZlibNg_CompressZlib(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error) {
	return CompressCommon(DEFLATE_MODE_ZLIB, input, len, level, output, error);
}


static int DecompressCommon(deflate_mode_t mode, const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error) {
	uLongf size;
	REBINT result;

	size = (limit != NO_LIMIT) ? limit : (uLongf)len * 3;

	*output = RL_MAKE_BINARY(size);

	zng_stream stream;
	stream.zalloc = &ZlibNg_alloc;
	stream.zfree  = &ZlibNg_free;
	stream.opaque = NULL; // passed to zalloc and zfree, not needed currently
	stream.total_out = 0;

	stream.avail_in = len;
	stream.next_in = input;

	int windowBits = -15;
	if (mode == DEFLATE_MODE_GZIP) windowBits = 31;
	else if (mode == DEFLATE_MODE_ZLIB) windowBits = 15;

	result = zng_inflateInit2(&stream, windowBits);
	if (result != Z_OK) goto error;

	stream.avail_out = SERIES_AVAIL(*output);
	stream.next_out = BIN_HEAD(*output);

	for (;;) {
		result = zng_inflate(&stream, Z_NO_FLUSH);
		if (result == Z_STREAM_END || (limit && stream.total_out >= limit))
			break; // Finished or we have enough data.
		if (result != Z_OK) goto error;
		if (stream.avail_out == 0) {
			// expand output buffer...
			SERIES_TAIL(*output) = stream.total_out;
			RL_EXPAND_SERIES(*output, AT_TAIL, len);
			stream.next_out = BIN_SKIP(*output, stream.total_out);
			stream.avail_out = SERIES_REST(*output) - stream.total_out - 1;
		}
	}
	zng_inflateEnd(&stream);

	if (limit && stream.total_out > limit) {
		stream.total_out = limit;
	}

	SERIES_TAIL(*output) = stream.total_out;
	return TRUE;

error:
	*error = result;
	zng_inflateEnd(&stream);
	return FALSE;
}

int ZlibNg_DecompressDeflate(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error) {
	return DecompressCommon(DEFLATE_MODE_DEFLATE, input, len, limit, output, error);
}
int ZlibNg_DecompressGzip(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error) {
	return DecompressCommon(DEFLATE_MODE_GZIP, input, len, limit, output, error);
}
int ZlibNg_DecompressZlib(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error) {
	return DecompressCommon(DEFLATE_MODE_ZLIB, input, len, limit, output, error);
}

COMMAND cmd_zlib_ng_compress(RXIFRM *frm, void *ctx) {
	REBSER *data    = RXA_SERIES(frm, 1);
	REBINT index    = RXA_INDEX(frm, 1);
	REBFLG ref_part = RXA_REF(frm, 2);
	REBLEN length   = SERIES_TAIL(data) - index;
	REBINT level    = RXA_REF(frm, 4) ? RXA_INT32(frm, 5) : 0;
	REBFLG ref_zlib = RXA_REF(frm, 6);
	REBFLG ref_gzip = RXA_REF(frm, 7);
	REBSER *output  = NULL;
	REBINT  error   = 0;

	deflate_mode_t mode = DEFLATE_MODE_DEFLATE;
	if (ref_zlib)  mode = DEFLATE_MODE_ZLIB;
	if (ref_gzip)  mode = DEFLATE_MODE_GZIP;

	if (ref_part) length = (REBLEN)MAX(0, MIN(length, RXA_INT64(frm, 3)));

	if (!CompressCommon(mode, (const REBYTE*)BIN_SKIP(data, index), length, (REBCNT)level, &output, &error)) {
		RETURN_ERROR(ERR_NO_COMPRESS);
	}

	RXA_SERIES(frm, 1) = output;
	RXA_TYPE(frm, 1) = RXT_BINARY;
	RXA_INDEX(frm, 1) = 0;
	return RXR_VALUE;
}

COMMAND cmd_zlib_ng_decompress(RXIFRM *frm, void *ctx) {
	REBSER *data    = RXA_SERIES(frm, 1);
	REBINT index    = RXA_INDEX(frm, 1);
	REBFLG ref_part = RXA_REF(frm, 2);
	REBI64 length   = SERIES_TAIL(data) - index;
	REBI64 limit    = RXA_REF(frm, 4) ? RXA_INT64(frm, 5) : NO_LIMIT;
	REBFLG ref_zlib = RXA_REF(frm, 6);
	REBFLG ref_gzip = RXA_REF(frm, 7);
	REBSER *output  = NULL;
	REBINT  error   = 0;

	deflate_mode_t mode = DEFLATE_MODE_DEFLATE;
	if (ref_zlib)  mode = DEFLATE_MODE_ZLIB;
	if (ref_gzip)  mode = DEFLATE_MODE_GZIP;

	if (ref_part) length = MAX(0, MIN(length, RXA_INT64(frm, 3)));
	if (length < 0 || length > MAX_I32) {
		RETURN_ERROR(ERR_NO_DECOMPRESS);
	}

	if (!DecompressCommon(mode, (const uint8_t*)BIN_SKIP(data, index), (REBLEN)length, (REBCNT)limit, &output, &error)) {
		RETURN_ERROR(ERR_NO_DECOMPRESS);
	}

	RXA_SERIES(frm, 1) = output;
	RXA_TYPE(frm, 1) = RXT_BINARY;
	RXA_INDEX(frm, 1) = 0;
	return RXR_VALUE;
}


//== streaming ================================================================

// Expands the stream's output buffer, keeping all output produced so far.
// The hint is how much output is still expected; it is never trusted to be
// non-zero (there may be no input left, just pending output to flush).
static void grow_output(REBSER *buffer, zng_stream *stream, size_t hint) {
	SERIES_TAIL(buffer) = (REBLEN)stream->total_out;
	RL_EXPAND_SERIES(buffer, AT_TAIL, (REBCNT)MAX(hint, 4096));
	stream->next_out = BIN_SKIP(buffer, stream->total_out);
	stream->avail_out = SERIES_REST(buffer) - (REBLEN)stream->total_out;
}

static int make_stream_handle(RXIFRM *frm, void *ctx, REBCNT type, int windowBits, int strategy) {
	REBINT result;
	REBINT level = RXA_REF(frm, 1) ? RXA_INT32(frm, 2) : 6;

	level = MAX(0, MIN(9, level)); // 0 = no compression (stored), 9 = best

	REBHOB *hob = RL_MAKE_HANDLE_CONTEXT(type);
	if (hob == NULL) RETURN_ERROR(ERR_NO_STREAM);

	zng_stream *stream = (zng_stream*)hob->data;
	stream->zalloc = &ZlibNg_alloc;
	stream->zfree  = &ZlibNg_free;
	stream->opaque = NULL;

	if (type == Handle_ZlibEncoder) {
		result = zng_deflateInit2(stream, level, Z_DEFLATED, windowBits, 8, strategy);
	} else {
		result = zng_inflateInit2(stream, windowBits);
	}
	if (result != Z_OK) RETURN_ERROR(ERR_NO_STREAM);

	debug_print("stream: %p %u\n", (void*)stream, hob->sym);

	RETURN_HANDLE(hob);
}

COMMAND cmd_zlib_ng_make_encoder(RXIFRM *frm, void *ctx) {
	int windowBits = RXA_REF(frm, 3) ? RXA_INT32(frm, 4) : -15;
	int strategy   = RXA_REF(frm, 5) ? RXA_INT32(frm, 6) : Z_DEFAULT_STRATEGY;
	return make_stream_handle(frm, ctx, Handle_ZlibEncoder, windowBits, strategy);
}
COMMAND cmd_zlib_ng_make_zlib_encoder(RXIFRM *frm, void *ctx) {
	return make_stream_handle(frm, ctx, Handle_ZlibEncoder, 15, Z_DEFAULT_STRATEGY);
}
COMMAND cmd_zlib_ng_make_gzip_encoder(RXIFRM *frm, void *ctx) {
	return make_stream_handle(frm, ctx, Handle_ZlibEncoder, 31, Z_DEFAULT_STRATEGY);
}

COMMAND cmd_zlib_ng_make_decoder(RXIFRM *frm, void *ctx) {
	int windowBits = RXA_REF(frm, 3) ? RXA_INT32(frm, 4) : -15;
	return make_stream_handle(frm, ctx, Handle_ZlibDecoder, windowBits, 0);
}
COMMAND cmd_zlib_ng_make_zlib_decoder(RXIFRM *frm, void *ctx) {
	return make_stream_handle(frm, ctx, Handle_ZlibDecoder, 15, 0);
}
COMMAND cmd_zlib_ng_make_gzip_decoder(RXIFRM *frm, void *ctx) {
	return make_stream_handle(frm, ctx, Handle_ZlibDecoder, 31, 0);
}

COMMAND cmd_zlib_ng_write(RXIFRM *frm, void *ctx) {
	REBHOB *hob       = RXA_HANDLE(frm, 1);
	REBSER *data      = OPT_SERIES(2);
	REBINT index      = RXA_INDEX(frm, 2);
	REBFLG ref_flush  = RXA_REF(frm, 3);
	REBFLG ref_finish = RXA_REF(frm, 4);
	size_t size = 0;
	size_t available_in = 0;
	REBSER *buffer;
	REBINT result;

	if (hob->handle == NULL || !(hob->sym == Handle_ZlibEncoder || hob->sym == Handle_ZlibDecoder)) {
		RETURN_ERROR(ERR_INVALID_HANDLE);
	}

	zng_stream *stream = (zng_stream*)hob->data;
	buffer = hob->series;

	REBINT flush = ref_finish ? Z_FINISH : (ref_flush ? Z_SYNC_FLUSH : Z_NO_FLUSH);

	if (!data) {
		if (!buffer) return RXR_NONE;
		ref_finish = TRUE;
		flush = Z_FINISH;
		stream->avail_in = 0;
		stream->next_in = NULL;
	}
	else {
		available_in = SERIES_TAIL(data) - index;
		stream->avail_in = available_in;
		stream->next_in = BIN_SKIP(data, index);

		if (buffer == NULL) {
			size = (hob->sym == Handle_ZlibEncoder) ? zng_deflateBound(stream, available_in) : available_in;
			buffer = hob->series = RL_MAKE_BINARY((REBLEN)size);
		}
	}

	stream->avail_out = SERIES_REST(buffer) - SERIES_TAIL(buffer);
	stream->next_out = BIN_TAIL(buffer);
	debug_print("input total_in: %lu avail_in: %u avail_out: %u \n", (unsigned long)stream->total_in, stream->avail_in, stream->avail_out);

	if (hob->sym == Handle_ZlibEncoder) {
		for (;;) {
			result = zng_deflate(stream, flush);
			debug_print("deflate result: %i  stream.total_out: %zu avail_in %u .avail_out: %u\n", result, stream->total_out, stream->avail_in, stream->avail_out);
			// Z_BUF_ERROR is not fatal: no progress was possible (e.g. a
			// repeated flush with no new input). The output is never full
			// here, because the buffer is expanded before the next call.
			if (result == Z_BUF_ERROR) break;
			if (result < 0) {
				RETURN_ERROR(ERR_NO_COMPRESS);
			}
			if (stream->avail_out == 0) {
				grow_output(buffer, stream, zng_deflateBound(stream, stream->avail_in));
			}
			else if (result == Z_OK || result == Z_STREAM_END)
				break; // Finished or we have enough data.
		}
	}
	else {
		for (;;) {
			result = zng_inflate(stream, flush);
			debug_print("inflate result: %i  stream.total_out: %zu avail_in %u .avail_out: %u\n", result, stream->total_out, stream->avail_in, stream->avail_out);
			if (result == Z_STREAM_END) break;
			// Z_BUF_ERROR is not fatal: it means no progress was possible.
			// With the output full, the buffer is expanded below; otherwise
			// all the input is consumed and more must be written.
			if (result < 0 && !(result == Z_BUF_ERROR && stream->avail_out == 0)) {
				if (result == Z_BUF_ERROR) break;
				RETURN_ERROR(ERR_NO_DECOMPRESS);
			}
			if (stream->avail_out == 0) {
				grow_output(buffer, stream, available_in);
			}
			else break; // all available input consumed
		}
		// A finished stream must be complete; truncated input is an error.
		if (ref_finish && result != Z_STREAM_END) {
			// Reset, so the decoder can be reused after the error.
			SERIES_TAIL(buffer) = 0;
			stream->total_out = 0;
			zng_inflateReset(stream);
			RETURN_ERROR(ERR_NO_DECOMPRESS);
		}
	}
	SERIES_TAIL(buffer) = stream->total_out;

	debug_print("tail: %u\n", SERIES_TAIL(buffer));

	if (ref_flush || ref_finish) {
		// Make copy of the buffer (for safety reasons).
		size = SERIES_TAIL(buffer);
		REBSER *output = RL_MAKE_BINARY(size);
		COPY_MEM(BIN_HEAD(output), BIN_HEAD(buffer), size);
		SERIES_TAIL(output) = size;

		// Reset tail of the buffer, so it may be resused.
		SERIES_TAIL(buffer) = 0;
		stream->total_out = 0;

		if (ref_finish) {
			// Reset the stream so it can be reused.
			if (hob->sym == Handle_ZlibEncoder) zng_deflateReset(stream);
			else zng_inflateReset(stream);
		}

		// Return the new binary
		RXA_SERIES(frm, 1) = output;
		RXA_TYPE(frm, 1) = RXT_BINARY;
		RXA_INDEX(frm, 1) = 0;
	}
	return RXR_VALUE;
}

COMMAND cmd_zlib_ng_read(RXIFRM *frm, void *ctx) {
	REBINT result;
	REBLEN tail;
	REBHOB *hob = RXA_HANDLE(frm, 1);
	if (hob->data == NULL || (hob->sym != Handle_ZlibEncoder && hob->sym != Handle_ZlibDecoder)) {
		RETURN_ERROR(ERR_INVALID_HANDLE);
	}

	REBSER *buffer = hob->series;
	if (!buffer) return RXR_NONE;

	zng_stream *stream = (zng_stream*)hob->data;
	stream->avail_in = 0;
	stream->next_in = NULL;
	stream->avail_out = SERIES_REST(buffer) - SERIES_TAIL(buffer);
	stream->next_out = BIN_TAIL(buffer);
	if (hob->sym == Handle_ZlibEncoder) {
		// Read from a compression stream...
		for (;;) {
			result = zng_deflate(stream, Z_SYNC_FLUSH);
			debug_print("deflate result: %i  stream.total_out: %zu avail_in %u .avail_out: %u\n", result, stream->total_out, stream->avail_in, stream->avail_out);
			// Z_BUF_ERROR is not fatal: no progress was possible (e.g. a
			// repeated flush with no new input). The output is never full
			// here, because the buffer is expanded before the next call.
			if (result == Z_BUF_ERROR) break;
			if (result < 0) {
				RETURN_ERROR(ERR_NO_COMPRESS);
			}
			if (stream->avail_out == 0) {
				grow_output(buffer, stream, zng_deflateBound(stream, stream->avail_in));
			}
			else if (result == Z_OK || result == Z_STREAM_END)
				break; // Finished or we have enough data.
		}
		SERIES_TAIL(buffer) = stream->total_out;
	}
	// else: a decompression stream has its output already in the buffer.

	// Make copy of the buffer (for safety reasons).
	tail = SERIES_TAIL(buffer);
	REBSER *output = RL_MAKE_BINARY(tail);
	COPY_MEM(BIN_HEAD(output), BIN_HEAD(buffer), tail);
	SERIES_TAIL(output) = tail;

	// Reset tail of the buffer, so it may be resused.
	SERIES_TAIL(buffer) = 0;
	stream->total_out = 0;

	// Return the new binary
	RXA_SERIES(frm, 1) = output;
	RXA_TYPE(frm, 1) = RXT_BINARY;
	RXA_INDEX(frm, 1) = 0;
	return RXR_VALUE;
}
