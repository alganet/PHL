--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
curl_init() answers a CurlHandle, and curl_close() does not close it
--DESCRIPTION--
The handle OBJECT and the verbs that manage it. Two answers here are the
opposite of what the names suggest, and both were measured:

  * curl_close() is a NO-OP. php 8 made the handle an object, and the object's
    own teardown is what frees it -- so after curl_close() the handle still
    works: the setters answer true, a transfer still runs, the reporters still
    report. Freeing there (which is what php 7's resource did) would turn every
    line after a close into a use-after-free on a script php runs happily.

  * curl_reset() does NOT clear the error state. It puts every OPTION back to
    its default and leaves curl_errno() reporting the last transfer's failure;
    only the option setters clear it. Probing the verbs one at a time is what
    shows this -- a sweep that resets after a setopt sees a cleared errno and
    credits the wrong verb.

var_dump of a handle lives in 002-integration instead: it prints an object
HANDLE, and in the shared in-process interpreter that number counts every
object every earlier test made.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
$h = curl_init();
var_dump(get_class($h), gettype($h), $h instanceof CurlHandle, is_object($h), is_resource($h));

// php presents NOTHING on a handle: no property on any surface.
print_r($h);
echo "\n";
var_export($h);
echo "\n";
var_dump((array)$h, get_object_vars($h), json_encode($h));
var_dump(curl_errno($h), curl_error($h));

echo "== the url argument ==\n";
var_dump(curl_init('http://example.com/') instanceof CurlHandle);
var_dump(curl_init(null) instanceof CurlHandle);
var_dump(curl_init('') instanceof CurlHandle);
// the scheme is not validated at init: an unknown one is a transfer-time error
var_dump(curl_init('nonsense://x') instanceof CurlHandle);
// ... but an embedded NUL is refused before libcurl, which takes a C string.
// The sentence says "cURL option" even though the argument is $url.
try { curl_init("\0bad"); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { curl_init([]); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { curl_init(1, 2); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "== close is a no-op ==\n";
$c = curl_init();
var_dump(@curl_close($c));
var_dump($c instanceof CurlHandle, curl_errno($c), curl_error($c));
var_dump(@curl_close($c), curl_reset($c), curl_errno($c));

echo "== reset ==\n";
$r = curl_init();
var_dump(curl_reset($r), curl_errno($r), curl_error($r));

echo "== refusals ==\n";
foreach ([
    'new CurlHandle' => fn() => new CurlHandle(),
    'serialize' => fn() => serialize(curl_init()),
    'unserialize' => fn() => unserialize('O:10:"CurlHandle":0:{}'),
    'dynamic prop write' => function () { $x = curl_init(); $x->foo = 1; return 'ok'; },
    'reflection newInstance' => fn() => (new ReflectionClass('CurlHandle'))->newInstance(),
] as $label => $fn) {
    try { echo $label, ' => ', var_export($fn(), true), "\n"; }
    catch (Throwable $e) { echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
}

// isInstantiable() is TRUE even though `new` is refused: php puts the refusal
// in the creation step, not in a private constructor.
$rc = new ReflectionClass('CurlHandle');
printf("final=%s instantiable=%s cloneable=%s methods=%d props=%d consts=%d parent=%s ifaces=%d\n",
    var_export($rc->isFinal(), true), var_export($rc->isInstantiable(), true),
    var_export($rc->isCloneable(), true), count($rc->getMethods()), count($rc->getProperties()),
    count($rc->getConstants()), var_export($rc->getParentClass(), true), count($rc->getInterfaceNames()));

echo "== wrong handle type ==\n";
foreach (['null' => null, 'int' => 1, 'string' => 'x', 'array' => [], 'stdClass' => new stdClass] as $l => $v) {
    try { curl_errno($v); echo "$l => no throw\n"; }
    catch (Throwable $ex) { echo "$l => ", get_class($ex), ': ', $ex->getMessage(), "\n"; }
}
foreach (['curl_init', 'curl_close', 'curl_reset', 'curl_errno', 'curl_error'] as $f) {
    $rf = new ReflectionFunction($f);
    $ps = [];
    foreach ($rf->getParameters() as $p) {
        $ps[] = (string)$p->getType() . ' $' . $p->getName()
            . ($p->isDefaultValueAvailable() ? ' = ' . var_export($p->getDefaultValue(), true) : '');
    }
    echo $f, '(', implode(', ', $ps), '): ', (string)$rf->getReturnType(), "\n";
}
--EXPECT--
string(10) "CurlHandle"
string(6) "object"
bool(true)
bool(true)
bool(false)
CurlHandle Object
(
)

\CurlHandle::__set_state(array(
))
array(0) {
}
array(0) {
}
string(2) "{}"
int(0)
string(0) ""
== the url argument ==
bool(true)
bool(true)
bool(true)
bool(true)
ValueError: curl_init(): cURL option must not contain any null bytes
TypeError: curl_init(): Argument #1 ($url) must be of type ?string, array given
ArgumentCountError: curl_init() expects at most 1 argument, 2 given
== close is a no-op ==
NULL
bool(true)
int(0)
string(0) ""
NULL
NULL
int(0)
== reset ==
NULL
int(0)
string(0) ""
== refusals ==
new CurlHandle => new CurlHandle => Error: Cannot directly construct CurlHandle, use curl_init() instead
serialize => serialize => Exception: Serialization of 'CurlHandle' is not allowed
unserialize => unserialize => Exception: Unserialization of 'CurlHandle' is not allowed
dynamic prop write => dynamic prop write => Error: Cannot create dynamic property CurlHandle::$foo
reflection newInstance => reflection newInstance => Error: Cannot directly construct CurlHandle, use curl_init() instead
final=true instantiable=true cloneable=true methods=0 props=0 consts=0 parent=false ifaces=0
== wrong handle type ==
null => TypeError: curl_errno(): Argument #1 ($handle) must be of type CurlHandle, null given
int => TypeError: curl_errno(): Argument #1 ($handle) must be of type CurlHandle, int given
string => TypeError: curl_errno(): Argument #1 ($handle) must be of type CurlHandle, string given
array => TypeError: curl_errno(): Argument #1 ($handle) must be of type CurlHandle, array given
stdClass => TypeError: curl_errno(): Argument #1 ($handle) must be of type CurlHandle, stdClass given
curl_init(?string $url = NULL): CurlHandle|false
curl_close(CurlHandle $handle): void
curl_reset(CurlHandle $handle): void
curl_errno(CurlHandle $handle): int
curl_error(CurlHandle $handle): string
