--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload_call refuses an array and counts its arguments
--FILE--
<?php
foreach ([[[1]], [], [1, 2]] as $splCallArg) {
    try {
        var_dump(spl_autoload_call(...$splCallArg));
    } catch (Throwable $splCallErr) {
        echo get_class($splCallErr), ": ", $splCallErr->getMessage(), "\n";
    }
}
?>
--EXPECT--
TypeError: spl_autoload_call(): Argument #1 ($class) must be of type string, array given
ArgumentCountError: spl_autoload_call() expects exactly 1 argument, 0 given
ArgumentCountError: spl_autoload_call() expects exactly 1 argument, 2 given
--CLEAN--
<?php
unset($splCallArg, $splCallErr);
