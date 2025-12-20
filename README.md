[![rebol-zlib-ng](https://github.com/user-attachments/assets/9055b8e8-a5e0-4e74-be5b-71ad7561d130)](https://github.com/Oldes/Rebol-Zlib-ng)

[![Rebol-Zlib-ng CI](https://github.com/Oldes/Rebol-Zlib-ng/actions/workflows/main.yml/badge.svg)](https://github.com/Oldes/Rebol-Zlib-ng/actions/workflows/main.yml)
[![Gitter](https://badges.gitter.im/rebol3/community.svg)](https://app.gitter.im/#/room/#Rebol3:gitter.im)
[![Zulip](https://img.shields.io/badge/zulip-join_chat-brightgreen.svg)](https://rebol.zulipchat.com/)

# Rebol/Zlib-ng

[Zlib-ng](https://github.com/zlib-ng/zlib-ng) extension for [Rebol3](https://github.com/Oldes/Rebol3) (version 3.20.5 and higher)


## Basic usage
Use Zlib-ng as a codec for the standard compress and decompress functions:
```rebol
import zlib-ng
bin: compress "some data" 'zlib
txt: to string! decompress bin 'zlib
```

## Streaming API
The streaming API lets you process data in chunks, which is useful for large inputs, pipes, or network streams.
```rebol
zlib: import zlib-ng     ;; Import the module and assign it to a variable
enc: zlib/make-encoder   ;; Initialize the Zlib-ng encoder state handle
zlib/write :enc "Hello"  ;; Process some input data
zlib/write :enc " "
zlib/write :enc "Zlib-ng"
;; When there is enough data to compress,
;; use `read` to finish the current data block and get the encoded chunk
bin1: zlib/read :enc
;; Continue with other data and use `/finish` to encode all remaining input
;; and mark the stream as complete.
bin2: zlib/write/finish :enc " from Rebol!"

;; Decompress both compressed blocks again (using extension's command this time):
text: to string! zlib/decompress join bin1 bin2
;== "Hello Zlib-ng from Rebol!"

;; Or using streaming API:
dec: zlib/make-decoder   ;; Initialize the Zlib-ng decoder state handle
zlib/write :dec bin1     ;; Feed some compressed chunks
zlib/write :dec bin2
text: to string! zlib/read :dec ;; Resolve decompressed data
;== "Hello Zlib-ng from Rebol!"
```

### Example: file helpers
These (basic) examples show how to build file-level utilities on top of the streaming API.
```rebol
gz-compress-file: function[file][
    src: open/read file                 ;; input file
    out: open/new/write join file %.gz  ;; output file
    enc: zlib/make-gzip-encoder/level 6 ;; initialize gzip encoder
    chunk-size: 65536
    while [not finish][
        chunk: copy/part src chunk-size
        ;; If length of the chunk is less than chunk-size,
        ;; it must be the last chunk and we can finish the stream.
        finish: chunk-size > length? chunk
        ;; Flush output after each chunk.
        write out zlib/write/flush/:finish :enc :chunk
    ]
    close src
    close out
]
gz-decompress-file: function[file][
    src: open/read file                 ;; input file
    dec: zlib/make-gzip-decoder         ;; initialize gzip decoder
    chunk-size: 65536
    while [not empty? chunk: copy/part src chunk-size][
        zlib/write :dec :chunk
    ]
    close src
    zlib/read :dec
]
```


## Extension commands:


#### `version`
Native Zlib-ng version

#### `compress` `:data`
Compress data using Zlib.
* `data` `[binary! any-string!]` Input data to compress.
* `/part` Limit the input data to a given length.
* `length` `[integer!]` Length of input data.
* `/level`
* `quality` `[integer!]` Compression level from 0 to 9.
* `/zlib` Assumes the zlib wrapper format
* `/gzip` Assumes the gzip wrapper format

#### `decompress` `:data`
Decompress data using Zlib.
* `data` `[binary! any-string!]` Input data to decompress.
* `/part` Limit the input data to a given length.
* `length` `[integer!]` Length of input data.
* `/size` Limit the output size.
* `bytes` `[integer!]` Maximum number of uncompressed bytes.
* `/zlib` Assumes the zlib wrapper format
* `/gzip` Assumes the gzip wrapper format

#### `make-encoder`
Creates a deflate encoder handle for streaming compression.
* `/level` Compression level (0..9, default 6)
* `quality` `[integer!]` 0=fastest, 9=best ratio
* `/window` Window size configuration
* `window-bits` `[integer!]` 8..15 (zlib), -8..-15 (raw deflate), 24..31 (gzip)
* `/strategy` Compression strategy
* `method` `[integer!]` 1=Z_FILTERED, 2=Z_HUFFMAN_ONLY, 3=Z_RLE, 4=Z_FIXED

#### `make-zlib-encoder`
Creates a zlib-wrapped deflate encoder (window-bits=15).
* `/level` Compression level (0-9, default 6)
* `quality` `[integer!]` 0=fastest, 9=best ratio

#### `make-gzip-encoder`
Creates a gzip-wrapped deflate encoder (window-bits=31).
* `/level` Compression level (0..9, default 6)
* `quality` `[integer!]` 0=fastest, 9=best ratio

#### `make-decoder`
Creates a deflate decoder handle for streaming decompression.
* `/window` Expected window size (auto-detect by default)
* `window-bits` `[integer!]` 8..15 (zlib), -8..-15 (raw deflate), 24..31 (gzip)

#### `make-zlib-decoder`
Creates a zlib-wrapped deflate decoder (window-bits=15).

#### `make-gzip-decoder`
Creates a gzip-wrapped deflate decoder (window-bits=31).

#### `write` `:codec` `:data`
Feed data into a Zlib streaming codec.
* `codec` `[handle!]` Zlib encoder or decoder handle.
* `data` `[binary! any-string! none!]` Data to compress or decompress, or NONE to finish the stream.
* `/flush` Finish the current data block and return the encoded chunk.
* `/finish` Encode all remaining input and mark the stream as complete.

#### `read` `:codec`
Retrieve pending encoded or decoded data from the stream.
* `codec` `[handle!]` Zlib encoder or decoder handle.


## Other extension values:
```rebol
;; compression strategy
Z_FILTERED:     1
Z_HUFFMAN_ONLY: 2
Z_RLE:          3
Z_FIXED:        4
```
