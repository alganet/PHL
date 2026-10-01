--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a removed filter's memory goes back on its last reference
--DESCRIPTION--
stream_filter_remove() takes the filter off its chain, but the instance can only
be handed back once nothing can still reach it: a value naming the handle keeps
it alive and readable as a closed resource, and so does a brigade handle a
userland filter() kept past the call it was given in. A loop of append/remove
pairs must therefore cost nothing that lasts.
--FILE--
<?php
$f = fopen('php://memory', 'r+');

/* Removed, but still named: a closed resource, exactly as php reports one. */
$h = stream_filter_append($f, 'string.toupper', STREAM_FILTER_WRITE);
stream_filter_remove($h);
var_dump(is_resource($h), get_resource_type($h));
$copy = $h;
unset($h);
var_dump(is_resource($copy));
unset($copy);

/* 5000 append/remove pairs, and nothing accumulates. */
$base = memory_get_usage();
for ($i = 0; $i < 5000; $i++) {
    $g = stream_filter_append($f, 'string.toupper', STREAM_FILTER_WRITE);
    stream_filter_remove($g);
}
unset($g);
var_dump(memory_get_usage() - $base < 65536);

/* And a filter that is still attached still filters. */
$w = stream_filter_append($f, 'string.toupper', STREAM_FILTER_WRITE);
fwrite($f, 'still filtering');
stream_filter_remove($w);
rewind($f);
var_dump(stream_get_contents($f));
fclose($f);

/* A userland filter whose $in outlives the call that handed it over. */
class KeepBrigade extends php_user_filter
{
    public static $in = null;
    public function filter($in, $out, &$consumed, $closing): int
    {
        self::$in = $in;
        while ($bucket = stream_bucket_make_writeable($in)) {
            $bucket->data = strrev($bucket->data);
            $consumed += $bucket->datalen;
            stream_bucket_append($out, $bucket);
        }
        return PSFS_PASS_ON;
    }
}
stream_filter_register('keep.brigade', 'KeepBrigade');
$m = fopen('php://memory', 'r+');
$k = stream_filter_append($m, 'keep.brigade', STREAM_FILTER_WRITE);
fwrite($m, 'abcdef');
stream_filter_remove($k);
unset($k);
rewind($m);
var_dump(stream_get_contents($m));
var_dump(is_resource(KeepBrigade::$in));
KeepBrigade::$in = null;
fclose($m);
?>
--EXPECT--
bool(false)
string(7) "Unknown"
bool(false)
bool(true)
string(15) "STILL FILTERING"
string(6) "fedcba"
bool(true)
