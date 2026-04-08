--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
::class with namespaces resolves fully qualified name
--FILE--
<?php
namespace CcnVendor\CcnLib;
class CcnWidget {}
echo CcnWidget::class . "\n";
echo \CcnVendor\CcnLib\CcnWidget::class . "\n";
?>
--EXPECT--
CcnVendor\CcnLib\CcnWidget
CcnVendor\CcnLib\CcnWidget
--CLEAN--
<?php
