--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_start() from inside an output handler is refused (php member: php's own wording, "Fatal error" label and stack trace)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
// A handler runs while the buffer stack is mid-operation, so php refuses to open
// another one from inside it. PHL used to allow it: the handler re-entered itself
// through its own echo up to 15 levels deep, and the buffers it opened were left
// behind pointing into a set the next ob_start() could realloc.
ob_start(function ($obre_b, $obre_p) {
    for ($obre_i = 0; $obre_i < 40; $obre_i++) {
        ob_start();
    }
    return "<$obre_b>";
});
echo "x";
ob_end_flush();
echo "NOT REACHED\n";
?>
--EXPECTF--
PHP Fatal error:  ob_start(): Cannot use output buffering in output buffering display handlers in %s on line 8
Stack trace:
#0 %s(8): ob_start()
#1 [internal function]: {closure:%s:6}()
#2 %s(13): ob_end_flush()
#3 {main}
