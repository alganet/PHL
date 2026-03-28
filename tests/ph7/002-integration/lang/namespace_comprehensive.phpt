--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Namespace comprehensive functionality
--FILE--
<?php
namespace App\Models;

// __NAMESPACE__ in namespace
echo "ns:", __NAMESPACE__, "\n";

// Class in namespace
class User {
    function name() { return "User"; }
}

// Function in namespace
function greet() { return "hello"; }

// Unqualified access within same namespace
$u = new User();
echo $u->name(), "\n";
echo greet(), "\n";

// FQN access
$u2 = new \App\Models\User();
echo $u2->name(), "\n";

// Global function fallback
echo strtoupper("test"), "\n";
echo strlen("abcd"), "\n";

namespace App\Services;

// __NAMESPACE__ updates
echo "ns2:", __NAMESPACE__, "\n";

// use import
use App\Models\User;

$u3 = new User();
echo $u3->name(), "\n";

// use with alias
use App\Models\User as Person;

$p = new Person();
echo $p->name(), "\n";
?>
--EXPECT--
ns:App\Models
User
hello
User
TEST
4
ns2:App\Services
User
User
--CLEAN--
<?php
