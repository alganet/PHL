--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
putenv and getenv should set and read environment variables
--FILE--
<?php
$ok = putenv("PH7_UNIT_TEST_TESTER=HELLO_VARS_TEST");
echo "putenv_ok=" . ($ok ? 'true' : 'false') . PHP_EOL;
echo "getenv_val=" . getenv("PH7_UNIT_TEST_TESTER") . PHP_EOL;
// Cleanup
putenv("PH7_UNIT_TEST_TESTER=");
?>
--EXPECT--
putenv_ok=true
getenv_val=HELLO_VARS_TEST
--CLEAN--
<?php
putenv("PH7_UNIT_TEST_TESTER=");
?>
