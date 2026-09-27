--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A transfer has ONE body destination, and the last option to name it wins
--DESCRIPTION--
CURLOPT_RETURNTRANSFER and CURLOPT_WRITEFUNCTION are not two independent
settings: php keeps a single destination on the handle and both of them write
it, so which one was set LAST is the whole answer. That is visible four ways,
and reading the two as separate flags gets every one of them wrong:

  * a handle carrying both answers TRUE, not the empty buffer that nothing
    filled -- the callback took the body and RETURNTRANSFER's buffer never saw
    a byte;
  * CURLOPT_RETURNTRANSFER set AFTER the callback takes the body back off it,
    so the callback is never called and the bytes are answered;
  * a WRITEFUNCTION of null does not restore whatever RETURNTRANSFER last said;
    it lands on the DEFAULT, which is the script's own output;
  * curl_reset() puts it back to that default too, and drops the retained
    callables with it -- they are php's own state, not libcurl's, and "every
    option goes back to its default" covers them.

A copied handle carries the destination and the callables, for the same reason
it carries the slists: they are state the duplicate is expected to have, and
`curl_easy_duphandle` cannot know about any of it.

file:// is enough for all of it -- no socket, and a body of known length.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
$curlDestTmp = tempnam(sys_get_temp_dir(), 'curldest');
file_put_contents($curlDestTmp, "body\n");
$curlDestUrl = 'file://' . (DIRECTORY_SEPARATOR === '\\'
    ? '/' . str_replace('\\', '/', $curlDestTmp) : $curlDestTmp);

$curlDestSink = static function ($handle, $chunk) {
    echo '[write ', strlen($chunk), ']';
    return strlen($chunk);
};
$curlDestRun = static function (string $label, $h) {
    ob_start();
    $r = curl_exec($h);
    $printed = ob_get_clean();
    printf("%-24s => %s printed=%s\n", $label, var_export($r, true), var_export($printed, true));
};

// 1. both set, RETURNTRANSFER first: the callback owns the body
$h = curl_init($curlDestUrl);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_WRITEFUNCTION, $curlDestSink);
$curlDestRun('return then callback', $h);

// 2. the other order: RETURNTRANSFER takes it back
$h = curl_init($curlDestUrl);
curl_setopt($h, CURLOPT_WRITEFUNCTION, $curlDestSink);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
$curlDestRun('callback then return', $h);

// 3. a null callback lands on the default, not on the previous setting
$h = curl_init($curlDestUrl);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_WRITEFUNCTION, $curlDestSink);
curl_setopt($h, CURLOPT_WRITEFUNCTION, null);
$curlDestRun('callback then null', $h);

// 4. RETURNTRANSFER false is the same default
$h = curl_init($curlDestUrl);
curl_setopt($h, CURLOPT_WRITEFUNCTION, $curlDestSink);
curl_setopt($h, CURLOPT_RETURNTRANSFER, false);
$curlDestRun('callback then no-return', $h);

// 5. curl_reset() drops the destination AND the callables
$h = curl_init($curlDestUrl);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_WRITEFUNCTION, $curlDestSink);
curl_setopt($h, CURLOPT_HEADERFUNCTION, static function ($handle, $line) {
    echo '[header]';
    return strlen($line);
});
curl_reset($h);
curl_setopt($h, CURLOPT_URL, $curlDestUrl);
$curlDestRun('after curl_reset', $h);

// 6. a copy carries both
$h = curl_init($curlDestUrl);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
$curlDestRun('copy of a returner', curl_copy_handle($h));

$h = curl_init($curlDestUrl);
curl_setopt($h, CURLOPT_WRITEFUNCTION, $curlDestSink);
$curlDestRun('copy of a callback', curl_copy_handle($h));
$curlDestRun('clone of a callback', clone $h);

// 7. and the copy's callable outlives the source
$h = curl_init($curlDestUrl);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_WRITEFUNCTION, $curlDestSink);
$curlDestCopy = curl_copy_handle($h);
$h = null;
$curlDestRun('source gone', $curlDestCopy);

unlink($curlDestTmp);
?>
--EXPECT--
return then callback     => true printed='[write 5]'
callback then return     => 'body
' printed=''
callback then null       => true printed='body
'
callback then no-return  => true printed='body
'
after curl_reset         => true printed='body
'
copy of a returner       => 'body
' printed=''
copy of a callback       => true printed='[write 5]'
clone of a callback      => true printed='[write 5]'
source gone              => true printed='[write 5]'
--CLEAN--
<?php
unset($curlDestTmp, $curlDestUrl, $curlDestSink, $curlDestRun, $h, $curlDestCopy);
?>
