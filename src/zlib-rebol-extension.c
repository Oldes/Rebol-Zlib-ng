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

#include "zlib-rebol-extension.h"

RL_LIB *RL; // Link back to reb-lib from embedded extensions

//==== Globals ===============================================================//
extern MyCommandPointer Command[];
REBCNT Handle_ZlibEncoder;
REBCNT Handle_ZlibDecoder;

u32* arg_words;
u32* type_words;
//============================================================================//

static const char* init_block = ZLIB_EXT_INIT_CODE;

int CompressDeflate(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error);
int CompressGzip(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error);
int CompressZlib(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error);

int DecompressDeflate(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error);
int DecompressGzip(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error);
int DecompressZlib(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error);

int Common_mold(REBHOB *hob, REBSER *ser);

int ZlibEncHandle_free(void* hndl);
int ZlibEncHandle_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int ZlibEncHandle_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);

int ZlibDecHandle_free(void* hndl);
int ZlibDecHandle_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int ZlibDecHandle_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);

RXIEXT const char *RX_Init(int opts, RL_LIB *lib) {
	RL = lib;
	REBYTE ver[8];
	RL_VERSION(ver);
	debug_print(
		"RXinit zlib-ng-extension; Rebol v%i.%i.%i\n",
		ver[1], ver[2], ver[3]);

	if (MIN_REBOL_VERSION > VERSION(ver[1], ver[2], ver[3])) {
		debug_print(
			"Needs at least Rebol v%i.%i.%i!\n",
			 MIN_REBOL_VER, MIN_REBOL_REV, MIN_REBOL_UPD);
		return 0;
	}
	if (!CHECK_STRUCT_ALIGN) {
		trace("CHECK_STRUCT_ALIGN failed!");
		return 0;
	}

	REBHSP spec;
	spec.mold = Common_mold;

	spec.size      = sizeof(zng_stream);
	spec.flags     = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free      = ZlibEncHandle_free;
	spec.get_path  = ZlibEncHandle_get_path;
	spec.set_path  = ZlibEncHandle_set_path;
	Handle_ZlibEncoder = RL_REGISTER_HANDLE_SPEC((REBYTE*)"zlib-ng-encoder", &spec);

	spec.size      = sizeof(zng_stream);
	spec.flags     = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free      = ZlibDecHandle_free;
	spec.get_path  = ZlibDecHandle_get_path;
	//spec.set_path  = ZlibDecHandle_set_path;
	Handle_ZlibDecoder = RL_REGISTER_HANDLE_SPEC((REBYTE*)"zlib-ng-decoder", &spec);

	RL_REGISTER_COMPRESS_METHOD(cb_cast("deflate-ng"), CompressDeflate, DecompressDeflate);
	RL_REGISTER_COMPRESS_METHOD(cb_cast("zlib-ng"), CompressZlib, DecompressZlib);
	RL_REGISTER_COMPRESS_METHOD(cb_cast("gzip-ng"), CompressGzip, DecompressGzip);

	return init_block;
}

RXIEXT int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Command[cmd](frm, ctx);
}
