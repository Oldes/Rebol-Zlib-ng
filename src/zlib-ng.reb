REBOL [
	Title:   "Rebol Zlib-NG Extension"
	Name:    zlib-ng
	Version: 0.1.0
	Needs:   3.20.5
	Author:  @Oldes
	License: MIT
	Options: [delay]
	Url:     https://github.com/Oldes/Rebol-Zlib-ng
	Exports: []
	Purpose: {
		Deflate, zlib and gzip (de)compression using zlib-ng - one-shot
		and streaming - plus registration of the "deflate", "zlib" and
		"gzip" compression methods (and their "-ng" aliases) usable from
		Rebol's compress/decompress natives.

		Data only - never evaluated. The C header (gen-zlib-ng.h) and the
		command table (gen-zlib-ng.c) are generated from this file by
		make-extension.r3.
	}
]

;; ---------------------------------------------------------------------------
;; Banner put on top of the generated files.
logo: {//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Zlib-ng extension
// SPDX-License-Identifier: MIT
// =============================================================================
// NOTE: auto-generated file, do not modify!}

;; ---------------------------------------------------------------------------
;; C-side configuration
;;
;; NOTE: the zlib-ng header is carried here rather than by `c-include:`.
;; The template only emits `c-include:` inside the `#ifdef REB_EXT` branch,
;; but zng_stream is needed in both build modes (same as zstd.reb).
c-header: {
#include "zlib-ng.h"

#ifndef NO_LIMIT
#define NO_LIMIT ((REBCNT)-1)
#endif

extern REBCNT Handle_ZlibEncoder;
extern REBCNT Handle_ZlibDecoder;

int ZlibNg_mold(REBHOB *hob, REBSER *ser);

int ZlibEncHandle_free(void *hndl);
int ZlibDecHandle_free(void *hndl);

int ZlibNg_CompressDeflate(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error);
int ZlibNg_CompressGzip(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error);
int ZlibNg_CompressZlib(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error);

int ZlibNg_DecompressDeflate(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error);
int ZlibNg_DecompressGzip(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error);
int ZlibNg_DecompressZlib(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error);
}

;; ---------------------------------------------------------------------------
;; No `handles:` block: `zlib-ng-encoder`/`zlib-ng-decoder` are registered by
;; hand in zlib-ng.c's Zlib_ng_Init (a zng_stream embedded in the handle
;; context). Neither type has a path-accessible field, so there is nothing
;; for this block to collect or document. If a field is ever added, add it
;; here as `NAME  GET-type  SET-type  "description"` plus a get_path/set_path
;; pair wired up in Zlib_ng_Init.

;; ---------------------------------------------------------------------------
;; Commands. Order is significant - it fixes the command indices.
;; The `_init` command is injected as the first one by the generator.
commands: [
	version: ["Native Zlib-ng version"]

	compress: [
		"Compress data using Zlib."
		data [binary! any-string!] "Input data to compress."
		/part                      "Limit the input data to a given length."
		 length [integer!]         "Length of input data."
		/level quality [integer!]  "Compression level from 0 to 9."
		/zlib "Assumes the zlib wrapper format"
		/gzip "Assumes the gzip wrapper format"
	]

	decompress: [
		"Decompress data using Zlib."
		data [binary! any-string!] "Input data to decompress."
		/part                      "Limit the input data to a given length."
		 length [integer!]         "Length of input data."
		/size                      "Limit the output size."
		 bytes [integer!]          "Maximum number of uncompressed bytes."
		/zlib "Assumes the zlib wrapper format"
		/gzip "Assumes the gzip wrapper format"
	]

	;; Streaming API
	make-encoder: [
		"Creates a deflate encoder handle for streaming compression."
		/level                  "Compression level (0..9, default 6)"
		 quality     [integer!] "0=fastest, 9=best ratio"
		/window                 "Window size configuration"
		 window-bits [integer!] "8..15 (zlib), -8..-15 (raw deflate), 24..31 (gzip)"
		/strategy               "Compression strategy"
		 method      [integer!] "1=Z_FILTERED, 2=Z_HUFFMAN_ONLY, 3=Z_RLE, 4=Z_FIXED"
	]

	make-zlib-encoder: [
		"Creates a zlib-wrapped deflate encoder (window-bits=15)."
		/level "Compression level (0-9, default 6)"
		 quality [integer!] "0=fastest, 9=best ratio"
	]

	make-gzip-encoder: [
		"Creates a gzip-wrapped deflate encoder (window-bits=31)."
		/level "Compression level (0..9, default 6)"
		 quality [integer!] "0=fastest, 9=best ratio"
	]

	make-decoder: [
		"Creates a deflate decoder handle for streaming decompression."
		/window "Expected window size (auto-detect by default)"
		 window-bits  [integer!] "8..15 (zlib), -8..-15 (raw deflate), 24..31 (gzip)"
	]

	make-zlib-decoder: [
		"Creates a zlib-wrapped deflate decoder (window-bits=15)."
	]

	make-gzip-decoder: [
		"Creates a gzip-wrapped deflate decoder (window-bits=31)."
	]

	write: [
		"Feed data into a Zlib streaming codec."
		codec [handle!] "Zlib encoder or decoder handle."
		data  [binary! any-string! none!] "Data to compress or decompress, or NONE to finish the stream."
		/flush  "Finish the current data block and return the encoded chunk."
		/finish "Encode all remaining input and mark the stream as complete."
	]

	read: [
		"Retrieve pending encoded or decoded data from the stream."
		codec [handle!] "Zlib encoder or decoder handle."
	]
]

;; ---------------------------------------------------------------------------
;; Module body.
mezzanine: [
	;; compression strategy
	Z_FILTERED:     1
	Z_HUFFMAN_ONLY: 2
	Z_RLE:          3
	Z_FIXED:        4

	;; Update HTTP scheme that it's able to use the Zlib compression
	;attempt [
	;	unless parse system/schemes/http/headers/Accept-Encoding [
	;		thru #"," any #" " "zlib" any #" " [end | #","] to end
	;	][
	;		append system/schemes/http/headers/Accept-Encoding ",zlib,deflate,gzip"
	;	]
	;]
]
