--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/fileinfo: the inventory, the finfo class and every signature
--SKIPIF--
<?php
/* a capability guard: CI's Windows php is built without ext/fileinfo */
if (!extension_loaded('fileinfo')) {
    die("skip this php has no ext/fileinfo\n");
}
--FILE--
<?php
/* The API half of ext/fileinfo, which is php's own and small: six functions,
 * eleven flags, one class with four methods and no properties. The ANSWERS are
 * the test beside this one. */
echo "-- the extension\n";
var_dump(extension_loaded('fileinfo'));
echo implode(', ', get_extension_funcs('fileinfo')), "\n";
$ext = new ReflectionExtension('fileinfo');
echo implode(', ', $ext->getClassNames()), "\n";
foreach ($ext->getConstants() as $name => $value) {
    printf("%-24s %d\n", $name, $value);
}

echo "-- every function's signature\n";
foreach (get_extension_funcs('fileinfo') as $name) {
    echo (new ReflectionFunction($name))->__toString(), "\n";
}

echo "-- the class\n";
$c = new ReflectionClass('finfo');
printf("final=%d abstract=%d instantiable=%d parent=%s interfaces=%d\n",
    (int) $c->isFinal(), (int) $c->isAbstract(), (int) $c->isInstantiable(),
    var_export($c->getParentClass() ? $c->getParentClass()->getName() : null, true),
    count($c->getInterfaceNames()));
printf("properties=%d constants=%d\n", count($c->getProperties()), count($c->getConstants()));
foreach ($c->getMethods() as $m) {
    echo $m->__toString(), "\n";
}

echo "-- an instance shows nothing at all\n";
$finfo = new finfo();
var_dump((array) $finfo, get_object_vars($finfo), $finfo instanceof finfo);
$dump = print_r($finfo, true);
echo str_replace("\n", ' ', $dump), "\n";

echo "-- and it may be neither copied nor serialized\n";
try { clone $finfo; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { serialize($finfo); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "-- a subclass is an ordinary one\n";
class PhltFinfo extends finfo
{
    public function __construct()
    {
        parent::__construct(FILEINFO_MIME_TYPE);
    }
}
$sub = new PhltFinfo();
var_dump($sub instanceof finfo, $sub->buffer('hello'), finfo_buffer($sub, 'hello'));

echo "-- an object nobody constructed has no flags to answer with\n";
$bare = (new ReflectionClass('finfo'))->newInstanceWithoutConstructor();
foreach (['file', 'buffer', 'set_flags'] as $verb) {
    try {
        $bare->$verb('x');
    } catch (Throwable $e) {
        printf("%-12s %s: %s\n", $verb, get_class($e), $e->getMessage());
    }
}

echo "-- finfo_close is deprecated in 8.5 and does nothing\n";
error_reporting(E_ALL);
set_error_handler(function (int $n, string $m): bool { echo "[$n] $m\n"; return true; });
$open = finfo_open(FILEINFO_MIME_TYPE);
var_dump($open instanceof finfo, finfo_close($open), $open->buffer('hello'));
restore_error_handler();
--EXPECT--
-- the extension
bool(true)
finfo_open, finfo_close, finfo_set_flags, finfo_file, finfo_buffer, mime_content_type
finfo
FILEINFO_NONE            0
FILEINFO_SYMLINK         2
FILEINFO_MIME            1040
FILEINFO_MIME_TYPE       16
FILEINFO_MIME_ENCODING   1024
FILEINFO_DEVICES         8
FILEINFO_CONTINUE        32
FILEINFO_PRESERVE_ATIME  128
FILEINFO_RAW             256
FILEINFO_APPLE           2048
FILEINFO_EXTENSION       16777216
-- every function's signature
Function [ <internal:fileinfo> function finfo_open ] {

  - Parameters [2] {
    Parameter #0 [ <optional> int $flags = FILEINFO_NONE ]
    Parameter #1 [ <optional> ?string $magic_database = null ]
  }
  - Return [ finfo|false ]
}

Function [ <internal, deprecated:fileinfo> function finfo_close ] {

  - Parameters [1] {
    Parameter #0 [ <required> finfo $finfo ]
  }
  - Return [ true ]
}

Function [ <internal:fileinfo> function finfo_set_flags ] {

  - Parameters [2] {
    Parameter #0 [ <required> finfo $finfo ]
    Parameter #1 [ <required> int $flags ]
  }
  - Return [ true ]
}

Function [ <internal:fileinfo> function finfo_file ] {

  - Parameters [4] {
    Parameter #0 [ <required> finfo $finfo ]
    Parameter #1 [ <required> string $filename ]
    Parameter #2 [ <optional> int $flags = FILEINFO_NONE ]
    Parameter #3 [ <optional> $context = null ]
  }
  - Return [ string|false ]
}

Function [ <internal:fileinfo> function finfo_buffer ] {

  - Parameters [4] {
    Parameter #0 [ <required> finfo $finfo ]
    Parameter #1 [ <required> string $string ]
    Parameter #2 [ <optional> int $flags = FILEINFO_NONE ]
    Parameter #3 [ <optional> $context = null ]
  }
  - Return [ string|false ]
}

Function [ <internal:fileinfo> function mime_content_type ] {

  - Parameters [1] {
    Parameter #0 [ <required> $filename ]
  }
  - Return [ string|false ]
}

-- the class
final=0 abstract=0 instantiable=1 parent=NULL interfaces=0
properties=0 constants=0
Method [ <internal:fileinfo, ctor> public method __construct ] {

  - Parameters [2] {
    Parameter #0 [ <optional> int $flags = FILEINFO_NONE ]
    Parameter #1 [ <optional> ?string $magic_database = null ]
  }
}

Method [ <internal:fileinfo> public method file ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $filename ]
    Parameter #1 [ <optional> int $flags = FILEINFO_NONE ]
    Parameter #2 [ <optional> $context = null ]
  }
  - Tentative return [ string|false ]
}

Method [ <internal:fileinfo> public method buffer ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $string ]
    Parameter #1 [ <optional> int $flags = FILEINFO_NONE ]
    Parameter #2 [ <optional> $context = null ]
  }
  - Tentative return [ string|false ]
}

Method [ <internal:fileinfo> public method set_flags ] {

  - Parameters [1] {
    Parameter #0 [ <required> int $flags ]
  }
  - Tentative return [ true ]
}

-- an instance shows nothing at all
array(0) {
}
array(0) {
}
bool(true)
finfo Object ( ) 
-- and it may be neither copied nor serialized
Error: Trying to clone an uncloneable object of class finfo
Exception: Serialization of 'finfo' is not allowed
-- a subclass is an ordinary one
bool(true)
string(10) "text/plain"
string(10) "text/plain"
-- an object nobody constructed has no flags to answer with
file         Error: Invalid finfo object
buffer       Error: Invalid finfo object
set_flags    TypeError: finfo::set_flags(): Argument #1 ($flags) must be of type int, string given
-- finfo_close is deprecated in 8.5 and does nothing
[8192] Function finfo_close() is deprecated since 8.5, as finfo objects are freed automatically
bool(true)
bool(true)
string(10) "text/plain"
