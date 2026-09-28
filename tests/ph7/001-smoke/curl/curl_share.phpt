--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The share handles, and the cache several transfers read together
--DESCRIPTION--
php has TWO share classes and they are not related by inheritance:
curl_share_close() and curl_share_setopt() take a CurlShareHandle and refuse a
CurlSharePersistentHandle by TYPE, which is what keeps a cache meant to outlive
the request out of the reach of the setter that would reconfigure it.

What the surface says:

CURL_LOCK_DATA_PSL is deliberately not among the values asserted here: whether
the library was built with a public-suffix list is a BUILD answer, and the
Windows libcurl says CURLSHE_NOT_BUILT_IN where this box's says OK.

  * curl_share_setopt() hands its value to the library: any int cast applies
    (`"3"` and `3.7` are both CURL_LOCK_DATA_DNS) and libcurl judges the
    result, so a null, a true and an out-of-range number are its
    CURLSHE_BAD_OPTION -- false, with the code left on the handle. An option
    php does not know is a ValueError that leaves the SAME code behind, so the
    throw and the refusal are not alternatives;

  * curl_share_init_persistent() validates element by element, in the array's
    own order, with three different sentences -- a value no int cast applies
    to, a value that is not a CURL_LOCK_DATA_*, and COOKIE, which is refused by
    name because a cookie jar shared across requests would hand one visitor's
    cookies to the next. The set it keeps is normalized: sorted, each name
    once;

  * every call answers its OWN object, so two persistent handles asking for the
    same set are neither identical nor equal;

  * CURLOPT_SHARE is the one option php screens NOTHING for: a share of either
    class is attached and anything else -- a null, an int, an array, another
    CurlHandle -- is taken and dropped in silence, with true answered either
    way.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
$sh = curl_share_init();
var_dump(get_class($sh), $sh instanceof CurlShareHandle, is_object($sh));
print_r($sh);
echo "\n";
var_export($sh);
echo "\n";
var_dump((array) $sh, get_object_vars($sh), json_encode($sh));

$rc = new ReflectionClass('CurlShareHandle');
var_dump($rc->isFinal(), $rc->isInstantiable(), count($rc->getMethods()),
    count($rc->getProperties()), count($rc->getConstants()));

