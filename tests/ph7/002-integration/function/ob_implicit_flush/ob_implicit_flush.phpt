--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_implicit_flush controls implicit flushing

--FILE--
<?php
ob_implicit_flush(1);
echo "flush_enabled\n";
ob_implicit_flush(0);
echo "flush_disabled\n";
?>
--EXPECT--
flush_enabled
flush_disabled
--CLEAN--
<?php

