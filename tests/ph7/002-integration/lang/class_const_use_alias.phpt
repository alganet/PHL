--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
::class with use and alias resolves to original name
--FILE--
<?php
namespace CcuaVendor\CcuaLib;
class CcuaWidget {}

namespace CcuaApp;
use CcuaVendor\CcuaLib\CcuaWidget;
use CcuaVendor\CcuaLib\CcuaWidget as CcuaW;
echo CcuaWidget::class . "\n";
echo CcuaW::class . "\n";
?>
--EXPECT--
CcuaVendor\CcuaLib\CcuaWidget
CcuaVendor\CcuaLib\CcuaWidget
--CLEAN--
<?php
