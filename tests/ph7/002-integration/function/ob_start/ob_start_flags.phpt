--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_start()'s $flags: the PHP_OUTPUT_HANDLER_* constants, and the capability bit each member tests before it touches the buffer
--FILE--
<?php
// The PHP_OUTPUT_HANDLER_* constants were all undefined, so `ob_start($cb, 0,
// PHP_OUTPUT_HANDLER_CLEANABLE)` and the `$phase & PHP_OUTPUT_HANDLER_FINAL`
// test every handler is written around were undefined-constant fatals.
foreach ([
    'PHP_OUTPUT_HANDLER_WRITE', 'PHP_OUTPUT_HANDLER_CONT', 'PHP_OUTPUT_HANDLER_START',
    'PHP_OUTPUT_HANDLER_CLEAN', 'PHP_OUTPUT_HANDLER_FLUSH', 'PHP_OUTPUT_HANDLER_FINAL',
    'PHP_OUTPUT_HANDLER_END', 'PHP_OUTPUT_HANDLER_CLEANABLE', 'PHP_OUTPUT_HANDLER_FLUSHABLE',
    'PHP_OUTPUT_HANDLER_REMOVABLE', 'PHP_OUTPUT_HANDLER_STDFLAGS', 'PHP_OUTPUT_HANDLER_STARTED',
    'PHP_OUTPUT_HANDLER_DISABLED', 'PHP_OUTPUT_HANDLER_PROCESSED',
] as $obf_c) {
    printf("%-32s %s\n", $obf_c, defined($obf_c) ? constant($obf_c) : 'UNDEFINED');
}

