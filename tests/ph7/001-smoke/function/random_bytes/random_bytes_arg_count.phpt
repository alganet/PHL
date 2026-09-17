--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_bytes throws ArgumentCountError when called with wrong arity

--FILE--
<?php
try { random_bytes(); echo "n0\n"; } catch (ArgumentCountError $e) { echo "ace0\n"; }
try { random_bytes(8, 9); echo "n2\n"; } catch (ArgumentCountError $e) { echo "ace2\n"; }
?>
--EXPECT--
ace0
ace2
--CLEAN--
<?php
