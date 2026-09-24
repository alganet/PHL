--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the $context argument every opener declares
--DESCRIPTION--
Sixteen rows of aBuiltinSig[] declare `$context` and no C body read one, so a
resource of ANY kind was accepted in silence — a stream, a process handle, an
integer — where php refuses everything that is not a stream-context. This is
that contract, asked of each name that carries the argument. (WHETHER the
operation still happens after the refusal is the twin-paired divergence in
002-integration: php raises the TypeError and acts anyway.)
--FILE--
<?php
$sarg = function ($label, $fn) {
    try { ob_start(); $r = $fn(); ob_end_clean(); $out = var_export($r, true); }
    catch (Throwable $e) { ob_end_clean(); $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
};

$sargDir  = sys_get_temp_dir() . '/phl_sargd_' . getmypid();
@mkdir($sargDir);
$sargPath = $sargDir . '/f.txt';
file_put_contents($sargPath, "one\n");
$sargCtx  = stream_context_create(['file' => ['k' => 'v']]);
$sargNot  = fopen($sargPath, 'r');          /* a resource of another kind */

/* A resource that is not a stream-context: php names the RESOURCE it wanted,
 * not the argument position. Each case gets a scratch path of its own, and the
 * calls are silenced, because in php the operation still HAPPENS before the
 * throw lands (the twin-paired divergence) and a failing one warns. */
$sarg('fopen', fn() => @fopen($sargDir . '/a', 'w', false, $sargNot));
$sarg('file_get_contents', fn() => @file_get_contents($sargPath, false, $sargNot));
$sarg('file_put_contents', fn() => @file_put_contents($sargDir . '/b', 'x', 0, $sargNot));
$sarg('file', fn() => @file($sargPath, 0, $sargNot));
$sarg('readfile', fn() => @readfile($sargPath, false, $sargNot));
$sarg('copy', fn() => @copy($sargPath, $sargDir . '/c', $sargNot));
$sarg('rename', fn() => @rename($sargDir . '/d', $sargDir . '/e', $sargNot));
$sarg('unlink', fn() => @unlink($sargDir . '/g', $sargNot));
$sarg('mkdir', fn() => @mkdir($sargDir . '/h', 0777, false, $sargNot));
$sarg('rmdir', fn() => @rmdir($sargDir . '/i', $sargNot));
$sarg('opendir', fn() => @opendir($sargDir, $sargNot));
$sarg('scandir', fn() => @scandir($sargDir, SCANDIR_SORT_ASCENDING, $sargNot));
$sarg('hash_update_file', fn() => hash_update_file(hash_init('md5'), $sargPath, $sargNot));

/* Anything else non-null is the ordinary type refusal, and it names the
 * argument. */
$sarg('fopen string', fn() => fopen($sargPath, 'r', false, 'ctx'));
$sarg('unlink int', fn() => @unlink($sargDir . '/j', 7));
$sarg('file_get_contents array', fn() => file_get_contents($sargPath, false, []));
$sarg('scandir float', fn() => scandir($sargDir, SCANDIR_SORT_ASCENDING, 1.5));

/* NULL is "no context of my own" and is the documented default. A real one is
 * simply accepted — nothing about the open changes for a plain file, which is
 * also why php reports the empty option set for the handle it produced. */
$sarg('fopen null ctx', function () use ($sargPath) {
    $h = fopen($sargPath, 'r', false, null);
    $ok = is_resource($h);
    fclose($h);
    return $ok;
});
$sarg('fopen with ctx', function () use ($sargPath, $sargCtx) {
    $h = fopen($sargPath, 'r', false, $sargCtx);
    $ok = [is_resource($h), stream_context_get_options($h)];
    fclose($h);
    return $ok;
});
$sarg('file_get_contents with ctx', fn() => file_get_contents($sargPath, false, $sargCtx));
$sarg('scandir with ctx', fn() => count(scandir($sargDir, SCANDIR_SORT_ASCENDING, $sargCtx)) >= 3);
$sarg('hash_update_file with ctx', function () use ($sargPath, $sargCtx) {
    $h = hash_init('md5');
    hash_update_file($h, $sargPath, $sargCtx);
    return hash_final($h) === md5("one\n");
});

fclose($sargNot);
foreach (['a', 'b', 'c', 'e', 'f.txt'] as $sargLeft) {
    if (is_file($sargDir . '/' . $sargLeft)) { unlink($sargDir . '/' . $sargLeft); }
}
if (is_dir($sargDir . '/h')) { rmdir($sargDir . '/h'); }
rmdir($sargDir);
?>
--EXPECT--
fopen => TypeError: fopen(): supplied resource is not a valid Stream-Context resource
file_get_contents => TypeError: file_get_contents(): supplied resource is not a valid Stream-Context resource
file_put_contents => TypeError: file_put_contents(): supplied resource is not a valid Stream-Context resource
file => TypeError: file(): supplied resource is not a valid Stream-Context resource
readfile => TypeError: readfile(): supplied resource is not a valid Stream-Context resource
copy => TypeError: copy(): supplied resource is not a valid Stream-Context resource
rename => TypeError: rename(): supplied resource is not a valid Stream-Context resource
unlink => TypeError: unlink(): supplied resource is not a valid Stream-Context resource
mkdir => TypeError: mkdir(): supplied resource is not a valid Stream-Context resource
rmdir => TypeError: rmdir(): supplied resource is not a valid Stream-Context resource
opendir => TypeError: opendir(): supplied resource is not a valid Stream-Context resource
scandir => TypeError: scandir(): supplied resource is not a valid Stream-Context resource
hash_update_file => TypeError: hash_update_file(): supplied resource is not a valid Stream-Context resource
fopen string => TypeError: fopen(): Argument #4 ($context) must be of type resource or null, string given
unlink int => TypeError: unlink(): Argument #2 ($context) must be of type resource or null, int given
file_get_contents array => TypeError: file_get_contents(): Argument #3 ($context) must be of type resource or null, array given
scandir float => TypeError: scandir(): Argument #3 ($context) must be of type resource or null, float given
fopen null ctx => true
fopen with ctx => array (  0 => true,  1 =>   array (  ),)
file_get_contents with ctx => 'one'
scandir with ctx => true
hash_update_file with ctx => true
