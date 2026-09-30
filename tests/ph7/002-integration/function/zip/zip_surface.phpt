--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/zip: the inventory, the class's whole declaration and the two questions a build answers
--DESCRIPTION--
What the extension IS, as php declares it: ten deprecated functions, one class,
a hundred and two constants and six properties php answers through a handler.
Nothing here prints a compressed byte or a capability that belongs to the
library a build linked -- `isCompressionMethodSupported(CM_BZIP2)` and the AES
encryption methods are a BUILD's answer, not php's, and differ between two
correct engines.
--SKIPIF--
<?php
/* The answers pinned here are libzip 1.7's, which is what this engine derives;
 * a php linked against a newer libzip answers that version's. */
if (!class_exists('ZipArchive')) {
    die("skip this php has no ext/zip\n");
}
if (!str_starts_with(ZipArchive::LIBZIP_VERSION, '1.7.')) {
    die("skip php here links libzip " . ZipArchive::LIBZIP_VERSION . ", not 1.7\n");
}
--FILE--
<?php
error_reporting(E_ALL);
echo "-- the extension\n";
var_dump(extension_loaded('zip'), class_exists('ZipArchive'));
echo implode(', ', get_extension_funcs('zip')), "\n";
$ext = new ReflectionExtension('zip');
echo $ext->getName(), "\n";
echo implode(', ', $ext->getClassNames()), "\n";
var_dump($ext->getConstants(), $ext->getINIEntries());

echo "-- every function's signature\n";
foreach (get_extension_funcs('zip') as $name) {
    echo (new ReflectionFunction($name))->__toString(), "\n";
}

echo "-- the class, whole\n";
$c = new ReflectionClass('ZipArchive');
echo $c->__toString();

echo "-- the six are declared, answered and read-only\n";
foreach ((new ReflectionClass('ZipArchive'))->getProperties() as $p) {
    printf("%-10s type=%-6s virtual=%d default=%d readonly=%d static=%d\n",
        $p->getName(), (string) $p->getType(), (int) $p->isVirtual(),
        (int) $p->hasDefaultValue(), (int) $p->isReadOnly(), (int) $p->isStatic());
}
$z = new ZipArchive();
/* print_r rather than var_dump: an object HANDLE never matches between two
 * engines, and it is the only thing the two renderings differ by */
