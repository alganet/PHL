--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_get_flush() SENDS the buffer it answers, a nested flush lands in the buffer below it, and flush() is not ob_flush()
--FILE--
<?php
$obgf_err = [];
$obgf_log = [];
set_error_handler(function ($n, $m) use (&$obgf_err) { $obgf_err[] = $m; return true; });
function obgf_log($obgf_what, ...$obgf_vals) {
    global $obgf_log;
    $obgf_log[] = $obgf_what . ': ' . implode(' ', array_map(fn ($v) => var_export($v, true), $obgf_vals));
}

// ob_get_flush() SENDS the buffer it hands back. It was registered as
// ob_get_clean(), so the string came back and the output it names was thrown
// away -- `$html = ob_get_flush();` printed nothing at all. The send is
// verified by wrapping the call in an OUTER buffer and reading that back.
ob_start();
ob_start();
echo "sent";
$obgf_got = ob_get_flush();
obgf_log('ob_get_flush', $obgf_got, ob_get_level());
obgf_log('outer holds', ob_get_clean());

// A flush lands in the buffer BELOW the flushing one, not on the real output --
// through every spelling.
ob_start();
echo "outer:";
ob_start();
echo "inner";
obgf_log('ob_end_flush', ob_end_flush());
obgf_log('outer holds', ob_get_clean(), ob_get_level());

ob_start();
echo "outer:";
ob_start();
echo "inner";
obgf_log('ob_flush', ob_flush(), ob_get_contents());
obgf_log('outer holds', ob_end_clean(), ob_get_clean());

// flush() is NOT ob_flush(): php flushes the output LAYER and leaves every
// userland buffer alone. It used to be the same C routine, so the
// echo-then-flush() progress idiom emptied a buffer the script meant to read.
ob_start();
echo "kept";
obgf_log('flush', flush(), ob_get_contents());
obgf_log('still there', ob_get_clean());

// ob_clean()/ob_flush() answer a bool, not nothing.
ob_start();
echo "x";
obgf_log('ob_clean', ob_clean(), ob_get_length());
obgf_log('ob_flush', ob_flush(), ob_end_clean());

// With no buffer at all each member answers false, and four of the six report
// it -- ob_get_clean() and ob_get_contents() are php's silent ones.
obgf_log('level', ob_get_level());
obgf_log('empty stack', ob_clean(), ob_flush(), ob_end_clean(), ob_end_flush());
obgf_log('empty stack', ob_get_clean(), ob_get_flush(), ob_get_contents(), ob_get_length());

restore_error_handler();
echo implode("\n", $obgf_log), "\n";
print_r($obgf_err);
?>
--EXPECT--
ob_get_flush: 'sent' 1
outer holds: 'sent'
ob_end_flush: true
outer holds: 'outer:inner' 0
ob_flush: true ''
outer holds: true 'outer:inner'
flush: NULL 'kept'
still there: 'kept'
ob_clean: true 0
ob_flush: true true
level: 0
empty stack: false false false false
empty stack: false false false false
Array
(
    [0] => ob_clean(): Failed to delete buffer. No buffer to delete
    [1] => ob_flush(): Failed to flush buffer. No buffer to flush
    [2] => ob_end_clean(): Failed to delete buffer. No buffer to delete
    [3] => ob_end_flush(): Failed to delete and flush buffer. No buffer to delete or flush
    [4] => ob_get_flush(): Failed to delete and flush buffer. No buffer to delete or flush
)
