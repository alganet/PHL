<?php
/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

class OOTestClass {
    public $name;
    public function __construct($name) {
        $this->name = $name;
    }
}

$obj = new OOTestClass("PHL");
echo "Name: " . $obj->name . "\n";
?>