echo "== the refusals ==\n";
foreach ([fn() => new CurlShareHandle(), fn() => clone $sh, fn() => serialize($sh)] as $f) {
    try { $f(); echo "no throw\n"; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

echo "== setopt ==\n";
foreach ([[CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS], [CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE],
          [CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION], [CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT],
          [CURLSHOPT_UNSHARE, CURL_LOCK_DATA_DNS],
          [CURLSHOPT_SHARE, '3'], [CURLSHOPT_SHARE, 3.7],
          [CURLSHOPT_SHARE, 0], [CURLSHOPT_SHARE, 1], [CURLSHOPT_SHARE, 999],
          [CURLSHOPT_SHARE, null], [CURLSHOPT_SHARE, true], [CURLSHOPT_SHARE, [1]],
          [CURLSHOPT_NONE, 3], [999, 3]] as $pair) {
    try {
        printf("%-4s %-10s => %-5s errno=%d\n", $pair[0], str_replace("\n", '', var_export($pair[1], true)),
            var_export(curl_share_setopt($sh, $pair[0], $pair[1]), true), curl_share_errno($sh));
    } catch (Throwable $e) {
        printf("%-4s %-10s => %s: %s errno=%d\n", $pair[0], var_export($pair[1], true),
            get_class($e), $e->getMessage(), curl_share_errno($sh));
    }
}
// close is a no-op: the share still takes options, and keeps its error state
$sh2 = curl_share_init();
curl_share_setopt($sh2, CURLSHOPT_SHARE, 0);
printf("errno before the close=%d\n", curl_share_errno($sh2));
var_dump(@curl_share_close($sh2));
printf("errno after the close=%d, setopt after it=%s\n", curl_share_errno($sh2),
    var_export(curl_share_setopt($sh2, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS), true));
var_dump(@curl_share_close($sh2));

echo "== the persistent one ==\n";
$p = curl_share_init_persistent([CURL_LOCK_DATA_SSL_SESSION, CURL_LOCK_DATA_DNS, CURL_LOCK_DATA_DNS]);
var_dump(get_class($p), $p instanceof CurlShareHandle, $p->options);
print_r($p);
echo "\n";
var_export($p);
echo "\n";
var_dump((array) $p, get_object_vars($p), json_encode($p), isset($p->options));

$rp = new ReflectionProperty('CurlSharePersistentHandle', 'options');
var_dump($rp->isReadOnly(), $rp->isPublic(), (string) $rp->getType());
$rcp = new ReflectionClass('CurlSharePersistentHandle');
var_dump($rcp->isFinal(), $rcp->isInstantiable(), count($rcp->getMethods()),
    count($rcp->getProperties()), $rcp->getParentClass() === false);

// two calls for the same set are two objects, and neither identical nor equal
$q = curl_share_init_persistent([CURL_LOCK_DATA_DNS]);
$r = curl_share_init_persistent([CURL_LOCK_DATA_DNS]);
var_dump($q === $r, $q == $r, $q === $q);

echo "== what the persistent one refuses ==\n";
foreach ([fn() => new CurlSharePersistentHandle(), fn() => clone $p, fn() => serialize($p),
          fn() => curl_share_setopt($p, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS),
          fn() => @curl_share_close($p), fn() => curl_share_errno($p),
          fn() => $p->options = [1], fn() => $p->options[] = 9, fn() => $p->other = 1] as $f) {
    try { $f(); echo "no throw\n"; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

echo "== the option set is validated element by element ==\n";
foreach ([[], [CURL_LOCK_DATA_DNS], [CURL_LOCK_DATA_CONNECT],
          [CURL_LOCK_DATA_COOKIE], [CURL_LOCK_DATA_DNS, CURL_LOCK_DATA_COOKIE],
          [CURL_LOCK_DATA_COOKIE, CURL_LOCK_DATA_DNS], [999, CURL_LOCK_DATA_COOKIE],
          ['a', CURL_LOCK_DATA_COOKIE], [CURL_LOCK_DATA_DNS, 'a'], [[1]], [3.9], ['3'],
          [-1], [null], [true], [1], ['k' => CURL_LOCK_DATA_DNS]] as $set) {
    $label = str_replace("\n", '', var_export($set, true));
    try {
        $h = curl_share_init_persistent($set);
        printf("%-46s => %s\n", $label, json_encode($h->options));
    } catch (Throwable $e) {
        printf("%-46s => %s: %s\n", $label, get_class($e), $e->getMessage());
    }
}

echo "== CURLOPT_SHARE screens nothing ==\n";
$e1 = curl_init();
foreach ([$sh, $p, null, 1, 'x', [1], new stdClass, curl_init(), curl_multi_init()] as $v) {
    printf("%-16s => %s errno=%d\n", is_object($v) ? get_class($v) : var_export($v, true),
        var_export(curl_setopt($e1, CURLOPT_SHARE, $v), true), curl_errno($e1));
}
var_dump(curl_setopt_array($e1, array(CURLOPT_SHARE => $sh)));
// a handle attached to a share keeps it alive, and curl_reset drops it
curl_setopt($e1, CURLOPT_SHARE, curl_share_init());
var_dump(curl_reset($e1), curl_errno($e1));

echo "== the arguments ==\n";
foreach ([null, 1, 'x', curl_init()] as $v) {
    try { curl_share_setopt($v, CURLSHOPT_SHARE, 3); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
foreach ([null, 1, 'x', 1.5, new stdClass] as $v) {
    try { curl_share_init_persistent($v); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
try { curl_share_init(1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { curl_share_setopt($sh, CURLSHOPT_SHARE); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
--EXPECT--
string(15) "CurlShareHandle"
bool(true)
bool(true)
CurlShareHandle Object
(
)

\CurlShareHandle::__set_state(array(
))
array(0) {
}
array(0) {
}
string(2) "{}"
bool(true)
bool(true)
int(0)
int(0)
int(0)
== the refusals ==
Error: Cannot directly construct CurlShareHandle, use curl_share_init() instead
Error: Trying to clone an uncloneable object of class CurlShareHandle
Exception: Serialization of 'CurlShareHandle' is not allowed
== setopt ==
1    3          => true  errno=0
1    2          => true  errno=0
1    4          => true  errno=0
1    5          => true  errno=0
2    3          => true  errno=0
1    '3'        => true  errno=0
1    3.7        => true  errno=0
1    0          => false errno=1
1    1          => false errno=1
1    999        => false errno=1
1    NULL       => false errno=1
1    true       => false errno=1
1    array (  0 => 1,) => false errno=1
0    3          => ValueError: curl_share_setopt(): Argument #2 ($option) is not a valid cURL share option errno=1
999  3          => ValueError: curl_share_setopt(): Argument #2 ($option) is not a valid cURL share option errno=1
errno before the close=1
NULL
errno after the close=1, setopt after it=true
NULL
== the persistent one ==
string(25) "CurlSharePersistentHandle"
bool(false)
array(2) {
  [0]=>
  int(3)
  [1]=>
  int(4)
}
CurlSharePersistentHandle Object
(
    [options] => Array
        (
            [0] => 3
            [1] => 4
        )

)

\CurlSharePersistentHandle::__set_state(array(
   'options' => 
  array (
    0 => 3,
    1 => 4,
  ),
))
array(1) {
  ["options"]=>
  array(2) {
    [0]=>
    int(3)
    [1]=>
    int(4)
  }
}
array(1) {
  ["options"]=>
  array(2) {
    [0]=>
    int(3)
    [1]=>
    int(4)
  }
}
string(17) "{"options":[3,4]}"
bool(true)
bool(true)
bool(true)
string(5) "array"
bool(true)
bool(true)
int(0)
int(1)
bool(true)
bool(false)
bool(false)
bool(true)
== what the persistent one refuses ==
Error: Cannot directly construct CurlSharePersistentHandle, use curl_share_init_persistent() instead
Error: Trying to clone an uncloneable object of class CurlSharePersistentHandle
Exception: Serialization of 'CurlSharePersistentHandle' is not allowed
TypeError: curl_share_setopt(): Argument #1 ($share_handle) must be of type CurlShareHandle, CurlSharePersistentHandle given
TypeError: curl_share_close(): Argument #1 ($share_handle) must be of type CurlShareHandle, CurlSharePersistentHandle given
TypeError: curl_share_errno(): Argument #1 ($share_handle) must be of type CurlShareHandle, CurlSharePersistentHandle given
Error: Cannot modify readonly property CurlSharePersistentHandle::$options
Error: Cannot indirectly modify readonly property CurlSharePersistentHandle::$options
Error: Cannot create dynamic property CurlSharePersistentHandle::$other
== the option set is validated element by element ==
array ()                                       => ValueError: curl_share_init_persistent(): Argument #1 ($share_options) must not be empty
array (  0 => 3,)                              => [3]
array (  0 => 5,)                              => [5]
array (  0 => 2,)                              => ValueError: curl_share_init_persistent(): Argument #1 ($share_options) must not contain CURL_LOCK_DATA_COOKIE because sharing cookies across PHP requests is unsafe
array (  0 => 3,  1 => 2,)                     => ValueError: curl_share_init_persistent(): Argument #1 ($share_options) must not contain CURL_LOCK_DATA_COOKIE because sharing cookies across PHP requests is unsafe
array (  0 => 2,  1 => 3,)                     => ValueError: curl_share_init_persistent(): Argument #1 ($share_options) must not contain CURL_LOCK_DATA_COOKIE because sharing cookies across PHP requests is unsafe
array (  0 => 999,  1 => 2,)                   => ValueError: curl_share_init_persistent(): Argument #1 ($share_options) must contain only CURL_LOCK_DATA_* constants
array (  0 => 'a',  1 => 2,)                   => TypeError: curl_share_init_persistent(): Argument #1 ($share_options) must contain only int values, string given
array (  0 => 3,  1 => 'a',)                   => TypeError: curl_share_init_persistent(): Argument #1 ($share_options) must contain only int values, string given
array (  0 =>   array (    0 => 1,  ),)        => TypeError: curl_share_init_persistent(): Argument #1 ($share_options) must contain only int values, array given
array (  0 => 3.9,)                            => [3]
array (  0 => '3',)                            => [3]
array (  0 => -1,)                             => ValueError: curl_share_init_persistent(): Argument #1 ($share_options) must contain only CURL_LOCK_DATA_* constants
array (  0 => NULL,)                           => ValueError: curl_share_init_persistent(): Argument #1 ($share_options) must contain only CURL_LOCK_DATA_* constants
array (  0 => true,)                           => ValueError: curl_share_init_persistent(): Argument #1 ($share_options) must contain only CURL_LOCK_DATA_* constants
array (  0 => 1,)                              => ValueError: curl_share_init_persistent(): Argument #1 ($share_options) must contain only CURL_LOCK_DATA_* constants
array (  'k' => 3,)                            => [3]
== CURLOPT_SHARE screens nothing ==
CurlShareHandle  => true errno=0
CurlSharePersistentHandle => true errno=0
NULL             => true errno=0
1                => true errno=0
'x'              => true errno=0
array (
  0 => 1,
) => true errno=0
stdClass         => true errno=0
CurlHandle       => true errno=0
CurlMultiHandle  => true errno=0
bool(true)
NULL
int(0)
== the arguments ==
TypeError: curl_share_setopt(): Argument #1 ($share_handle) must be of type CurlShareHandle, null given
TypeError: curl_share_setopt(): Argument #1 ($share_handle) must be of type CurlShareHandle, int given
TypeError: curl_share_setopt(): Argument #1 ($share_handle) must be of type CurlShareHandle, string given
TypeError: curl_share_setopt(): Argument #1 ($share_handle) must be of type CurlShareHandle, CurlHandle given
TypeError: curl_share_init_persistent(): Argument #1 ($share_options) must be of type array, null given
TypeError: curl_share_init_persistent(): Argument #1 ($share_options) must be of type array, int given
TypeError: curl_share_init_persistent(): Argument #1 ($share_options) must be of type array, string given
TypeError: curl_share_init_persistent(): Argument #1 ($share_options) must be of type array, float given
TypeError: curl_share_init_persistent(): Argument #1 ($share_options) must be of type array, stdClass given
ArgumentCountError: curl_share_init() expects exactly 0 arguments, 1 given
ArgumentCountError: curl_share_setopt() expects exactly 3 arguments, 2 given
