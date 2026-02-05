--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
register_shutdown_function registers callback
--SKIPIF--
<?php
if (!function_exists('register_shutdown_function')) { echo 'skip: register_shutdown_function not available'; }
?>
--FILE--
<?php
register_shutdown_function(function() {
    echo "shutdown_called\n";
});
echo "before_shutdown\n";
?>
--EXPECT--
before_shutdown
shutdown_called
--CLEAN--
<?php

