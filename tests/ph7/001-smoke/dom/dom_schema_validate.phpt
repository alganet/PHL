--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMDocument::schemaValidateSource: libxml2-verbatim schema error wording + line numbers
--FILE--
<?php
// Mirror PHPUnit's TextUI\Configuration\Xml\Validator + ValidationResult
$xsd = <<<'XSD'
<?xml version="1.0" encoding="UTF-8"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element name="phpunit">
    <xs:complexType>
      <xs:sequence>
        <xs:element name="testsuites" minOccurs="0"/>
      </xs:sequence>
      <xs:attribute name="bootstrap" type="xs:string"/>
      <xs:attribute name="colors" type="xs:boolean"/>
    </xs:complexType>
  </xs:element>
</xs:schema>
XSD;

function validate(DOMDocument $document, string $xsd): array {
    $priorState = libxml_use_internal_errors(true);
    libxml_clear_errors();
    $document->schemaValidateSource($xsd);
    $errors = [];
    foreach (libxml_get_errors() as $error) {
        $errors[$error->line][] = trim($error->message);
    }
    libxml_clear_errors();
    libxml_use_internal_errors($priorState);
    return $errors;
}

$valid = new DOMDocument;
$valid->loadXML('<phpunit bootstrap="x.php" colors="true"/>');
var_dump($valid->schemaValidateSource($xsd));

$invalid = new DOMDocument;
$invalid->loadXML("<phpunit\n  bogusAttr=\"1\"\n  colors=\"true\">\n  <badChild/>\n</phpunit>");
$errors = validate($invalid, $xsd);
foreach ($errors as $line => $msgs) {
    echo "Line $line:\n";
    foreach ($msgs as $m) { echo "  - $m\n"; }
}
--EXPECT--
bool(true)
Line 3:
  - Element 'phpunit', attribute 'bogusAttr': The attribute 'bogusAttr' is not allowed.
Line 4:
  - Element 'badChild': This element is not expected. Expected is ( testsuites ).
--CLEAN--
<?php
libxml_use_internal_errors(false);
libxml_clear_errors();
