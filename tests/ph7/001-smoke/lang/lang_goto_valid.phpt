--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto with valid label
--FILE--
<?php
echo "start\n";
goto label;
echo "skipped\n";
label:
echo "end\n";
?>
--EXPECT--
start
end
--CLEAN--
<?php

