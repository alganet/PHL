--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
(array) cast mangles non-public property keys, as php does
--DESCRIPTION--
php prefixes a private property's key with "\0DeclaringClass\0" and a protected one's with
"\0*\0" when casting an object to an array, so same-named members from different visibility
levels stay distinct and isset($arr['priv']) is FALSE. PHL emitted bare names, which
collided them and answered TRUE. get_object_vars/foreach/json_encode are NOT mangled (they
expose the accessible surface), and var_export keeps plain names -- all as php has them.
--FILE--
<?php
class AMK { public $pub = 1; private $priv = 2; protected $prot = 3; }
class BMK extends AMK { private $bpriv = 4; }

$arr = (array) new BMK();
$show = fn($s) => str_replace("\0", '|', $s);
echo implode(',', array_map($show, array_keys($arr))), "\n";
var_dump(isset($arr['priv']), array_key_exists('priv', $arr), $arr['pub']);

echo "unmangled surfaces:\n";
echo '  get_object_vars: ', implode(',', array_keys(get_object_vars(new AMK()))), "\n";
$k = []; foreach (new AMK() as $key => $v) { $k[] = $key; }
echo '  foreach: ', implode(',', $k), "\n";
echo '  json: ', json_encode(new AMK()), "\n";

echo "round trip keeps the mangled keys:\n";
$round = (object) ((array) new AMK());
echo '  ', implode(',', array_map($show, array_keys(get_object_vars($round)))), "\n";
?>
--EXPECT--
pub,|AMK|priv,|*|prot,|BMK|bpriv
bool(false)
bool(false)
int(1)
unmangled surfaces:
  get_object_vars: pub
  foreach: pub
  json: {"pub":1}
round trip keeps the mangled keys:
  pub,|AMK|priv,|*|prot