print_r($z);
var_dump((array) $z, json_encode($z), get_object_vars($z), serialize($z));
foreach ($z as $k => $v) { echo "  foreach $k=", var_export($v, true), "\n"; }
try { $z->numFiles = 7; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { clone $z; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump(isset($z->numFiles), isset($z->nope), property_exists($z, 'comment'));

echo "-- what a build can write\n";
foreach ([ZipArchive::CM_DEFAULT, ZipArchive::CM_STORE, ZipArchive::CM_DEFLATE] as $m) {
    var_dump(ZipArchive::isCompressionMethodSupported($m));
}
foreach ([ZipArchive::EM_NONE, ZipArchive::EM_TRAD_PKWARE, ZipArchive::EM_AES_128,
          ZipArchive::EM_AES_192, ZipArchive::EM_AES_256, ZipArchive::EM_UNKNOWN] as $m) {
    var_dump(ZipArchive::isEncryptionMethodSupported($m));
}
var_dump(get_debug_type(ZipArchive::LIBZIP_VERSION));

echo "-- an object nobody opened\n";
var_dump($z->getStatusString());
foreach (['close', 'count', 'numFiles'] as $verb) {
    try {
        echo $verb, ': ';
        var_dump($verb === 'numFiles' ? $z->numFiles : $z->$verb());
    } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
var_dump(in_array('zip', stream_get_wrappers(), true));
--EXPECT--
-- the extension
bool(true)
bool(true)
zip_open, zip_close, zip_read, zip_entry_open, zip_entry_close, zip_entry_read, zip_entry_name, zip_entry_compressedsize, zip_entry_filesize, zip_entry_compressionmethod
zip
ZipArchive
array(0) {
}
array(0) {
}
-- every function's signature
Function [ <internal, deprecated:zip> function zip_open ] {

  - Parameters [1] {
    Parameter #0 [ <required> string $filename ]
  }
}

Function [ <internal, deprecated:zip> function zip_close ] {

  - Parameters [1] {
    Parameter #0 [ <required> $zip ]
  }
  - Return [ void ]
}

Function [ <internal, deprecated:zip> function zip_read ] {

  - Parameters [1] {
    Parameter #0 [ <required> $zip ]
  }
}

Function [ <internal, deprecated:zip> function zip_entry_open ] {

  - Parameters [3] {
    Parameter #0 [ <required> $zip_dp ]
    Parameter #1 [ <required> $zip_entry ]
    Parameter #2 [ <optional> string $mode = "rb" ]
  }
  - Return [ bool ]
}

Function [ <internal, deprecated:zip> function zip_entry_close ] {

  - Parameters [1] {
    Parameter #0 [ <required> $zip_entry ]
  }
  - Return [ bool ]
}

Function [ <internal, deprecated:zip> function zip_entry_read ] {

  - Parameters [2] {
    Parameter #0 [ <required> $zip_entry ]
    Parameter #1 [ <optional> int $len = 1024 ]
  }
  - Return [ string|false ]
}

Function [ <internal, deprecated:zip> function zip_entry_name ] {

  - Parameters [1] {
    Parameter #0 [ <required> $zip_entry ]
  }
  - Return [ string|false ]
}

Function [ <internal, deprecated:zip> function zip_entry_compressedsize ] {

  - Parameters [1] {
    Parameter #0 [ <required> $zip_entry ]
  }
  - Return [ int|false ]
}

Function [ <internal, deprecated:zip> function zip_entry_filesize ] {

  - Parameters [1] {
    Parameter #0 [ <required> $zip_entry ]
  }
  - Return [ int|false ]
}

Function [ <internal, deprecated:zip> function zip_entry_compressionmethod ] {

  - Parameters [1] {
    Parameter #0 [ <required> $zip_entry ]
  }
  - Return [ string|false ]
}

-- the class, whole
Class [ <internal:zip> class ZipArchive implements Countable ] {

  - Constants [102] {
    Constant [ public int CREATE ] { 1 }
    Constant [ public int EXCL ] { 2 }
    Constant [ public int CHECKCONS ] { 4 }
    Constant [ public int OVERWRITE ] { 8 }
    Constant [ public int RDONLY ] { 16 }
    Constant [ public int FL_NOCASE ] { 1 }
    Constant [ public int FL_NODIR ] { 2 }
    Constant [ public int FL_COMPRESSED ] { 4 }
    Constant [ public int FL_UNCHANGED ] { 8 }
    Constant [ public int FL_RECOMPRESS ] { 16 }
    Constant [ public int FL_ENCRYPTED ] { 32 }
    Constant [ public int FL_OVERWRITE ] { 8192 }
    Constant [ public int FL_LOCAL ] { 256 }
    Constant [ public int FL_CENTRAL ] { 512 }
    Constant [ public int FL_ENC_GUESS ] { 0 }
    Constant [ public int FL_ENC_RAW ] { 64 }
    Constant [ public int FL_ENC_STRICT ] { 128 }
    Constant [ public int FL_ENC_UTF_8 ] { 2048 }
    Constant [ public int FL_ENC_CP437 ] { 4096 }
    Constant [ public int FL_OPEN_FILE_NOW ] { 1073741824 }
    Constant [ public int CM_DEFAULT ] { -1 }
    Constant [ public int CM_STORE ] { 0 }
    Constant [ public int CM_SHRINK ] { 1 }
    Constant [ public int CM_REDUCE_1 ] { 2 }
    Constant [ public int CM_REDUCE_2 ] { 3 }
    Constant [ public int CM_REDUCE_3 ] { 4 }
    Constant [ public int CM_REDUCE_4 ] { 5 }
    Constant [ public int CM_IMPLODE ] { 6 }
    Constant [ public int CM_DEFLATE ] { 8 }
    Constant [ public int CM_DEFLATE64 ] { 9 }
    Constant [ public int CM_PKWARE_IMPLODE ] { 10 }
    Constant [ public int CM_BZIP2 ] { 12 }
    Constant [ public int CM_LZMA ] { 14 }
    Constant [ public int CM_LZMA2 ] { 33 }
    Constant [ public int CM_XZ ] { 95 }
    Constant [ public int CM_TERSE ] { 18 }
    Constant [ public int CM_LZ77 ] { 19 }
    Constant [ public int CM_WAVPACK ] { 97 }
    Constant [ public int CM_PPMD ] { 98 }
    Constant [ public int ER_OK ] { 0 }
    Constant [ public int ER_MULTIDISK ] { 1 }
    Constant [ public int ER_RENAME ] { 2 }
    Constant [ public int ER_CLOSE ] { 3 }
    Constant [ public int ER_SEEK ] { 4 }
    Constant [ public int ER_READ ] { 5 }
    Constant [ public int ER_WRITE ] { 6 }
    Constant [ public int ER_CRC ] { 7 }
    Constant [ public int ER_ZIPCLOSED ] { 8 }
    Constant [ public int ER_NOENT ] { 9 }
    Constant [ public int ER_EXISTS ] { 10 }
    Constant [ public int ER_OPEN ] { 11 }
    Constant [ public int ER_TMPOPEN ] { 12 }
    Constant [ public int ER_ZLIB ] { 13 }
    Constant [ public int ER_MEMORY ] { 14 }
    Constant [ public int ER_CHANGED ] { 15 }
    Constant [ public int ER_COMPNOTSUPP ] { 16 }
    Constant [ public int ER_EOF ] { 17 }
    Constant [ public int ER_INVAL ] { 18 }
    Constant [ public int ER_NOZIP ] { 19 }
    Constant [ public int ER_INTERNAL ] { 20 }
    Constant [ public int ER_INCONS ] { 21 }
    Constant [ public int ER_REMOVE ] { 22 }
    Constant [ public int ER_DELETED ] { 23 }
    Constant [ public int ER_ENCRNOTSUPP ] { 24 }
    Constant [ public int ER_RDONLY ] { 25 }
    Constant [ public int ER_NOPASSWD ] { 26 }
    Constant [ public int ER_WRONGPASSWD ] { 27 }
    Constant [ public int ER_OPNOTSUPP ] { 28 }
    Constant [ public int ER_INUSE ] { 29 }
    Constant [ public int ER_TELL ] { 30 }
    Constant [ public int ER_COMPRESSED_DATA ] { 31 }
    Constant [ public int ER_CANCELLED ] { 32 }
    Constant [ public int AFL_RDONLY ] { 2 }
    Constant [ public int OPSYS_DOS ] { 0 }
    Constant [ public int OPSYS_AMIGA ] { 1 }
    Constant [ public int OPSYS_OPENVMS ] { 2 }
    Constant [ public int OPSYS_UNIX ] { 3 }
    Constant [ public int OPSYS_VM_CMS ] { 4 }
    Constant [ public int OPSYS_ATARI_ST ] { 5 }
    Constant [ public int OPSYS_OS_2 ] { 6 }
    Constant [ public int OPSYS_MACINTOSH ] { 7 }
    Constant [ public int OPSYS_Z_SYSTEM ] { 8 }
    Constant [ public int OPSYS_CPM ] { 9 }
    Constant [ public int OPSYS_WINDOWS_NTFS ] { 10 }
    Constant [ public int OPSYS_MVS ] { 11 }
    Constant [ public int OPSYS_VSE ] { 12 }
    Constant [ public int OPSYS_ACORN_RISC ] { 13 }
    Constant [ public int OPSYS_VFAT ] { 14 }
    Constant [ public int OPSYS_ALTERNATE_MVS ] { 15 }
    Constant [ public int OPSYS_BEOS ] { 16 }
    Constant [ public int OPSYS_TANDEM ] { 17 }
    Constant [ public int OPSYS_OS_400 ] { 18 }
    Constant [ public int OPSYS_OS_X ] { 19 }
    Constant [ public int OPSYS_DEFAULT ] { 3 }
    Constant [ public int EM_NONE ] { 0 }
    Constant [ public int EM_TRAD_PKWARE ] { 1 }
    Constant [ public int EM_AES_128 ] { 257 }
    Constant [ public int EM_AES_192 ] { 258 }
    Constant [ public int EM_AES_256 ] { 259 }
    Constant [ public int EM_UNKNOWN ] { 65535 }
    Constant [ public string LIBZIP_VERSION ] { 1.7.3 }
    Constant [ public int LENGTH_TO_END ] { 0 }
  }

  - Static properties [0] {
  }

  - Static methods [2] {
    Method [ <internal:zip> static public method isCompressionMethodSupported ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $method ]
        Parameter #1 [ <optional> bool $enc = true ]
      }
      - Return [ bool ]
    }

    Method [ <internal:zip> static public method isEncryptionMethodSupported ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $method ]
        Parameter #1 [ <optional> bool $enc = true ]
      }
      - Return [ bool ]
    }
  }

  - Properties [6] {
    Property [ public int $lastId ]
    Property [ public int $status ]
    Property [ public int $statusSys ]
    Property [ public int $numFiles ]
    Property [ public string $filename ]
    Property [ public string $comment ]
  }

  - Methods [50] {
    Method [ <internal:zip> public method open ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $filename ]
        Parameter #1 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ int|bool ]
    }

    Method [ <internal:zip> public method setPassword ] {

      - Parameters [1] {
        Parameter #0 [ <required> string $password ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method close ] {

      - Parameters [0] {
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip, prototype Countable> public method count ] {

      - Parameters [0] {
      }
      - Tentative return [ int ]
    }

    Method [ <internal:zip> public method getStatusString ] {

      - Parameters [0] {
      }
      - Tentative return [ string ]
    }

    Method [ <internal:zip> public method clearError ] {

      - Parameters [0] {
      }
      - Return [ void ]
    }

    Method [ <internal:zip> public method addEmptyDir ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $dirname ]
        Parameter #1 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method addFromString ] {

      - Parameters [3] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <required> string $content ]
        Parameter #2 [ <optional> int $flags = ZipArchive::FL_OVERWRITE ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method addFile ] {

      - Parameters [5] {
        Parameter #0 [ <required> string $filepath ]
        Parameter #1 [ <optional> string $entryname = "" ]
        Parameter #2 [ <optional> int $start = 0 ]
        Parameter #3 [ <optional> int $length = ZipArchive::LENGTH_TO_END ]
        Parameter #4 [ <optional> int $flags = ZipArchive::FL_OVERWRITE ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method replaceFile ] {

      - Parameters [5] {
        Parameter #0 [ <required> string $filepath ]
        Parameter #1 [ <required> int $index ]
        Parameter #2 [ <optional> int $start = 0 ]
        Parameter #3 [ <optional> int $length = ZipArchive::LENGTH_TO_END ]
        Parameter #4 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method addGlob ] {

      - Parameters [3] {
        Parameter #0 [ <required> string $pattern ]
        Parameter #1 [ <optional> int $flags = 0 ]
        Parameter #2 [ <optional> array $options = [] ]
      }
      - Tentative return [ array|false ]
    }

    Method [ <internal:zip> public method addPattern ] {

      - Parameters [3] {
        Parameter #0 [ <required> string $pattern ]
        Parameter #1 [ <optional> string $path = "." ]
        Parameter #2 [ <optional> array $options = [] ]
      }
      - Tentative return [ array|false ]
    }

    Method [ <internal:zip> public method renameIndex ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $index ]
        Parameter #1 [ <required> string $new_name ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method renameName ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <required> string $new_name ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method setArchiveComment ] {

      - Parameters [1] {
        Parameter #0 [ <required> string $comment ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method getArchiveComment ] {

      - Parameters [1] {
        Parameter #0 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ string|false ]
    }

    Method [ <internal:zip> public method setArchiveFlag ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $flag ]
        Parameter #1 [ <required> int $value ]
      }
      - Return [ bool ]
    }

    Method [ <internal:zip> public method getArchiveFlag ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $flag ]
        Parameter #1 [ <optional> int $flags = 0 ]
      }
      - Return [ int ]
    }

    Method [ <internal:zip> public method setCommentIndex ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $index ]
        Parameter #1 [ <required> string $comment ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method setCommentName ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <required> string $comment ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method setMtimeIndex ] {

      - Parameters [3] {
        Parameter #0 [ <required> int $index ]
        Parameter #1 [ <required> int $timestamp ]
        Parameter #2 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method setMtimeName ] {

      - Parameters [3] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <required> int $timestamp ]
        Parameter #2 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method getCommentIndex ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $index ]
        Parameter #1 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ string|false ]
    }

    Method [ <internal:zip> public method getCommentName ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ string|false ]
    }

    Method [ <internal:zip> public method deleteIndex ] {

      - Parameters [1] {
        Parameter #0 [ <required> int $index ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method deleteName ] {

      - Parameters [1] {
        Parameter #0 [ <required> string $name ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method statName ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ array|false ]
    }

    Method [ <internal:zip> public method statIndex ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $index ]
        Parameter #1 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ array|false ]
    }

    Method [ <internal:zip> public method locateName ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ int|false ]
    }

    Method [ <internal:zip> public method getNameIndex ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $index ]
        Parameter #1 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ string|false ]
    }

    Method [ <internal:zip> public method unchangeArchive ] {

      - Parameters [0] {
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method unchangeAll ] {

      - Parameters [0] {
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method unchangeIndex ] {

      - Parameters [1] {
        Parameter #0 [ <required> int $index ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method unchangeName ] {

      - Parameters [1] {
        Parameter #0 [ <required> string $name ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method extractTo ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $pathto ]
        Parameter #1 [ <optional> array|string|null $files = null ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method getFromName ] {

      - Parameters [3] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <optional> int $len = 0 ]
        Parameter #2 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ string|false ]
    }

    Method [ <internal:zip> public method getFromIndex ] {

      - Parameters [3] {
        Parameter #0 [ <required> int $index ]
        Parameter #1 [ <optional> int $len = 0 ]
        Parameter #2 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ string|false ]
    }

    Method [ <internal:zip> public method getStreamIndex ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $index ]
        Parameter #1 [ <optional> int $flags = 0 ]
      }
    }

    Method [ <internal:zip> public method getStreamName ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <optional> int $flags = 0 ]
      }
    }

    Method [ <internal:zip> public method getStream ] {

      - Parameters [1] {
        Parameter #0 [ <required> string $name ]
      }
    }

    Method [ <internal:zip> public method setExternalAttributesName ] {

      - Parameters [4] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <required> int $opsys ]
        Parameter #2 [ <required> int $attr ]
        Parameter #3 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method setExternalAttributesIndex ] {

      - Parameters [4] {
        Parameter #0 [ <required> int $index ]
        Parameter #1 [ <required> int $opsys ]
        Parameter #2 [ <required> int $attr ]
        Parameter #3 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method getExternalAttributesName ] {

      - Parameters [4] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <required> &$opsys ]
        Parameter #2 [ <required> &$attr ]
        Parameter #3 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method getExternalAttributesIndex ] {

      - Parameters [4] {
        Parameter #0 [ <required> int $index ]
        Parameter #1 [ <required> &$opsys ]
        Parameter #2 [ <required> &$attr ]
        Parameter #3 [ <optional> int $flags = 0 ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method setCompressionName ] {

      - Parameters [3] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <required> int $method ]
        Parameter #2 [ <optional> int $compflags = 0 ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method setCompressionIndex ] {

      - Parameters [3] {
        Parameter #0 [ <required> int $index ]
        Parameter #1 [ <required> int $method ]
        Parameter #2 [ <optional> int $compflags = 0 ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method setEncryptionName ] {

      - Parameters [3] {
        Parameter #0 [ <required> string $name ]
        Parameter #1 [ <required> int $method ]
        Parameter #2 [ <optional> ?string $password = null ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method setEncryptionIndex ] {

      - Parameters [3] {
        Parameter #0 [ <required> int $index ]
        Parameter #1 [ <required> int $method ]
        Parameter #2 [ <optional> ?string $password = null ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method registerProgressCallback ] {

      - Parameters [2] {
        Parameter #0 [ <required> float $rate ]
        Parameter #1 [ <required> callable $callback ]
      }
      - Tentative return [ bool ]
    }

    Method [ <internal:zip> public method registerCancelCallback ] {

      - Parameters [1] {
        Parameter #0 [ <required> callable $callback ]
      }
      - Tentative return [ bool ]
    }
  }
}
-- the six are declared, answered and read-only
lastId     type=int    virtual=0 default=0 readonly=0 static=0
status     type=int    virtual=0 default=0 readonly=0 static=0
statusSys  type=int    virtual=0 default=0 readonly=0 static=0
numFiles   type=int    virtual=0 default=0 readonly=0 static=0
filename   type=string virtual=0 default=0 readonly=0 static=0
comment    type=string virtual=0 default=0 readonly=0 static=0
ZipArchive Object
(
    [lastId] => -1
    [status] => 0
    [statusSys] => 0
    [numFiles] => 0
    [filename] => 
    [comment] => 
)
array(6) {
  ["lastId"]=>
  int(-1)
  ["status"]=>
  int(0)
  ["statusSys"]=>
  int(0)
  ["numFiles"]=>
  int(0)
  ["filename"]=>
  string(0) ""
  ["comment"]=>
  string(0) ""
}
string(78) "{"lastId":-1,"status":0,"statusSys":0,"numFiles":0,"filename":"","comment":""}"
array(6) {
  ["lastId"]=>
  int(-1)
  ["status"]=>
  int(0)
  ["statusSys"]=>
  int(0)
  ["numFiles"]=>
  int(0)
  ["filename"]=>
  string(0) ""
  ["comment"]=>
  string(0) ""
}
string(139) "O:10:"ZipArchive":6:{s:6:"lastId";i:-1;s:6:"status";i:0;s:9:"statusSys";i:0;s:8:"numFiles";i:0;s:8:"filename";s:0:"";s:7:"comment";s:0:"";}"
  foreach lastId=-1
  foreach status=0
  foreach statusSys=0
  foreach numFiles=0
  foreach filename=''
  foreach comment=''
Error: Cannot write read-only property ZipArchive::$numFiles
Error: Trying to clone an uncloneable object of class ZipArchive
bool(true)
bool(false)
bool(true)
-- what a build can write
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
string(6) "string"
-- an object nobody opened
string(8) "No error"
close: ValueError: Invalid or uninitialized Zip object
count: ValueError: Invalid or uninitialized Zip object
numFiles: int(0)
bool(true)
