--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
// php 8.3 deprecates assert_options() AND the ASSERT_* constants; PHL emits
// neither deprecation yet (recorded, NEWPLAN section 7), so the oracle's output
// carries extra E_DEPRECATED lines. Un-skip once the deprecations land.
if (function_exists('zend_version')) { echo 'skip php 8.3 deprecates assert_options; PHL does not yet'; }
?>
--TEST--
assert_options query ASSERT_ACTIVE returns 1 by default

--FILE--
<?php
error_reporting(E_ALL & ~E_DEPRECATED);
echo assert_options(ASSERT_ACTIVE) === 1 ? "OK" : "FAIL";
?>
--EXPECT--
OK
--CLEAN--
<?php

