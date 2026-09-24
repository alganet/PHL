--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The read-only ob functions still answer from inside a handler, for the buffer being processed
--FILE--
<?php
// The READ-ONLY ob functions stay usable from inside a handler, and they answer
// for the buffer being processed: its RAW contents (the handler has been handed
// them, but they are still there), its length, and its still-counted level.
// The mutating ones are refused -- that half is its own test, since php ends the
// request over it.
$obro_seen = [];
ob_start();
ob_start(function ($obro_b, $obro_p) use (&$obro_seen) {
    $obro_seen[] = [ob_get_contents(), ob_get_length(), ob_get_level(), ob_list_handlers()];
    return "<$obro_b>";
});
echo "abc";
ob_end_flush();
$obro_outer = ob_get_clean();
var_dump($obro_outer);
var_dump($obro_seen[0][0], $obro_seen[0][1], $obro_seen[0][2], count($obro_seen[0][3]));

// ...and "the buffer being processed" is not always the topmost one: a chunked
// buffer writes out while an inner buffer is open, and php truncates the stack at
// the active one for the duration.
$obro_deep = [];
ob_start(function ($obro_b, $obro_p) use (&$obro_deep) {
    $obro_deep[] = [ob_get_level(), ob_get_contents(), count(ob_list_handlers())];
    return "";
}, 4);
echo "12345";
ob_start();
echo "inner";
ob_end_flush();
ob_end_flush();
print_r($obro_deep);
?>
--EXPECT--
string(5) "<abc>"
string(3) "abc"
int(3)
int(2)
int(2)
Array
(
    [0] => Array
        (
            [0] => 1
            [1] => 12345
            [2] => 1
        )

    [1] => Array
        (
            [0] => 1
            [1] => inner
            [2] => 1
        )

    [2] => Array
        (
            [0] => 1
            [1] => 
            [2] => 1
        )

)
