//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// SPDX-License-Identifier: MIT
// =============================================================================
// Rebol/Zlib-ng extension
// =============================================================================

#pragma warning(disable : 4267)

#include "zlib-rebol-extension.h"
#include <stdio.h>

#define COMMAND int

#define OPT_SERIES(n)         (RXA_TYPE(frm,n) == RXT_NONE ? NULL : RXA_SERIES(frm, n))

#define RETURN_HANDLE(hob)                   \
	RXA_HANDLE(frm, 1)       = hob;          \
	RXA_HANDLE_TYPE(frm, 1)  = hob->sym;     \
	RXA_HANDLE_FLAGS(frm, 1) = hob->flags;   \
	RXA_TYPE(frm, 1) = RXT_HANDLE;           \
	return RXR_VALUE

#define APPEND_STRING(str, ...) \
	len = snprintf(NULL,0,__VA_ARGS__);\
	if (len > SERIES_REST(str)-SERIES_LEN(str)) {\
		RL_EXPAND_SERIES(str, SERIES_TAIL(str), len);\
		SERIES_TAIL(str) -= len;\
	}\
	len = snprintf( \
		SERIES_TEXT(str)+SERIES_TAIL(str),\
		SERIES_REST(str)-SERIES_TAIL(str),\
		__VA_ARGS__\
	);\
	SERIES_TAIL(str) += len;

#define RETURN_ERROR(err)  do {RXA_SERIES(frm, 1) = (void*)err; return RXR_ERROR;} while(0)

static const REBYTE* ERR_INVALID_HANDLE = (const REBYTE*)"Invalid Zlib encoder or decoder handle!";
static const REBYTE* ERR_NO_STREAM      = (const REBYTE*)"Failed to create Zlib stream!";
static const REBYTE* ERR_NO_COMPRESS    = (const REBYTE*)"Failed to compress using the Zlib encoder!";
static const REBYTE* ERR_NO_DECOMPRESS  = (const REBYTE*)"Failed to decompress using the Zlib decoder!";
static const REBYTE* ERR_ENCODER_FINISHED = (const REBYTE*)"Zlib encoder is finished!";


typedef enum {
    DEFLATE_MODE_DEFLATE,
    DEFLATE_MODE_ZLIB,
    DEFLATE_MODE_GZIP
} deflate_mode_t;

int Common_mold(REBHOB *hob, REBSER *str) {
	size_t len;
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
int ZlibDecHandle_free(void* hndl) {
	if (!hndl) return 0;
	REBHOB *hob = (REBHOB*)hndl;
	zng_stream *stream = (zng_stream*)hob->data;
	zng_inflateEnd(stream);
	UNMARK_HOB(hob);
	return 0;
}
int ZlibEncHandle_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
//	ZlibEncoderState *encoder = (ZlibEncoderState*)hob->handle;
//	word = RL_FIND_WORD(arg_words, word);
//	switch (word) {
//	case W_ARG_FINISHED:
//		*type = RXT_LOGIC;
//		arg->int32a = ZlibEncoderIsFinished(encoder)?1:0;
//		break;
//	default:
//		return PE_BAD_SELECT;	
//	}
	return PE_USE;
}
int ZlibEncHandle_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
//	ZlibEncoderState *encoder = (ZlibEncoderState*)hob->handle;
//	word = RL_FIND_WORD(arg_words, word);
//	switch (word) {
//	case W_ARG_MODE:
//		switch (*type) {
//		case RXT_INTEGER:
//			ZlibEncoderSetParameter(encoder, ZLIB_PARAM_MODE, (uint32_t)arg->uint64);
//			break;
//		default: 
//			return PE_BAD_SET_TYPE;
//		}
//		break;
//	case W_ARG_SIZE_HINT:
//		switch (*type) {
//		case RXT_INTEGER:
//			ZlibEncoderSetParameter(encoder, ZLIB_PARAM_SIZE_HINT, (uint32_t)arg->uint64);
//			break;
//		default: 
//			return PE_BAD_SET_TYPE;
//		}
//		break;
//	default:
//		return PE_BAD_SET;	
//	}
	return PE_OK;
}

