--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
true/null return types reject a wrong value with a catchable TypeError (PHP 8.2)
--FILE--
<?php
function rtRejTrue(): true { return false; }
function rtRejNull(): null { return 1; }
try { rtRejTrue(); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { rtRejNull(); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
rtRejTrue(): Return value must be of type true, false returned
rtRejNull(): Return value must be of type null, int returned
--CLEAN--
<?php
