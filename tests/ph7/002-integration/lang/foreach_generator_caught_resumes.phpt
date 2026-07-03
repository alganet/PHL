--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An exception escaping a generator body during foreach is catchable by the enclosing try
--FILE--
<?php
function t(): Generator {
    yield 1;
    throw new RuntimeException("boom");
}
try {
    foreach (t() as $v) {
        echo "v=", $v, "\n";
    }
    echo "not-reached\n";
} catch (RuntimeException $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
function rt(): Generator {
    throw new LogicException("at-rewind");
    yield 1;
}
try {
    foreach (rt() as $v) {
        echo "v=", $v, "\n";
    }
} catch (LogicException $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
echo "after\n";
?>
--EXPECT--
v=1
caught: boom
caught: at-rewind
after
--CLEAN--
<?php
