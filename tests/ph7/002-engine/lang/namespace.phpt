--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Namespace emits warning
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
namespace my\ns;
echo "done\n";
?>
--EXPECTF--
%s 2 Warning: Namespace support is disabled in the current release of the PH7(2.1.4) engine
done
