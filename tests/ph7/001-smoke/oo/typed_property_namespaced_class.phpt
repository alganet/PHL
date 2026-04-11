--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: full namespace-qualified class type resolves correctly
--FILE--
<?php
namespace TpnAppModels;
class Tag { public string $name = ""; }

namespace TpnApp;
class Post {
    public \TpnAppModels\Tag $tag;
}

$p = new Post();
$p->tag = new \TpnAppModels\Tag();
$p->tag->name = "php";
echo get_class($p->tag), " ", $p->tag->name, "\n";
?>
--EXPECT--
TpnAppModels\Tag php
--CLEAN--
<?php
unset($p);
