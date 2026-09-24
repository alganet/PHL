--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: php://filter/…/resource=… — the URL form of the chain
--DESCRIPTION--
The URL form is how a filter reaches an opener that knows nothing about filters:
file_get_contents(), include, fopen(). php://filter is not a stream of its own —
it OPENS the stream named by `resource=` and wraps a chain around it, so what a
script gets back reports the wrapper as PHP and the ops of whatever is
underneath. The parse is php's, quirks included: `/resource=` cuts the filter
list, but a path that BEGINS with `resource=` takes the resource and leaves the
list alone, so the resource's own path segments are then tried as filter names
and warned about — twice each, which is the URL form's own diagnostic pair.
--FILE--
<?php
$sfuShow = function ($label, $fn) {
    $seen = [];
    set_error_handler(function ($n, $m) use (&$seen) { $seen[] = $m; return true; });
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    echo $label, ' => ', str_replace("\n", '', $out);
    foreach ($seen as $m) { echo ' | ', $m; }
    echo "\n";
};
$sfuFile = tempnam(sys_get_temp_dir(), 'sfu');
file_put_contents($sfuFile, 'abcdefgh');

$sfuShow('read chain', fn() => file_get_contents("php://filter/read=string.toupper/resource=$sfuFile"));
$sfuShow('two filters left to right',
    fn() => file_get_contents("php://filter/read=string.rot13|string.toupper/resource=$sfuFile"));
$sfuShow('a bare list is both chains',
    fn() => file_get_contents("php://filter/string.toupper/resource=$sfuFile"));
$sfuShow('a write chain does not filter this read',
    fn() => file_get_contents("php://filter/write=string.toupper/read=string.rot13/resource=$sfuFile"));
$sfuShow('an empty list', fn() => file_get_contents("php://filter/read=/resource=$sfuFile"));
$sfuShow('a codec', fn() => file_get_contents(
    "php://filter/read=convert.base64-encode/resource=$sfuFile"));
$sfuShow('nested', fn() => file_get_contents(
    "php://filter/read=string.toupper/resource=php://filter/read=string.rot13/resource=$sfuFile"));

/* The quirk: no slash before `resource=` means the list is never cut. */
$sfuShow('a name nothing registered', fn() => file_get_contents(
    "php://filter/bogus1/resource=$sfuFile"));
$sfuShow('case matters', fn() => file_get_contents(
    "php://filter/read=STRING.ROT13/resource=$sfuFile"));

/* Writing through the URL. */
$sfuShow('write chain', function () use ($sfuFile) {
    $h = fopen("php://filter/write=string.toupper/resource=$sfuFile", 'w');
    fwrite($h, 'written');
    fclose($h);
    return file_get_contents($sfuFile);
});
$sfuShow('write through a codec', function () use ($sfuFile) {
    $h = fopen("php://filter/write=convert.base64-encode/resource=$sfuFile", 'w');
    fwrite($h, 'abcdefgh');
    fclose($h);
    return file_get_contents($sfuFile);
});

/* What the handle SAYS about itself: php keeps the wrapper it was opened
 * through and the ops of the stream underneath. */
file_put_contents($sfuFile, 'abcdefgh');
$sfuShow('what the handle is', function () use ($sfuFile) {
    $h = fopen("php://filter/read=string.rot13/resource=$sfuFile", 'r');
    $m = stream_get_meta_data($h);
    $r = [$m['wrapper_type'], $m['stream_type'], $m['mode'], $m['seekable'],
          get_resource_type($h), stream_is_local($h), stream_supports_lock($h)];
    fclose($h);
    return $r;
});
$sfuShow('position and size', function () use ($sfuFile) {
    $h = fopen("php://filter/read=string.rot13/resource=$sfuFile", 'r');
    $r = [ftell($h), fread($h, 3), ftell($h), fstat($h)['size']];
    fseek($h, 0);
    $r[] = fread($h, 3);
    rewind($h);
    $r[] = stream_get_contents($h);
    $r[] = feof($h);
    fclose($h);
    return $r;
});