int ZlibDecHandle_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
//	ZlibDecoderState *decoder = (ZlibDecoderState*)hob->handle;
//	word = RL_FIND_WORD(arg_words, word);
//	switch (word) {
//	case W_ARG_FINISHED:
//		*type = RXT_LOGIC;
//		arg->int32a = ZlibDecoderIsFinished(decoder)?1:0;
//		break;
//	default:
//		return PE_BAD_SELECT;	
//	}
	return PE_USE;
}


void* ZlibDefaultAllocFunc(void *opaque, unsigned int num, unsigned int size) {
	//debug_print("alloc: %lu\n", num*size);
	return RL_MEM_ALLOC(opaque, num * size);
}
void ZlibDefaultFreeFunc(void *opaque, void *address) {
	RL_MEM_FREE(opaque, address);
}


static void expand_and_copy(REBSER *buffer, const uint8_t *data, REBLEN size) {
	debug_print("out_size: %lu ", size);
	if (size > 0 && data != NULL) {
		REBLEN tail = SERIES_TAIL(buffer);
		RL_EXPAND_SERIES(buffer, tail, size);
		// Reset tail after expand
		SERIES_TAIL(buffer) = tail;
		// Copy encoded data to the output buffer.
		COPY_MEM(BIN_TAIL(buffer), data, size);
		SERIES_TAIL(buffer) += size;
	}
}


COMMAND cmd_init_words(RXIFRM *frm, void *ctx) {
	arg_words  = RL_MAP_WORDS(RXA_SERIES(frm,1));
	type_words = RL_MAP_WORDS(RXA_SERIES(frm,2));

	// custom initialization may be done here...

	return RXR_TRUE;
}

COMMAND cmd_version(RXIFRM *frm, void *ctx) {
	RXA_TYPE(frm, 1) = RXT_TUPLE;
	RXA_TUPLE(frm, 1)[0] = ZLIBNG_VER_MAJOR;
	RXA_TUPLE(frm, 1)[1] = ZLIBNG_VER_MINOR;
	RXA_TUPLE(frm, 1)[2] = ZLIBNG_VER_REVISION;
	RXA_TUPLE_LEN(frm, 1) = 3;

	return RXR_VALUE;
}


