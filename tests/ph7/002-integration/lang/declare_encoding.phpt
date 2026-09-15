--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
declare(encoding=...) warns exactly as php does
--FILE--
<?php
declare(encoding='UTF-8');
echo "OK";
?>
--EXPECTF--
%Adeclare(encoding=...) ignored because Zend multibyte feature is turned off by settings in %s on line 2%AOK