/* include() reads through the chain too — and include_path has nothing to say
 * about a URL, which is what walking it for one used to break. */
$sfuInc = tempnam(sys_get_temp_dir(), 'sfi');
file_put_contents($sfuInc, '<?php echo "INCLUDED\n";');
include "php://filter/read=string.tolower/resource=$sfuInc";

/* A codec breaks the tie between the device's offset and the script's, so the
 * position is what the chain DELIVERED and a relative seek is resolved against
 * that — which is the only model in which fseek(…,0,SEEK_CUR) is a no-op. */
$sfuShow('position under a codec', function () use ($sfuFile) {
    $h = fopen("php://filter/read=convert.base64-encode/resource=$sfuFile", 'r');
    $r = [ftell($h), fread($h, 4), ftell($h)];
    $r[] = fseek($h, 0, SEEK_CUR);
    $r[] = fread($h, 4);
    $r[] = fseek($h, 0);
    $r[] = fread($h, 4);
    fclose($h);
    return $r;
});
$sfuShow('the same on an attached chain', function () use ($sfuFile) {
    $h = fopen($sfuFile, 'r');
    stream_filter_append($h, 'convert.base64-encode', STREAM_FILTER_READ, []);
    $r = [ftell($h), fread($h, 4), ftell($h)];
    $r[] = fseek($h, 0, SEEK_CUR);
    $r[] = fread($h, 4);
    fclose($h);
    return $r;
});
$sfuShow('locking and flushing reach the stream underneath', function () use ($sfuFile) {
    $h = fopen("php://filter/read=string.rot13/resource=$sfuFile", 'r');
    $r = [stream_supports_lock($h), flock($h, LOCK_SH), flock($h, LOCK_UN)];
    fclose($h);
    $w = fopen("php://filter/write=string.toupper/resource=$sfuFile", 'w');
    fwrite($w, 'x');
    $r[] = fflush($w);
    fclose($w);
    return $r;
});

/* A filter added afterwards runs after the ones the URL named. */
$sfuShow('appending to a URL chain', function () use ($sfuFile) {
    $h = fopen("php://filter/read=string.rot13/resource=$sfuFile", 'r');
    stream_filter_append($h, 'string.toupper', STREAM_FILTER_READ);
    $s = stream_get_contents($h);
    fclose($h);
    return $s;
});

@unlink($sfuFile);
@unlink($sfuInc);
?>
--EXPECT--
read chain => 'ABCDEFGH'
two filters left to right => 'NOPQRSTU'
a bare list is both chains => 'ABCDEFGH'
a write chain does not filter this read => 'nopqrstu'
an empty list => 'abcdefgh'
a codec => 'YWJjZGVmZ2g='
nested => 'NOPQRSTU'
a name nothing registered => 'abcdefgh' | file_get_contents(): Unable to locate filter "bogus1" | file_get_contents(): Unable to create filter (bogus1)
case matters => 'abcdefgh' | file_get_contents(): Unable to locate filter "STRING.ROT13" | file_get_contents(): Unable to create filter (STRING.ROT13)
write chain => 'WRITTEN'
write through a codec => 'YWJjZGVmZ2g='
what the handle is => array (  0 => 'PHP',  1 => 'STDIO',  2 => 'r',  3 => true,  4 => 'stream',  5 => true,  6 => true,)
position and size => array (  0 => 0,  1 => 'nop',  2 => 3,  3 => 8,  4 => 'nop',  5 => 'nopqrstu',  6 => true,)
included
position under a codec => array (  0 => 0,  1 => 'YWJj',  2 => 4,  3 => 0,  4 => 'Z2hl',  5 => 0,  6 => 'YWJj',)
the same on an attached chain => array (  0 => 0,  1 => 'YWJj',  2 => 4,  3 => 0,  4 => 'Z2hl',)
locking and flushing reach the stream underneath => array (  0 => true,  1 => true,  2 => true,  3 => true,)
appending to a URL chain => 'K'
