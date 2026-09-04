--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: PH7_ENGINE constant value
--SKIPIF--
<?php
// PHL extension: `PH7_ENGINE` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: PH7_ENGINE is not a php symbol'; }
?>
--FILE--
<?php
echo "PH7_ENGINE=" . PH7_ENGINE . "\n";
?>
--EXPECT--
PH7_ENGINE=PH7/2.1.4
--CLEAN--
<?php

