--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_classes takes no arguments
--FILE--
<?php
try { spl_classes('x'); } catch (Throwable $splClsErr) { echo get_class($splClsErr), ": ", $splClsErr->getMessage(), "\n"; }
?>
--EXPECT--
ArgumentCountError: spl_classes() expects exactly 0 arguments, 1 given
--CLEAN--
<?php
unset($splClsErr);
