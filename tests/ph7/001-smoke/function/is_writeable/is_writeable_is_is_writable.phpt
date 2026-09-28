--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_writeable answers what is_writable answers and names itself
--FILE--
<?php
foreach ([__FILE__, __DIR__, '/no/such/path/at/all'] as $wrPath) {
    var_dump(is_writeable($wrPath) === is_writable($wrPath));
}
try { is_writeable([]); } catch (Throwable $wrErr) { echo get_class($wrErr), ": ", $wrErr->getMessage(), "\n"; }
try { is_writeable(); } catch (Throwable $wrErr) { echo get_class($wrErr), ": ", $wrErr->getMessage(), "\n"; }
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
TypeError: is_writeable(): Argument #1 ($filename) must be of type string, array given
ArgumentCountError: is_writeable() expects exactly 1 argument, 0 given
--CLEAN--
<?php
unset($wrPath, $wrErr);
