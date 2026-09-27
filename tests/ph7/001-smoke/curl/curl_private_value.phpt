--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
CURLOPT_PRIVATE stores a php VALUE, and CURLINFO_PRIVATE reads that one back
--DESCRIPTION--
The only option that stores a php value rather than something libcurl
understands. libcurl has a private pointer of its own and php never sets it:
the value is kept beside the handle and handed straight back, which is why any
type goes in -- an int, a float, a bool, null, a string, an array, an object --
and comes out as itself, an object by REFERENCE and an array by COPY, exactly
as an ordinary assignment behaves.

Three answers that are only visible from outside: a handle nothing has stored
on answers FALSE (which is how "never set" is told from a stored null), the
value is NOT one of the 41 keys the no-selector curl_getinfo() array carries,
and curl_reset() does not clear it -- the one piece of handle state that
survives a reset, because php resets libcurl's options and this was never one
of them.

A copied handle carries it, as it carries every other piece of php-side state.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
$h = curl_init();
printf("nothing stored: %s\n", var_export(curl_getinfo($h, CURLINFO_PRIVATE), true));

foreach (array(42, 1.5, true, false, null, 'a string', array('a' => 1)) as $v) {
    $set = curl_setopt($h, CURLOPT_PRIVATE, $v);
    printf("%-9s setopt=%-5s back=%s\n", gettype($v), var_export($set, true),
        str_replace("\n", '', var_export(curl_getinfo($h, CURLINFO_PRIVATE), true)));
}

/* an object is the SAME object; an array is a copy */
$o = new stdClass();
$o->n = 1;
curl_setopt($h, CURLOPT_PRIVATE, $o);
$back = curl_getinfo($h, CURLINFO_PRIVATE);
$back->n = 2;
printf("object identity: %s written-through=%d\n", var_export($back === $o, true), $o->n);

$a = array(1);
curl_setopt($h, CURLOPT_PRIVATE, $a);
$a[] = 2;
printf("array copied: %s\n", str_replace("\n", '', var_export(curl_getinfo($h, CURLINFO_PRIVATE), true)));

/* not one of the keys the whole-array form answers */
curl_setopt($h, CURLOPT_PRIVATE, 'tag');
printf("in the array form: %s\n", var_export(array_key_exists('private', curl_getinfo($h)), true));

/* a copy carries it, and curl_reset() does not clear it */
$c = curl_copy_handle($h);
printf("copy: %s\n", var_export(curl_getinfo($c, CURLINFO_PRIVATE), true));
printf("clone: %s\n", var_export(curl_getinfo(clone $h, CURLINFO_PRIVATE), true));
curl_reset($h);
printf("after curl_reset: %s\n", var_export(curl_getinfo($h, CURLINFO_PRIVATE), true));
?>
--EXPECT--
nothing stored: false
integer   setopt=true  back=42
double    setopt=true  back=1.5
boolean   setopt=true  back=true
boolean   setopt=true  back=false
NULL      setopt=true  back=NULL
string    setopt=true  back='a string'
array     setopt=true  back=array (  'a' => 1,)
object identity: true written-through=2
array copied: array (  0 => 1,)
in the array form: false
copy: 'tag'
clone: 'tag'
after curl_reset: 'tag'
--CLEAN--
<?php
unset($h, $c, $o, $a, $v, $set, $back);
?>
