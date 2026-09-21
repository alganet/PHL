--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PATHINFO_ALL holds php's value and is the OR of the four components
--FILE--
<?php
echo PATHINFO_ALL . "\n";
var_dump(PATHINFO_ALL === (PATHINFO_DIRNAME | PATHINFO_BASENAME | PATHINFO_EXTENSION | PATHINFO_FILENAME));
?>
--EXPECT--
15
bool(true)
--CLEAN--
<?php
