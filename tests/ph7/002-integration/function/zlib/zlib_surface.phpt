--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/zlib: the inventory, every signature and the two context classes
--FILE--
<?php
/* The API half of ext/zlib: what the extension IS. The answers it gives are
 * the two tests beside this one. Nothing here prints a compressed byte or the
 * version of the libz this build linked -- both belong to the library rather
 * than to php, and neither is the same on every box. */
echo "-- the extension\n";
var_dump(extension_loaded('zlib'));
echo implode(', ', get_extension_funcs('zlib')), "\n";
$ext = new ReflectionExtension('zlib');
echo implode(', ', $ext->getClassNames()), "\n";
foreach ($ext->getConstants() as $name => $value) {
    if ($name === 'ZLIB_VERSION' || $name === 'ZLIB_VERNUM') {
        printf("%-24s %s\n", $name, get_debug_type($value));
        continue;
    }
    printf("%-24s %d\n", $name, $value);
}
foreach ($ext->getINIEntries() as $name => $value) {
    printf("%-32s %s\n", $name, var_export($value, true));
}

echo "-- every function's signature\n";
foreach (get_extension_funcs('zlib') as $name) {
    echo (new ReflectionFunction($name))->__toString(), "\n";
}

echo "-- the two contexts are opaque handles\n";
foreach (['DeflateContext', 'InflateContext'] as $name) {
    $c = new ReflectionClass($name);
    printf("%s final=%d instantiable=%d methods=%d properties=%d constants=%d\n",
        $name, (int) $c->isFinal(), (int) $c->isInstantiable(),
        count($c->getMethods()), count($c->getProperties()), count($c->getConstants()));
    try {
        new $name();
    } catch (Throwable $e) {
        echo '  new: ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}

echo "-- and neither may be copied or serialized\n";
$d = deflate_init(ZLIB_ENCODING_RAW);
var_dump($d instanceof DeflateContext, get_debug_type($d), (array) $d, get_object_vars($d));
try { clone $d; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { serialize($d); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "-- the compress.zlib wrapper and the zlib.* filters are registered\n";
var_dump(in_array('compress.zlib', stream_get_wrappers(), true));
var_dump(in_array('zlib.*', stream_get_filters(), true));

echo "-- what the ini says with no output layer under it\n";
var_dump(ini_get('zlib.output_compression'), ini_get('zlib.output_compression_level'),
    ini_get('zlib.output_handler'), zlib_get_coding_type(), ob_gzhandler('x', 0));
--EXPECT--
-- the extension
bool(true)
ob_gzhandler, zlib_get_coding_type, gzfile, gzopen, readgzfile, zlib_encode, zlib_decode, gzdeflate, gzencode, gzcompress, gzinflate, gzdecode, gzuncompress, gzwrite, gzputs, gzrewind, gzclose, gzeof, gzgetc, gzpassthru, gzseek, gztell, gzread, gzgets, deflate_init, deflate_add, inflate_init, inflate_add, inflate_get_status, inflate_get_read_len
InflateContext, DeflateContext
FORCE_GZIP               31
FORCE_DEFLATE            15
ZLIB_ENCODING_RAW        -15
ZLIB_ENCODING_GZIP       31
ZLIB_ENCODING_DEFLATE    15
ZLIB_NO_FLUSH            0
ZLIB_PARTIAL_FLUSH       1
ZLIB_SYNC_FLUSH          2
ZLIB_FULL_FLUSH          3
ZLIB_BLOCK               5
ZLIB_FINISH              4
ZLIB_FILTERED            1
ZLIB_HUFFMAN_ONLY        2
ZLIB_RLE                 3
ZLIB_FIXED               4
ZLIB_DEFAULT_STRATEGY    0
ZLIB_VERSION             string
ZLIB_VERNUM              int
ZLIB_OK                  0
ZLIB_STREAM_END          1
ZLIB_NEED_DICT           2
ZLIB_ERRNO               -1
ZLIB_STREAM_ERROR        -2
ZLIB_DATA_ERROR          -3
ZLIB_MEM_ERROR           -4
ZLIB_BUF_ERROR           -5
ZLIB_VERSION_ERROR       -6
zlib.output_compression          ''
zlib.output_compression_level    '-1'
zlib.output_handler              ''
-- every function's signature
Function [ <internal:zlib> function ob_gzhandler ] {

  - Parameters [2] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> int $flags ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function zlib_get_coding_type ] {

  - Parameters [0] {
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function gzfile ] {

  - Parameters [2] {
    Parameter #0 [ <required> string $filename ]
    Parameter #1 [ <optional> bool $use_include_path = false ]
  }
  - Return [ array|false ]
}

Function [ <internal:zlib> function gzopen ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $filename ]
    Parameter #1 [ <required> string $mode ]
    Parameter #2 [ <optional> bool $use_include_path = false ]
  }
}

Function [ <internal:zlib> function readgzfile ] {

  - Parameters [2] {
    Parameter #0 [ <required> string $filename ]
    Parameter #1 [ <optional> bool $use_include_path = false ]
  }
  - Return [ int|false ]
}

Function [ <internal:zlib> function zlib_encode ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> int $encoding ]
    Parameter #2 [ <optional> int $level = -1 ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function zlib_decode ] {

  - Parameters [2] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <optional> int $max_length = 0 ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function gzdeflate ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <optional> int $level = -1 ]
    Parameter #2 [ <optional> int $encoding = ZLIB_ENCODING_RAW ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function gzencode ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <optional> int $level = -1 ]
    Parameter #2 [ <optional> int $encoding = ZLIB_ENCODING_GZIP ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function gzcompress ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <optional> int $level = -1 ]
    Parameter #2 [ <optional> int $encoding = ZLIB_ENCODING_DEFLATE ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function gzinflate ] {

  - Parameters [2] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <optional> int $max_length = 0 ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function gzdecode ] {

  - Parameters [2] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <optional> int $max_length = 0 ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function gzuncompress ] {

  - Parameters [2] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <optional> int $max_length = 0 ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function gzwrite ] {

  - Parameters [3] {
    Parameter #0 [ <required> $stream ]
    Parameter #1 [ <required> string $data ]
    Parameter #2 [ <optional> ?int $length = null ]
  }
  - Return [ int|false ]
}

Function [ <internal:zlib> function gzputs ] {

  - Parameters [3] {
    Parameter #0 [ <required> $stream ]
    Parameter #1 [ <required> string $data ]
    Parameter #2 [ <optional> ?int $length = null ]
  }
  - Return [ int|false ]
}

Function [ <internal:zlib> function gzrewind ] {

  - Parameters [1] {
    Parameter #0 [ <required> $stream ]
  }
  - Return [ bool ]
}

Function [ <internal:zlib> function gzclose ] {

  - Parameters [1] {
    Parameter #0 [ <required> $stream ]
  }
  - Return [ bool ]
}

Function [ <internal:zlib> function gzeof ] {

  - Parameters [1] {
    Parameter #0 [ <required> $stream ]
  }
  - Return [ bool ]
}

Function [ <internal:zlib> function gzgetc ] {

  - Parameters [1] {
    Parameter #0 [ <required> $stream ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function gzpassthru ] {

  - Parameters [1] {
    Parameter #0 [ <required> $stream ]
  }
  - Return [ int ]
}

Function [ <internal:zlib> function gzseek ] {

  - Parameters [3] {
    Parameter #0 [ <required> $stream ]
    Parameter #1 [ <required> int $offset ]
    Parameter #2 [ <optional> int $whence = SEEK_SET ]
  }
  - Return [ int ]
}

Function [ <internal:zlib> function gztell ] {

  - Parameters [1] {
    Parameter #0 [ <required> $stream ]
  }
  - Return [ int|false ]
}

Function [ <internal:zlib> function gzread ] {

  - Parameters [2] {
    Parameter #0 [ <required> $stream ]
    Parameter #1 [ <required> int $length ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function gzgets ] {

  - Parameters [2] {
    Parameter #0 [ <required> $stream ]
    Parameter #1 [ <optional> ?int $length = null ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function deflate_init ] {

  - Parameters [2] {
    Parameter #0 [ <required> int $encoding ]
    Parameter #1 [ <optional> object|array $options = [] ]
  }
  - Return [ DeflateContext|false ]
}

Function [ <internal:zlib> function deflate_add ] {

  - Parameters [3] {
    Parameter #0 [ <required> DeflateContext $context ]
    Parameter #1 [ <required> string $data ]
    Parameter #2 [ <optional> int $flush_mode = ZLIB_SYNC_FLUSH ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function inflate_init ] {

  - Parameters [2] {
    Parameter #0 [ <required> int $encoding ]
    Parameter #1 [ <optional> object|array $options = [] ]
  }
  - Return [ InflateContext|false ]
}

Function [ <internal:zlib> function inflate_add ] {

  - Parameters [3] {
    Parameter #0 [ <required> InflateContext $context ]
    Parameter #1 [ <required> string $data ]
    Parameter #2 [ <optional> int $flush_mode = ZLIB_SYNC_FLUSH ]
  }
  - Return [ string|false ]
}

Function [ <internal:zlib> function inflate_get_status ] {

  - Parameters [1] {
    Parameter #0 [ <required> InflateContext $context ]
  }
  - Return [ int ]
}

Function [ <internal:zlib> function inflate_get_read_len ] {

  - Parameters [1] {
    Parameter #0 [ <required> InflateContext $context ]
  }
  - Return [ int ]
}

-- the two contexts are opaque handles
DeflateContext final=1 instantiable=1 methods=0 properties=0 constants=0
  new: Error: Cannot directly construct DeflateContext, use deflate_init() instead
InflateContext final=1 instantiable=1 methods=0 properties=0 constants=0
  new: Error: Cannot directly construct InflateContext, use inflate_init() instead
-- and neither may be copied or serialized
bool(true)
string(14) "DeflateContext"
array(0) {
}
array(0) {
}
Error: Trying to clone an uncloneable object of class DeflateContext
Exception: Serialization of 'DeflateContext' is not allowed
-- the compress.zlib wrapper and the zlib.* filters are registered
bool(true)
bool(true)
-- what the ini says with no output layer under it
string(0) ""
string(2) "-1"
string(0) ""
bool(false)
bool(false)
