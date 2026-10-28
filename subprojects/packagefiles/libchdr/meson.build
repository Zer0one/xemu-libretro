project('libchdr', 'c', version: '0.3.0', default_options: ['c_std=gnu11', 'warning_level=0', 'werror=false'])
# Same portable decoder sources as the pinned upstream CMake default.
# Reuse zlib and build upstream's portable zstd decoder statically. This
# avoids a new host-specific dylib in core packages and a second miniz copy.
z = dependency('zlib')
zstd_lib = static_library('chdr-zstd', 'deps/zstd-1.5.7/zstddeclib.c', pic: true)
zstd_dep = declare_dependency(link_with: zstd_lib, include_directories: include_directories('deps/zstd-1.5.7'))
sources = files('src/libchdr_bitstream.c', 'src/libchdr_cdrom.c', 'src/libchdr_chd.c',
  'src/libchdr_codec_avhuff.c', 'src/libchdr_codec_cdfl.c', 'src/libchdr_codec_cdlz.c',
  'src/libchdr_codec_cdzl.c', 'src/libchdr_codec_cdzs.c', 'src/libchdr_codec_flac.c',
  'src/libchdr_codec_huff.c', 'src/libchdr_codec_lzma.c', 'src/libchdr_codec_zlib.c',
  'src/libchdr_codec_zstd.c', 'src/libchdr_flac.c', 'src/libchdr_huffman.c',
  'deps/lzma-26.02/src/LzmaDec.c')
inc = include_directories('include', 'deps/lzma-26.02/include')
chdr = static_library('chdr', sources, include_directories: inc, dependencies: [z, zstd_dep],
  c_args: ['-DCHDR_SYSTEM_ZLIB', '-DCHDR_SYSTEM_ZSTD', '-DWANT_RAW_DATA_SECTOR=1',
           '-DWANT_SUBCODE=1', '-DVERIFY_BLOCK_CRC=1', '-DLOWRAM_TARGET=0'], pic: true)
chdr_dep = declare_dependency(link_with: chdr, include_directories: include_directories('include'), dependencies: [z, zstd_dep])
