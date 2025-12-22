Rebol [
    title: "Rebol/Zlib extension CI test"
]

print ["Running test on Rebol build:" mold to-block system/build]

;; make sure that we load a fresh extension
try [system/modules/zlib-ng: none]
;; use current directory as a modules location
system/options/modules: what-dir

;; import the extension
zng: import 'zlib-ng

print as-yellow "Content of the module..."
? zng


print ["Zlib-ng version:" zng/version]


errors: 0
file: %src/rebol-extension.h ;; File to use as a source to compress.
text: to string! read file
sha1: checksum text 'sha1

;-----------------------------------------------------------------------
print-horizontal-line

print as-yellow "Basic de/compression using various qualities."
for quality 1 9 1 [
    unless all [
        time: dt [bin: zng/compress/level text :quality]
        print ["  quality:" quality "^-compressed size:" length? bin "time:" time]
        str: to string! zng/decompress bin
        equal? text str
    ][
        print [as-red "Failed to compress using quality:" quality]
        ++ errors
    ] 
]


print as-yellow "Using short input..."
short-text: "X"
for quality 1 9 1 [
    unless all [
        time: dt [bin: zng/compress/level short-text :quality]
        print ["  quality:" quality "^-compressed size:" length? bin "time:" time]
        str: to string! zng/decompress bin
        equal? short-text str
    ][
        print [as-red "Failed to de/compress short text using quality:" quality]
        ++ errors
    ] 
]

;-----------------------------------------------------------------------
print-horizontal-line
print as-yellow "Compress using deflate streaming API."
unless all [
    handle? enc: zng/make-encoder
    ? enc
    none?   zng/read  :enc      ;; reading from encoder without data returns none
    handle? zng/write :enc text ;; feed some data
    bin3:   zng/write :enc none ;; finish using none value
    ? bin3
    print ["compressed size:" length? bin3]
    str3:   attempt [to string! decompress bin3 'deflate]
    ? str3
    sha1 == checksum str3 'sha1

][
    print as-red "Failed to compress using deflate streaming API!"
    ++ errors
]

print as-yellow "Compress using zlib streaming API."
unless all [
    handle? enc: zng/make-zlib-encoder
    ? enc
    none?   zng/read  :enc      ;; reading from encoder without data returns none
    handle? zng/write :enc text ;; feed some data
    bin3:   zng/write :enc none ;; finish using none value
    ? bin3
    print ["compressed size:" length? bin3]
    str3:   attempt [to string! decompress bin3 'zlib]
    ? str3
    sha1 == checksum str3 'sha1

][
    print as-red "Failed to compress using streaming API!"
    ++ errors
]

print as-yellow "Compress using gzip streaming API."
unless all [
    handle? enc: zng/make-gzip-encoder
    ? enc
    none?   zng/read  :enc      ;; reading from encoder without data returns none
    handle? zng/write :enc text ;; feed some data
    bin3:   zng/write :enc none ;; finish using none value
    ? bin3
    print ["compressed size:" length? bin3]
    str3:   attempt [to string! decompress bin3 'gzip]
    ? str3
    sha1 == checksum str3 'sha1

][
    print as-red "Failed to compress using streaming API!"
    ++ errors
]


;-----------------------------------------------------------------------
print-horizontal-line
print as-yellow "Streaming using short inputs."
;; It is possible to reuse previous handle (when it was finished)
enc: zng/make-encoder
dec: zng/make-decoder
unless all [
    zng/write :enc "Hello"  ;; Process some input data
    zng/write :enc " "
    zng/write :enc "Zlib-ng"
    ;; When there is enough data to compress,
    ;; use `read` to finish the current data block and get the encoded chunk
    bin1: zng/read :enc
    ? bin1
    ;; Continue with other data and use `/finish` to encode all remaining input
    ;; and mark the stream as complete.
    bin2: zng/write/finish :enc " from Rebol!"
    ? bin2

    ;; Decompress chunks one by one using a decoder:
    str1: to string! zng/write/flush :dec bin1
    ? str1
    str2: to string! zng/write/flush :dec bin2
    ? str2
    equal? join str1 str2 "Hello Zlib-ng from Rebol!"

    ;; Decompress both compressed blocks again (using extension's command):
    str: to string! zng/decompress join bin1 bin2
    ? str
    equal? str "Hello Zlib-ng from Rebol!"

    ;; Or using streaming API...
    dec: zng/make-decoder
    zng/write :dec bin1
    zng/write :dec bin2
    str: to string! zng/read :dec
    ? str
    equal? str "Hello Zlib-ng from Rebol!"

    ;; Or using streaming API with write/finish
    ;; NOTE: it is possible to reuse the decoder
    dec: zng/make-decoder
    zng/write :dec bin1
    str: to string! zng/write/finish :dec bin2
    ? str
    equal? str "Hello Zlib-ng from Rebol!"
][
    print as-red "Failed to compress/decompress short inputs using streaming API!"
    ++ errors
]


;-----------------------------------------------------------------------
print-horizontal-line
print as-yellow "Compress and decompress files in chunks."

gz-compress-file: function[file][
    src: open/read file                 ;; input file
    out: open/new/write join file %.gz  ;; output file
    enc: zng/make-gzip-encoder          ;; initialize gzip encoder
    chunk-size: 65536
    while [not finish][
        chunk: copy/part src chunk-size
        ;; If length of the chunk is less than chunk-size,
        ;; it must be the last chunk and we can finish the stream.
        finish: chunk-size > length? chunk
        ;; Flush output after each chunk.
        write out zng/write/flush/:finish :enc :chunk
    ]
    close src
    close out
]
gz-decompress-file: function[file][
    src: open/read file                 ;; input file
    dec: zng/make-gzip-decoder          ;; initialize Zstd decoder
    chunk-size: 65536
    while [not empty? chunk: copy/part src chunk-size][
        zng/write :dec :chunk
    ]
    close src
    zng/read :dec
]

unless all [
    compressed-file: gz-compress-file :file
    print ["compressed size:" size? compressed-file]
    str4: to string! gz-decompress-file :compressed-file
    ? str4
    equal? text str4
][
    print as-red "Failed to compress & decompress file."
    ++ errors
]
delete compressed-file

;-----------------------------------------------------------------------
print-horizontal-line

prin as-yellow "TEST RESULT: "
print either errors == 0 [as-green "OK"][as-red "FAILED!"]
quit/return errors


%{
;-----------------------------------------------------------------------
print-horizontal-line
print as-yellow "Basic de/compression."
text: to string! read file
size: length? text
? text
? size
print to-decimal dt [bin: compress/level text 'zlib   9]
print to-decimal dt [bin: compress/level text 'zlib-ng 9]
? bin
print length? bin
str: to string! decompress bin 'zlib
? str

? system/catalog/compressions
test: mold system
size: length? test
? test
? size
foreach method [deflate deflate-ng zlib zlib-ng gzip gzip-ng br zstd][
    ctm: to decimal! dt [bin: compress/level test method 9]
    len: length? bin
    dtm: to decimal! dt [str: decompress/size bin method size]
    prin [pad method 10 pad ctm 10 pad dtm 10 len]
    print either test = to string! str [" OK"][" KO"]
]
}%