// $flags says what may be done to the buffer afterwards, and each member tests
// its own bit: CLEANABLE for ob_clean(), FLUSHABLE for ob_flush(), REMOVABLE for
// the four that take the buffer away. The argument was declared in the signature
// and read by nothing, so a buffer opened as un-removable -- the way a framework
// pins its own output layer in place -- could be torn out by any library that
// called ob_end_clean(). A refused operation does not run the handler either, and
// leaves the buffer exactly as it was.
//
// A buffer without REMOVABLE can never be closed, so each case reports its level
// RELATIVE to the level it started at and the un-closable ones simply stay --
// php flushes them at shutdown, innermost first, which is the tail of this
// output.
function obf_handler($obf_b, $obf_p) { return "<$obf_b>"; }
$obf_err = [];
set_error_handler(function ($n, $m) use (&$obf_err) { $obf_err[] = $m; return true; });
$obf_out = [];
$obf_sets = [
    'none' => 0,
    'C'    => PHP_OUTPUT_HANDLER_CLEANABLE,
    'F'    => PHP_OUTPUT_HANDLER_FLUSHABLE,
    'R'    => PHP_OUTPUT_HANDLER_REMOVABLE,
    'CR'   => PHP_OUTPUT_HANDLER_CLEANABLE | PHP_OUTPUT_HANDLER_REMOVABLE,
    'FR'   => PHP_OUTPUT_HANDLER_FLUSHABLE | PHP_OUTPUT_HANDLER_REMOVABLE,
    'STD'  => PHP_OUTPUT_HANDLER_STDFLAGS,
];
foreach ($obf_sets as $obf_name => $obf_flags) {
    foreach (['ob_clean', 'ob_flush', 'ob_end_clean', 'ob_end_flush', 'ob_get_clean', 'ob_get_flush'] as $obf_op) {
        $obf_err = [];
        $obf_base = ob_get_level();
        ob_start('obf_handler', 0, $obf_flags);
        $obf_ret = $obf_op();
        $obf_out[] = sprintf('%-4s %-13s ret=%-8s level=%+d err=%s',
            $obf_name, $obf_op, var_export($obf_ret, true),
            ob_get_level() - $obf_base, implode(' | ', $obf_err) ?: '-');
    }
}
restore_error_handler();
echo implode("\n", $obf_out), "\n";
?>
--EXPECT--
PHP_OUTPUT_HANDLER_WRITE         0
PHP_OUTPUT_HANDLER_CONT          0
PHP_OUTPUT_HANDLER_START         1
PHP_OUTPUT_HANDLER_CLEAN         2
PHP_OUTPUT_HANDLER_FLUSH         4
PHP_OUTPUT_HANDLER_FINAL         8
PHP_OUTPUT_HANDLER_END           8
PHP_OUTPUT_HANDLER_CLEANABLE     16
PHP_OUTPUT_HANDLER_FLUSHABLE     32
PHP_OUTPUT_HANDLER_REMOVABLE     64
PHP_OUTPUT_HANDLER_STDFLAGS      112
PHP_OUTPUT_HANDLER_STARTED       4096
PHP_OUTPUT_HANDLER_DISABLED      8192
PHP_OUTPUT_HANDLER_PROCESSED     16384
<<<<<<<<<<<<<<><<<<<<<<><><<<><><<><<><><<><<><>none ob_clean      ret=false    level=+1 err=ob_clean(): Failed to delete buffer of obf_handler (0)
none ob_flush      ret=false    level=+1 err=ob_flush(): Failed to flush buffer of obf_handler (1)
none ob_end_clean  ret=false    level=+1 err=ob_end_clean(): Failed to discard buffer of obf_handler (2)
none ob_end_flush  ret=false    level=+1 err=ob_end_flush(): Failed to send buffer of obf_handler (3)
none ob_get_clean  ret=''       level=+1 err=ob_get_clean(): Failed to discard buffer of obf_handler (4) | ob_get_clean(): Failed to delete buffer of obf_handler (4)
none ob_get_flush  ret=''       level=+1 err=ob_get_flush(): Failed to send buffer of obf_handler (5) | ob_get_flush(): Failed to delete buffer of obf_handler (5)
C    ob_clean      ret=true     level=+1 err=-
C    ob_flush      ret=false    level=+1 err=ob_flush(): Failed to flush buffer of obf_handler (7)
C    ob_end_clean  ret=false    level=+1 err=ob_end_clean(): Failed to discard buffer of obf_handler (8)
C    ob_end_flush  ret=false    level=+1 err=ob_end_flush(): Failed to send buffer of obf_handler (9)
C    ob_get_clean  ret=''       level=+1 err=ob_get_clean(): Failed to discard buffer of obf_handler (10) | ob_get_clean(): Failed to delete buffer of obf_handler (10)
C    ob_get_flush  ret=''       level=+1 err=ob_get_flush(): Failed to send buffer of obf_handler (11) | ob_get_flush(): Failed to delete buffer of obf_handler (11)
F    ob_clean      ret=false    level=+1 err=ob_clean(): Failed to delete buffer of obf_handler (12)
F    ob_flush      ret=true     level=+1 err=-
F    ob_end_clean  ret=false    level=+1 err=ob_end_clean(): Failed to discard buffer of obf_handler (14)
F    ob_end_flush  ret=false    level=+1 err=ob_end_flush(): Failed to send buffer of obf_handler (15)
F    ob_get_clean  ret=''       level=+1 err=ob_get_clean(): Failed to discard buffer of obf_handler (16) | ob_get_clean(): Failed to delete buffer of obf_handler (16)
F    ob_get_flush  ret=''       level=+1 err=ob_get_flush(): Failed to send buffer of obf_handler (17) | ob_get_flush(): Failed to delete buffer of obf_handler (17)
R    ob_clean      ret=false    level=+1 err=ob_clean(): Failed to delete buffer of obf_handler (18)
R    ob_flush      ret=false    level=+1 err=ob_flush(): Failed to flush buffer of obf_handler (19)
R    ob_end_clean  ret=true     level=+0 err=-
R    ob_end_flush  ret=true     level=+0 err=-
R    ob_get_clean  ret=''       level=+0 err=-
R    ob_get_flush  ret=''       level=+0 err=-
CR   ob_clean      ret=true     level=+1 err=-
CR   ob_flush      ret=false    level=+1 err=ob_flush(): Failed to flush buffer of obf_handler (21)
CR   ob_end_clean  ret=true     level=+0 err=-
CR   ob_end_flush  ret=true     level=+0 err=-
CR   ob_get_clean  ret=''       level=+0 err=-
CR   ob_get_flush  ret=''       level=+0 err=-
FR   ob_clean      ret=false    level=+1 err=ob_clean(): Failed to delete buffer of obf_handler (22)
FR   ob_flush      ret=true     level=+1 err=-
FR   ob_end_clean  ret=true     level=+0 err=-
FR   ob_end_flush  ret=true     level=+0 err=-
FR   ob_get_clean  ret=''       level=+0 err=-
FR   ob_get_flush  ret=''       level=+0 err=-
STD  ob_clean      ret=true     level=+1 err=-
STD  ob_flush      ret=true     level=+1 err=-
STD  ob_end_clean  ret=true     level=+0 err=-
STD  ob_end_flush  ret=true     level=+0 err=-
STD  ob_get_clean  ret=''       level=+0 err=-
STD  ob_get_flush  ret=''       level=+0 err=-
>>>>>>>>>>>>>>>>>>>>>>>>>>