int CompressCommon(deflate_mode_t mode, const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error) {
	zng_stream stream;
	stream.zalloc = &ZlibDefaultAllocFunc;
	stream.zfree = &ZlibDefaultFreeFunc;
	stream.opaque = NULL;
	REBINT result;
	uLongf size;


	int windowBits = -15;
	if (mode == DEFLATE_MODE_GZIP) windowBits = 31;
	else if (mode == DEFLATE_MODE_ZLIB) windowBits = 15;

	//TODO: Use Handle for the state!!!!

	if (level == NO_LIMIT) level = Z_DEFAULT_COMPRESSION;
	else if(level > Z_BEST_COMPRESSION)
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
		//printf("deflate result: %i  stream.total_out: %i .avail_out: %i\n", result, stream.total_out, stream.avail_out);
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

int CompressDeflate(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error) {
	return CompressCommon(DEFLATE_MODE_DEFLATE, input, len, level, output, error);
}
int CompressGzip(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error) {
	return CompressCommon(DEFLATE_MODE_GZIP, input, len, level, output, error);
}
int CompressZlib(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error) {
	return CompressCommon(DEFLATE_MODE_ZLIB, input, len, level, output, error);
}


int DecompressCommon(deflate_mode_t mode, const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error) {
	uLongf size;
	REBINT result;

	size = (limit != NO_LIMIT) ? limit : (uLongf)len * 3;

	*output = RL_MAKE_BINARY(size);

	zng_stream stream;
	stream.zalloc = &ZlibDefaultAllocFunc; // fail() cleans up automatically, see notes
	stream.zfree = &ZlibDefaultFreeFunc;
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

	for(;;) {
		result = zng_inflate(&stream, Z_NO_FLUSH);
		if (result == Z_STREAM_END || (limit && stream.total_out >= limit))
			break; // Finished or we have enough data.
		//printf("result: %i size: %i avail_out: %i total_out: %i\n", result, size, stream.avail_out, stream.total_out);
		if (result != Z_OK) goto error;
		if (stream.avail_out == 0) {
			// expand output buffer...
			SERIES_TAIL(*output) = stream.total_out;
			RL_EXPAND_SERIES(*output, AT_TAIL, len);
			stream.next_out = BIN_SKIP(*output, stream.total_out);
			stream.avail_out = SERIES_REST(*output) - stream.total_out - 1;
		}
	}
	//printf("total_out: %i\n", stream.total_out);
	zng_inflateEnd(&stream);

	if (limit && stream.total_out > limit) {
		stream.total_out = limit;
	}

	SERIES_TAIL(*output) = stream.total_out;

	//if (SERIES_AVAIL(output) > 4096) // Is there wasted space?
	//	output = Copy_Series(output); // Trim it down if too big. !!! Revisit this based on mem alloc alg.

	return TRUE;

error:
	*error = result;
	zng_inflateEnd(&stream);
	return FALSE;
}

int DecompressDeflate(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error) {
	return DecompressCommon(DEFLATE_MODE_DEFLATE, input, len, limit, output, error);
}
int DecompressGzip(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error) {
	return DecompressCommon(DEFLATE_MODE_GZIP, input, len, limit, output, error);
}
int DecompressZlib(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error) {
	return DecompressCommon(DEFLATE_MODE_ZLIB, input, len, limit, output, error);
}

COMMAND cmd_compress(RXIFRM *frm, void *ctx) {
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


COMMAND cmd_decompress(RXIFRM *frm, void *ctx) {
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


static make_stream_handle(RXIFRM *frm, void *ctx, REBCNT type, int windowBits, int strategy) {
	REBINT result;
	REBINT level = RXA_REF(frm, 1) ? RXA_INT32(frm, 2) : 6;

	level = MIN(1, MAX(9, level));

	REBHOB *hob = RL_MAKE_HANDLE_CONTEXT(type);
	if (hob == NULL) RETURN_ERROR(ERR_NO_STREAM);

	zng_stream *stream = (zng_stream*)hob->data;
	stream->zalloc = &ZlibDefaultAllocFunc;
	stream->zfree  = &ZlibDefaultFreeFunc;
	stream->opaque = NULL;

	if (type == Handle_ZlibEncoder) {
		result = zng_deflateInit2(stream, level, Z_DEFLATED, windowBits, 8, strategy);
	} else {
		result = zng_inflateInit2(stream, windowBits);
	}
	if (result != Z_OK) RETURN_ERROR(ERR_NO_STREAM);

	debug_print("stream: %p %u\n",stream, hob->sym);

	RXA_HANDLE(frm, 1) = hob;
	RXA_HANDLE_TYPE(frm, 1) = hob->sym;
	RXA_HANDLE_FLAGS(frm, 1) = hob->flags;
	RXA_TYPE(frm, 1) = RXT_HANDLE;
	return RXR_VALUE;
}

COMMAND cmd_make_encoder(RXIFRM *frm, void *ctx) {
	int windowBits = RXA_REF(frm, 3) ? RXA_INT32(frm, 4) : -15;
	int strategy   = RXA_REF(frm, 5) ? RXA_INT32(frm, 6) : Z_DEFAULT_STRATEGY;
	return make_stream_handle(frm, ctx, Handle_ZlibEncoder, windowBits, strategy);
}
COMMAND cmd_make_zlib_encoder(RXIFRM *frm, void *ctx) {
	return make_stream_handle(frm, ctx, Handle_ZlibEncoder, 15, Z_DEFAULT_STRATEGY);
}
COMMAND cmd_make_gzip_encoder(RXIFRM *frm, void *ctx) {
	return make_stream_handle(frm, ctx, Handle_ZlibEncoder, 31, Z_DEFAULT_STRATEGY);
}

COMMAND cmd_make_decoder(RXIFRM *frm, void *ctx) {
	int windowBits = RXA_REF(frm, 3) ? RXA_INT32(frm, 4) : -15;
	return make_stream_handle(frm, ctx, Handle_ZlibDecoder, windowBits, 0);
}
COMMAND cmd_make_zlib_decoder(RXIFRM *frm, void *ctx) {
	return make_stream_handle(frm, ctx, Handle_ZlibDecoder, 15, 0);
}
COMMAND cmd_make_gzip_decoder(RXIFRM *frm, void *ctx) {
	return make_stream_handle(frm, ctx, Handle_ZlibDecoder, 31, 0);
}

COMMAND cmd_write(RXIFRM *frm, void *ctx) {
	REBHOB *hob       = RXA_HANDLE(frm, 1);
	REBSER *data      = OPT_SERIES(2);
	REBINT index      = RXA_INDEX(frm, 2);
	REBFLG ref_flush  = RXA_REF(frm, 3);
	REBFLG ref_finish = RXA_REF(frm, 4);
	REBYTE *inp = NULL;
	REBYTE *out = NULL;
	size_t size = 0;
	size_t available_in = 0;
	REBSER *buffer;
	REBINT result;

	if (hob->handle == NULL || !(hob->sym == Handle_ZlibEncoder || hob->sym == Handle_ZlibDecoder)) {
		RETURN_ERROR(ERR_INVALID_HANDLE);
	}

	zng_stream *stream = (zng_stream*)hob->data;
	buffer = hob->series;


	REBINT flush = ref_finish ? Z_FINISH : (ref_flush ? Z_SYNC_FLUSH : Z_NO_FLUSH );


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
			size = (hob->sym == Handle_ZlibEncoder)?zng_deflateBound(stream, available_in):available_in;
			buffer = hob->series = RL_MAKE_BINARY((REBLEN)size);
		}
	}

	stream->avail_out = SERIES_REST(buffer) - SERIES_TAIL(buffer);
	stream->next_out = BIN_TAIL(buffer);
	debug_print("input total_in: %lu avail_in: %lu avail_out: %lu \n", stream->total_in, stream->avail_in, stream->avail_out);
	
	if (hob->sym == Handle_ZlibEncoder) {
		for (;;) {
			result = zng_deflate(stream, flush);
			debug_print("deflate result: %i  stream.total_out: %zu avail_in %u .avail_out: %u\n", result, stream->total_out, stream->avail_in, stream->avail_out);
			if (result < 0) {
				RETURN_ERROR(ERR_NO_COMPRESS);
			}
			if (stream->avail_out == 0) {
				// expand output buffer...
				SERIES_TAIL(buffer) = stream->total_out;
				RL_EXPAND_SERIES(buffer, AT_TAIL, (stream->total_in - stream->total_out)/3);
				stream->next_out = BIN_SKIP(buffer, stream->total_out);
				stream->avail_out = SERIES_REST(buffer) - stream->total_out;
			}
			else if (result == Z_OK || result == Z_STREAM_END)
				break; // Finished or we have enough data.
		}
	}
	else {
		for (;;) {
			result = zng_inflate(stream, flush);
			debug_print("inflate result: %i  stream.total_out: %zu avail_in %u .avail_out: %u\n", result, stream->total_out, stream->avail_in, stream->avail_out);
			if (result < 0) {
				RETURN_ERROR(ERR_NO_COMPRESS);
			}	
			if (stream->avail_out == 0) {
				// expand output buffer...
				SERIES_TAIL(buffer) = stream->total_out;
				RL_EXPAND_SERIES(buffer, AT_TAIL, available_in);
				stream->next_out = BIN_SKIP(buffer, stream->total_out);
				stream->avail_out = SERIES_REST(buffer) - stream->total_out;
			}
			else if (result == Z_OK || result == Z_STREAM_END)
				break; // Finished or we have enough data.
		}
	}
	SERIES_TAIL(buffer) = stream->total_out;

	debug_print("tail: %lu\n", SERIES_TAIL(buffer));

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

COMMAND cmd_read(RXIFRM *frm, void *ctx) {
	REBINT result;
	REBLEN tail;
	REBHOB *hob  = RXA_HANDLE(frm, 1);
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
			if (result < 0) {
				RETURN_ERROR(ERR_NO_COMPRESS);
			}
			if (stream->avail_out == 0) {
				// expand output buffer...
				SERIES_TAIL(buffer) = stream->total_out;
				RL_EXPAND_SERIES(buffer, AT_TAIL, (stream->total_in - stream->total_out)/3);
				stream->next_out = BIN_SKIP(buffer, stream->total_out);
				stream->avail_out = SERIES_REST(buffer) - stream->total_out;
			}
			else if (result == Z_OK || result == Z_STREAM_END)
				break; // Finished or we have enough data.
		}
		SERIES_TAIL(buffer) = stream->total_out;

	}
	else {
		// Read from a decompression stream...
	}
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
