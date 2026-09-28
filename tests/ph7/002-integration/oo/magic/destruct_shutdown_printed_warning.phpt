--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The shutdown refusal of a non-public __destruct prints Unknown on line 0
--DESCRIPTION--
The printed half of destruct_shutdown_visibility.phpt: with no frame under it
php names neither the file nor a line, and the diagnostic reads
`in Unknown on line 0` rather than pointing at the script.
--FILE--
<?php
class Hidden { private function __destruct() { echo "never\n"; } }
$a = new Hidden();
echo "script-end\n";
?>
--EXPECT--
script-end
--EXPECT_STDERR--
PHP Warning:  Call to private Hidden::__destruct() from global scope during shutdown ignored in Unknown on line 0
