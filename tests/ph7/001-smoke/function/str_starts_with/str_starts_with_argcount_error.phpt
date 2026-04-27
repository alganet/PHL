--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_starts_with throws ArgumentCountError on wrong arg count
--FILE--
<?php
try { str_starts_with(); } catch (ArgumentCountError $e) { echo "0:" . $e->getMessage() . "\n"; }
try { str_starts_with("a"); } catch (ArgumentCountError $e) { echo "1:" . $e->getMessage() . "\n"; }
try { str_starts_with("a", "b", "c"); } catch (ArgumentCountError $e) { echo "3:" . $e->getMessage() . "\n"; }
?>
--EXPECT--
0:str_starts_with() expects exactly 2 arguments, 0 given
1:str_starts_with() expects exactly 2 arguments, 1 given
3:str_starts_with() expects exactly 2 arguments, 3 given
--CLEAN--
<?php

