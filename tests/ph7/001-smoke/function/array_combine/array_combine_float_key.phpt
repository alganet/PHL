--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_combine should keep float keys as strings
--FILE--
<?php
$keys = array(1.5, 2.0);
$vals = array('foo','bar');
$c = array_combine($keys, $vals);
// keys should be string representations
foreach($c as $k=>$v) echo $k.":".$v."\n";
?>
--EXPECT--
1.5:foo
2:bar
--CLEAN--
<?php
unset($keys, $vals, $c);
