--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_bytes throws ValueError on negative length

--FILE--
<?php
try { random_bytes(-1); echo "no_throw\n"; } catch (ValueError $e) { echo "ve\n"; }
?>
--EXPECT--
ve
--CLEAN--
<?php
