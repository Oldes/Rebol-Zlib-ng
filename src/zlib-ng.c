//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Zlib-ng extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Entry points for the Rebol/Zlib-ng extension module.
//
//   REB_EXT defined ... standalone zlib-ng-x64.rebx
//   REB_EXT absent .... compiled into the host
//
// One-time setup lives in Zlib_ng_Init(), called from the generated `_init`
// command when the module body evaluates - not from the entry points, so
// that `Options: [delay]` can postpone it until the module is imported.
//

#include "gen-zlib-ng.h"

#ifdef REB_EXT
// Standalone builds are their own binary and must supply the storage.
// Embedded builds use host-lib.c's definition, declared extern by reb-lib.h.
RL_LIB *RL;

static char *init_block = ZLIB_NG_EXT_INIT_CODE;
#endif

// Symbols of the registered handle types. Declared in the generated header
// (from the spec's `c-header:`), defined here.
REBCNT Handle_ZlibEncoder = 0;
REBCNT Handle_ZlibDecoder = 0;


// Registers the encoder/decoder handle types and the de/compression
// methods. Runs when the module body evaluates, so it happens at the same
// point in both build modes - and only on first import under `delay`.
//
// Returns plain TRUE/FALSE, NOT an RXR_* code: RXR_FALSE is 3, which is
// truthy in C. The generated handler maps the result onto RXR_TRUE/RXR_FALSE.
int Zlib_ng_Init(void) {
	REBHSP spec;

	// The zng_stream lives directly in the handle's context data. Neither
	// handle has any path-accessible field, so get_path/set_path are left
	// unset (NULL) - same as the Zstd extension.
	CLEARS(&spec);
	spec.size     = sizeof(zng_stream);
	spec.flags    = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free     = ZlibEncHandle_free;
	spec.mold     = ZlibNg_mold;
	Handle_ZlibEncoder = RL_REGISTER_HANDLE_SPEC(cb_cast("zlib-ng-encoder"), &spec);

	CLEARS(&spec);
	spec.size     = sizeof(zng_stream);
	spec.flags    = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free     = ZlibDecHandle_free;
	spec.mold     = ZlibNg_mold;
	Handle_ZlibDecoder = RL_REGISTER_HANDLE_SPEC(cb_cast("zlib-ng-decoder"), &spec);

	RL_REGISTER_COMPRESS_METHOD(cb_cast("deflate"), ZlibNg_CompressDeflate, ZlibNg_DecompressDeflate);
	RL_REGISTER_COMPRESS_METHOD(cb_cast("zlib"),    ZlibNg_CompressZlib,    ZlibNg_DecompressZlib);
	RL_REGISTER_COMPRESS_METHOD(cb_cast("gzip"),    ZlibNg_CompressGzip,    ZlibNg_DecompressGzip);

	RL_REGISTER_COMPRESS_METHOD(cb_cast("deflate-ng"), ZlibNg_CompressDeflate, ZlibNg_DecompressDeflate);
	RL_REGISTER_COMPRESS_METHOD(cb_cast("zlib-ng"),    ZlibNg_CompressZlib,    ZlibNg_DecompressZlib);
	RL_REGISTER_COMPRESS_METHOD(cb_cast("gzip-ng"),    ZlibNg_CompressGzip,    ZlibNg_DecompressGzip);

	return TRUE;
}


#ifdef REB_EXT

/***********************************************************************
**  Standalone extension library
***********************************************************************/

RXIEXT const char *RX_Init(int opts, RL_LIB *lib) {
	REBYTE ver[8];
	RL = lib;
	RL_VERSION(ver);
	debug_print("RX_Init zlib-ng-extension; Rebol v%i.%i.%i\n", ver[1], ver[2], ver[3]);

	if (MIN_REBOL_VERSION > VERSION(ver[1], ver[2], ver[3])) {
		debug_print("Needs at least Rebol v%i.%i.%i!\n", MIN_REBOL_VER, MIN_REBOL_REV, MIN_REBOL_UPD);
		return 0;
	}
	if (!CHECK_STRUCT_ALIGN) {
		trace("CHECK_STRUCT_ALIGN failed!");
		return 0;
	}
	return init_block;
}

RXIEXT int RX_Quit(int opts) {
	return 0;
}

// Reports the RL_API ABI this was built against, so `load-extension`
// can refuse an incompatible host. An absent symbol means ABI 0.
RXIEXT int RX_Abi(void) {
	return RL_ABI_VERSION;
}

// Resolved by name, so the spelling is fixed. The bounds-checked
// dispatcher is generated into gen-zlib-ng.c.
RXIEXT int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Zlib_ng_RX_Call(cmd, frm, ctx);
}

#endif
