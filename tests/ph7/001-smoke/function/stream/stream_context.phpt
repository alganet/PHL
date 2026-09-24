--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a stream context is a resource, and what it carries
--DESCRIPTION--
stream_context_create() used to hand back the options ARRAY itself: is_resource()
was false, get_resource_type() had nothing to name, and every accessor around it
(get_options/set_option/set_options/get_params/set_params/get_default/set_default)
was a Call to undefined function. This is the store php keeps — a wrapper =>
option => value map in insertion order plus the `notification` parameter — and
the shapes it refuses.
--FILE--
<?php
$sctxShow = function ($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
};

/* It is a resource, and php names it. */
$sctxA = stream_context_create(['http' => ['method' => 'POST', 'timeout' => 3]]);
$sctxShow('is_resource', fn() => is_resource($sctxA));
$sctxShow('type', fn() => get_resource_type($sctxA));
$sctxShow('debug_type', fn() => get_debug_type($sctxA));
$sctxShow('two are distinct', fn() => stream_context_create() == stream_context_create());

/* The map answers in INSERTION order, both levels. */
$sctxShow('options', fn() => stream_context_get_options($sctxA));

/* Every entry must be wrappername => array. An integer key has no wrapper name
 * at all, so it is the same refusal as a non-array value; an integer key one
 * level DOWN has no option name and php drops that entry in silence. */
$sctxShow('value not an array', fn() => stream_context_create(['http' => 'x']));
$sctxShow('integer wrapper key', fn() => stream_context_create([5 => ['a' => 1]]));
$sctxShow('integer option key', fn() => stream_context_get_options(
    stream_context_create(['http' => [5 => 'dropped', 'a' => 2]])));

/* Params: only `notification` and `options` are read, anything else ignored. */
$sctxShow('params default', fn() => stream_context_get_params($sctxA));
$sctxShow('params keys with notification', fn() => array_keys(stream_context_get_params(
    stream_context_create([], ['notification' => 'printf']))));
$sctxShow('params unknown key ignored', fn() => stream_context_get_params(
    stream_context_create([], ['nosuchparam' => 1])));
$sctxShow('params carry options', fn() => stream_context_get_options(
    stream_context_create([], ['options' => ['ftp' => ['z' => 2]]])));
$sctxShow('notification must be callable', fn() => stream_context_create(
    [], ['notification' => 'sctx_no_such_function']));

/* The setters. */
$sctxShow('set_option', function () {
    $c = stream_context_create();
    $r = stream_context_set_option($c, 'http', 'method', 'PUT');
    return [$r, stream_context_get_options($c)];
});
$sctxShow('set_options', function () {
    $c = stream_context_create(['http' => ['a' => 1]]);
    $r = stream_context_set_options($c, ['http' => ['b' => 2], 'file' => ['c' => 3]]);
    return [$r, stream_context_get_options($c)];
});
$sctxShow('set_params', function () {
    $c = stream_context_create();
    $r = stream_context_set_params($c, ['notification' => 'printf', 'options' => ['s' => ['k' => 'v']]]);
    return [$r, array_keys(stream_context_get_params($c)), stream_context_get_options($c)];
});

/* Setting an option must not write through into the array it was built from. */
$sctxShow('source array untouched', function () {
    $src = ['http' => ['a' => 1]];
    $c = stream_context_create($src);
    stream_context_set_option($c, 'http', 'b', 2);
    return [$src, stream_context_get_options($c)];
});

/* There is exactly ONE default context, and BOTH calls merge into it. */
$sctxShow('default is one resource', fn() => stream_context_get_default() === stream_context_get_default());
$sctxShow('get_default merges', function () {
    stream_context_get_default(['file' => ['x' => 1]]);
    return stream_context_get_options(stream_context_get_default());
});
$sctxShow('set_default merges too', function () {
    $r = stream_context_set_default(['file' => ['y' => 2]]);
    return [get_resource_type($r), stream_context_get_options(stream_context_get_default())];
});

/* A STREAM is the other thing every accessor takes. A live one that was never
 * given a context answers the empty option set rather than refusing the call,
 * and a set_option on it creates one. */
$sctxPath = sys_get_temp_dir() . '/phl_sctx_' . getmypid() . '.txt';
file_put_contents($sctxPath, "hi\n");
$sctxH = fopen($sctxPath, 'r');
$sctxShow('stream with no context', fn() => stream_context_get_options($sctxH));
$sctxShow('set_option on a stream', function () use ($sctxH) {
    stream_context_set_option($sctxH, 'file', 'k', 'v');
    return stream_context_get_options($sctxH);
});
fclose($sctxH);
$sctxShow('closed stream', fn() => stream_context_get_options($sctxH));
$sctxShow('not a resource', fn() => stream_context_get_options([]));
unlink($sctxPath);
?>
--EXPECT--
is_resource => true
type => 'stream-context'
debug_type => 'resource (stream-context)'
two are distinct => false
options => array (  'http' =>   array (    'method' => 'POST',    'timeout' => 3,  ),)
value not an array => ValueError: Options should have the form ["wrappername"]["optionname"] = $value
integer wrapper key => ValueError: Options should have the form ["wrappername"]["optionname"] = $value
integer option key => array (  'http' =>   array (    'a' => 2,  ),)
params default => array (  'options' =>   array (    'http' =>     array (      'method' => 'POST',      'timeout' => 3,    ),  ),)
params keys with notification => array (  0 => 'notification',  1 => 'options',)
params unknown key ignored => array (  'options' =>   array (  ),)
params carry options => array (  'ftp' =>   array (    'z' => 2,  ),)
notification must be callable => TypeError: stream_context_create(): Argument #1 ($options) must be an array with valid callbacks as values, function "sctx_no_such_function" not found or invalid function name
set_option => array (  0 => true,  1 =>   array (    'http' =>     array (      'method' => 'PUT',    ),  ),)
set_options => array (  0 => true,  1 =>   array (    'http' =>     array (      'a' => 1,      'b' => 2,    ),    'file' =>     array (      'c' => 3,    ),  ),)
set_params => array (  0 => true,  1 =>   array (    0 => 'notification',    1 => 'options',  ),  2 =>   array (    's' =>     array (      'k' => 'v',    ),  ),)
source array untouched => array (  0 =>   array (    'http' =>     array (      'a' => 1,    ),  ),  1 =>   array (    'http' =>     array (      'a' => 1,      'b' => 2,    ),  ),)
default is one resource => true
get_default merges => array (  'file' =>   array (    'x' => 1,  ),)
set_default merges too => array (  0 => 'stream-context',  1 =>   array (    'file' =>     array (      'x' => 1,      'y' => 2,    ),  ),)
stream with no context => array ()
set_option on a stream => array (  'file' =>   array (    'k' => 'v',  ),)
closed stream => TypeError: stream_context_get_options(): Argument #1 ($stream_or_context) must be a valid stream/context
not a resource => TypeError: stream_context_get_options(): Argument #1 ($stream_or_context) must be of type resource, array given
