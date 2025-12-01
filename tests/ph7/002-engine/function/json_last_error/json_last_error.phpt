--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_last_error returns a non-zero error after invalid JSON
--FILE--
<?php
json_decode('invalid json');
echo (json_last_error() !== JSON_ERROR_NONE) ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
