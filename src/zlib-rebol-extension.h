//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Zlib-ng extension
// SPDX-License-Identifier: MIT
// =============================================================================
// NOTE: auto-generated file, do not modify!


#include "rebol-extension.h"
#include "zlib-ng.h"

#define SERIES_TEXT(s)   ((char*)SERIES_DATA(s))

#define MIN_REBOL_VER 3
#define MIN_REBOL_REV 20
#define MIN_REBOL_UPD 5
#define VERSION(a, b, c) (a << 16) + (b << 8) + c
#define MIN_REBOL_VERSION VERSION(MIN_REBOL_VER, MIN_REBOL_REV, MIN_REBOL_UPD)

#ifndef NO_LIMIT
#define NO_LIMIT ((REBCNT)-1)
#endif


extern REBCNT Handle_ZlibEncoder;
extern REBCNT Handle_ZlibDecoder;

extern u32* arg_words;
extern u32* type_words;

enum ext_commands {
	CMD_ZLIB_INIT_WORDS,
	CMD_ZLIB_VERSION,
	CMD_ZLIB_COMPRESS,
	CMD_ZLIB_DECOMPRESS,
	CMD_ZLIB_MAKE_ENCODER,
	CMD_ZLIB_MAKE_ZLIB_ENCODER,
	CMD_ZLIB_MAKE_GZIP_ENCODER,
	CMD_ZLIB_MAKE_DECODER,
	CMD_ZLIB_MAKE_ZLIB_DECODER,
	CMD_ZLIB_MAKE_GZIP_DECODER,
	CMD_ZLIB_WRITE,
	CMD_ZLIB_READ,
};


int cmd_init_words(RXIFRM *frm, void *ctx);
int cmd_version(RXIFRM *frm, void *ctx);
int cmd_compress(RXIFRM *frm, void *ctx);
int cmd_decompress(RXIFRM *frm, void *ctx);
int cmd_make_encoder(RXIFRM *frm, void *ctx);
int cmd_make_zlib_encoder(RXIFRM *frm, void *ctx);
int cmd_make_gzip_encoder(RXIFRM *frm, void *ctx);
int cmd_make_decoder(RXIFRM *frm, void *ctx);
int cmd_make_zlib_decoder(RXIFRM *frm, void *ctx);
int cmd_make_gzip_decoder(RXIFRM *frm, void *ctx);
int cmd_write(RXIFRM *frm, void *ctx);
int cmd_read(RXIFRM *frm, void *ctx);

enum ma_arg_words {W_ARG_0
};
enum ma_type_words {W_TYPE_0
};

typedef int (*MyCommandPointer)(RXIFRM *frm, void *ctx);

#define ZLIB_EXT_INIT_CODE \
	"REBOL [Title: \"Rebol Zlib-NG Extension\" Name: zlib-ng Type: module Version: 0.1.0 Needs: 3.20.5 Author: Oldes Date: 19-Dec-2025/21:49:44 License: MIT Url: https://github.com/Oldes/Rebol-Zlib-ng]\n"\
	"init-words: command [args [block!] type [block!]]\n"\
	"version: command [\"Native Zlib-ng version\"]\n"\
	"compress: command [\"Compress data using Zlib.\" data [binary! any-string!] \"Input data to compress.\" /part \"Limit the input data to a given length.\" length [integer!] \"Length of input data.\" /level quality [integer!] \"Compression level from 0 to 9.\" /zlib \"Assumes the zlib wrapper format\" /gzip \"Assumes the gzip wrapper format\"]\n"\
	"decompress: command [\"Decompress data using Zlib.\" data [binary! any-string!] \"Input data to decompress.\" /part \"Limit the input data to a given length.\" length [integer!] \"Length of input data.\" /size \"Limit the output size.\" bytes [integer!] \"Maximum number of uncompressed bytes.\" /zlib \"Assumes the zlib wrapper format\" /gzip \"Assumes the gzip wrapper format\"]\n"\
	"make-encoder: command [{Creates a deflate encoder handle for streaming compression.} /level \"Compression level (0..9, default 6)\" quality [integer!] \"0=fastest, 9=best ratio\" /window \"Window size configuration\" window-bits [integer!] \"8..15 (zlib), -8..-15 (raw deflate), 24..31 (gzip)\" /strategy \"Compression strategy\" method [integer!] \"1=Z_FILTERED, 2=Z_HUFFMAN_ONLY, 3=Z_RLE, 4=Z_FIXED\"]\n"\
	"make-zlib-encoder: command [{Creates a zlib-wrapped deflate encoder (window-bits=15).} /level \"Compression level (0-9, default 6)\" quality [integer!] \"0=fastest, 9=best ratio\"]\n"\
	"make-gzip-encoder: command [{Creates a gzip-wrapped deflate encoder (window-bits=31).} /level \"Compression level (0..9, default 6)\" quality [integer!] \"0=fastest, 9=best ratio\"]\n"\
	"make-decoder: command [{Creates a deflate decoder handle for streaming decompression.} /window \"Expected window size (auto-detect by default)\" window-bits [integer!] \"8..15 (zlib), -8..-15 (raw deflate), 24..31 (gzip)\"]\n"\
	"make-zlib-decoder: command [{Creates a zlib-wrapped deflate decoder (window-bits=15).}]\n"\
	"make-gzip-decoder: command [{Creates a gzip-wrapped deflate decoder (window-bits=31).}]\n"\
	"write: command [\"Feed data into a Zlib streaming codec.\" codec [handle!] \"Zlib encoder or decoder handle.\" data [binary! any-string! none!] {Data to compress or decompress, or NONE to finish the stream.} /flush {Finish the current data block and return the encoded chunk.} /finish {Encode all remaining input and mark the stream as complete.}]\n"\
	"read: command [{Retrieve pending encoded or decoded data from the stream.} codec [handle!] \"Zlib encoder or decoder handle.\"]\n"\
	"init-words [][]\n"\
	"protect/hide 'init-words\n"\
	"\n"\
	";; compression strategy\n"\
	"Z_FILTERED:     1\n"\
	"Z_HUFFMAN_ONLY: 2\n"\
	"Z_RLE:          3\n"\
	"Z_FIXED:        4\n"\
	"\n"\
	";; Update HTTP scheme that it's able to use the Zlib compression\n"\
	";attempt [\n"\
	";	unless parse system/schemes/http/headers/Accept-Encoding [\n"\
	";		thru #\",\" any #\" \" \"zlib\" any #\" \" [end | #\",\"] to end\n"\
	";	][\n"\
	";		append system/schemes/http/headers/Accept-Encoding \",zlib,deflate,gzip\"\n"\
	";	]\n"\
	";]\n"\
	"\n"

#ifdef  USE_TRACES
#include <stdio.h>
#define debug_print(fmt, ...) do { printf(fmt, __VA_ARGS__); } while (0)
#define trace(str) puts(str)
#else
#define debug_print(fmt, ...)
#define trace(str) 
#endif

