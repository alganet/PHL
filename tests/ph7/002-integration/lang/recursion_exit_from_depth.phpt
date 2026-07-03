--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
exit() from deep recursion unwinds cleanly and still runs shutdown callbacks
--FILE--
<?php
register_shutdown_function(function () {
    echo "shutdown\n";
});
function d(int $n) {
    if ($n === 0) {
        exit("exit@depth\n");
    }
    d($n - 1);
}
d(20);
echo "unreachable\n";
?>
--EXPECT--
exit@depth
shutdown
--CLEAN--
<?php
