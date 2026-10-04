--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespaced attribute factory parks its binding and declares nothing
--FILE--
<?php
// php 8.4's Dom\Document::createAttributeNS() is a different factory from the
// 2004 door's, not a retyped one. The 2004 one declares the namespace on the
// document's ROOT ELEMENT, so a rootless document cannot answer at all and the
// prefix asked for is replaced by any the root already binds. The namespaced
// one declares nothing: the binding rides on the attribute, so a rootless
// document answers, the prefix asked for is the prefix reported, and nothing
// is reused -- not on create and not on insert. Its grammar failures are the
// Invalid Character Error where the 2004 one says Namespace Error, and an
// empty-string namespace is simply no namespace.
$dom_mcans_xmlns = 'http://www.w3.org/2000/xmlns/';
$dom_mcans_xml   = 'http://www.w3.org/XML/1998/namespace';
$dom_mcans_show = static function ($doc, $uri, $name) {
    try {
        $a = $doc->createAttributeNS($uri, $name);
    } catch (\Throwable $e) {
        return get_class($e) . ' ' . $e->getCode() . ' ' . $e->getMessage();
    }
    return sprintf(
        'name=%s prefix=%s uri=%s local=%s value=%s',
        var_export($a->name, true),
        var_export($a->prefix, true),
        var_export($a->namespaceURI, true),
        var_export($a->localName, true),
        var_export($a->value, true)
    );
};

echo "== the table, on a document WITH a root element ==\n";
$dom_mcans_uris = [null, '', 'urn:u', $dom_mcans_xmlns, $dom_mcans_xml];
$dom_mcans_names = ['a', 'p:a', 'xmlns', 'xmlns:p', 'xml:a', '1:x', 'x y', 'a:b:c', ':x', '', 'p:'];
foreach ($dom_mcans_uris as $dom_mcans_u) {
    foreach ($dom_mcans_names as $dom_mcans_n) {
        $dom_mcans_d = Dom\XMLDocument::createFromString('<r xmlns:q="urn:q"/>');
        printf(
            "uri=%-40s name=%-8s => %s\n",
            var_export($dom_mcans_u, true),
            var_export($dom_mcans_n, true),
            $dom_mcans_show($dom_mcans_d, $dom_mcans_u, $dom_mcans_n)
        );
    }
}

// The 2004 door needs a root element to declare on and says so with a warning
// and a false; the namespaced one declares nothing, so it answers the same
// thing either way.
echo "== and the SAME answers on a document with none ==\n";
$dom_mcans_bad = 0;
foreach ($dom_mcans_uris as $dom_mcans_u) {
    foreach ($dom_mcans_names as $dom_mcans_n) {
        $dom_mcans_a = $dom_mcans_show(Dom\XMLDocument::createEmpty(), $dom_mcans_u, $dom_mcans_n);
        $dom_mcans_b = $dom_mcans_show(
            Dom\XMLDocument::createFromString('<r xmlns:q="urn:q"/>'),
            $dom_mcans_u,
            $dom_mcans_n
        );
        if ($dom_mcans_a !== $dom_mcans_b) {
            $dom_mcans_bad++;
            echo "DIFFERS uri=", var_export($dom_mcans_u, true), " name=",
                var_export($dom_mcans_n, true), ": $dom_mcans_a | $dom_mcans_b\n";
        }
    }
}
var_dump($dom_mcans_bad);

// Nothing is reused when the attribute is written, either: the node answers the
// prefix it was ASKED for however the document already spells that URI.
echo "== and nothing is respelled on insert ==\n";
$dom_mcans_ins = [
    ['<r/>', 'urn:u', 'p:a'],
    ['<r/>', 'urn:u', 'a'],
    ['<r xmlns:q="urn:u"/>', 'urn:u', 'z:b'],
    ['<r xmlns="urn:u"/>', 'urn:u', 'z:b'],
    ['<r xmlns="urn:u"/>', 'urn:u', 'b'],
    ['<r xmlns:z="urn:o"/>', 'urn:u', 'z:b'],
    ['<r/>', $dom_mcans_xml, 'xml:a'],
    ['<r/>', $dom_mcans_xml, 'a'],
    ['<r/>', null, 'a'],
];
foreach ($dom_mcans_ins as $dom_mcans_c) {
    [$dom_mcans_x, $dom_mcans_u, $dom_mcans_n] = $dom_mcans_c;
    $dom_mcans_d = Dom\XMLDocument::createFromString($dom_mcans_x);
    $dom_mcans_at = $dom_mcans_d->createAttributeNS($dom_mcans_u, $dom_mcans_n);
    $dom_mcans_at->value = 'V';
    printf("in=%-22s uri=%-40s name=%s\n", $dom_mcans_x, var_export($dom_mcans_u, true), $dom_mcans_n);
    printf("    pre  %s\n", sprintf(
        'name=%s prefix=%s uri=%s',
        var_export($dom_mcans_at->name, true),
        var_export($dom_mcans_at->prefix, true),
        var_export($dom_mcans_at->namespaceURI, true)
    ));
    $dom_mcans_d->documentElement->setAttributeNodeNS($dom_mcans_at);
    printf("    post %s\n", sprintf(
        'name=%s prefix=%s uri=%s owner=%s',
        var_export($dom_mcans_at->name, true),
        var_export($dom_mcans_at->prefix, true),
        var_export($dom_mcans_at->namespaceURI, true),
        var_export($dom_mcans_at->ownerElement?->localName, true)
    ));
    printf("    read %s\n", var_export(
        $dom_mcans_d->documentElement->getAttributeNS($dom_mcans_u, $dom_mcans_at->localName),
        true
    ));
}
--EXPECT--
== the table, on a document WITH a root element ==
uri=NULL                                     name='a'      => name='a' prefix=NULL uri=NULL local='a' value=''
uri=NULL                                     name='p:a'    => DOMException 14 Namespace Error
uri=NULL                                     name='xmlns'  => DOMException 14 Namespace Error
uri=NULL                                     name='xmlns:p' => DOMException 14 Namespace Error
uri=NULL                                     name='xml:a'  => DOMException 14 Namespace Error
uri=NULL                                     name='1:x'    => DOMException 5 Invalid Character Error
uri=NULL                                     name='x y'    => DOMException 5 Invalid Character Error
uri=NULL                                     name='a:b:c'  => DOMException 5 Invalid Character Error
uri=NULL                                     name=':x'     => DOMException 5 Invalid Character Error
uri=NULL                                     name=''       => DOMException 5 Invalid Character Error
uri=NULL                                     name='p:'     => DOMException 5 Invalid Character Error
uri=''                                       name='a'      => name='a' prefix=NULL uri=NULL local='a' value=''
uri=''                                       name='p:a'    => DOMException 14 Namespace Error
uri=''                                       name='xmlns'  => DOMException 14 Namespace Error
uri=''                                       name='xmlns:p' => DOMException 14 Namespace Error
uri=''                                       name='xml:a'  => DOMException 14 Namespace Error
uri=''                                       name='1:x'    => DOMException 5 Invalid Character Error
uri=''                                       name='x y'    => DOMException 5 Invalid Character Error
uri=''                                       name='a:b:c'  => DOMException 5 Invalid Character Error
uri=''                                       name=':x'     => DOMException 5 Invalid Character Error
uri=''                                       name=''       => DOMException 5 Invalid Character Error
uri=''                                       name='p:'     => DOMException 5 Invalid Character Error
uri='urn:u'                                  name='a'      => name='a' prefix=NULL uri='urn:u' local='a' value=''
uri='urn:u'                                  name='p:a'    => name='p:a' prefix='p' uri='urn:u' local='a' value=''
uri='urn:u'                                  name='xmlns'  => DOMException 14 Namespace Error
uri='urn:u'                                  name='xmlns:p' => DOMException 14 Namespace Error
uri='urn:u'                                  name='xml:a'  => DOMException 14 Namespace Error
uri='urn:u'                                  name='1:x'    => DOMException 5 Invalid Character Error
uri='urn:u'                                  name='x y'    => DOMException 5 Invalid Character Error
uri='urn:u'                                  name='a:b:c'  => DOMException 5 Invalid Character Error
uri='urn:u'                                  name=':x'     => DOMException 5 Invalid Character Error
uri='urn:u'                                  name=''       => DOMException 5 Invalid Character Error
uri='urn:u'                                  name='p:'     => DOMException 5 Invalid Character Error
uri='http://www.w3.org/2000/xmlns/'          name='a'      => DOMException 14 Namespace Error
uri='http://www.w3.org/2000/xmlns/'          name='p:a'    => DOMException 14 Namespace Error
uri='http://www.w3.org/2000/xmlns/'          name='xmlns'  => name='xmlns' prefix=NULL uri='http://www.w3.org/2000/xmlns/' local='xmlns' value=''
uri='http://www.w3.org/2000/xmlns/'          name='xmlns:p' => name='xmlns:p' prefix='xmlns' uri='http://www.w3.org/2000/xmlns/' local='p' value=''
uri='http://www.w3.org/2000/xmlns/'          name='xml:a'  => DOMException 14 Namespace Error
uri='http://www.w3.org/2000/xmlns/'          name='1:x'    => DOMException 5 Invalid Character Error
uri='http://www.w3.org/2000/xmlns/'          name='x y'    => DOMException 5 Invalid Character Error
uri='http://www.w3.org/2000/xmlns/'          name='a:b:c'  => DOMException 5 Invalid Character Error
uri='http://www.w3.org/2000/xmlns/'          name=':x'     => DOMException 5 Invalid Character Error
uri='http://www.w3.org/2000/xmlns/'          name=''       => DOMException 5 Invalid Character Error
uri='http://www.w3.org/2000/xmlns/'          name='p:'     => DOMException 5 Invalid Character Error
uri='http://www.w3.org/XML/1998/namespace'   name='a'      => name='a' prefix=NULL uri='http://www.w3.org/XML/1998/namespace' local='a' value=''
uri='http://www.w3.org/XML/1998/namespace'   name='p:a'    => name='p:a' prefix='p' uri='http://www.w3.org/XML/1998/namespace' local='a' value=''
uri='http://www.w3.org/XML/1998/namespace'   name='xmlns'  => DOMException 14 Namespace Error
uri='http://www.w3.org/XML/1998/namespace'   name='xmlns:p' => DOMException 14 Namespace Error
uri='http://www.w3.org/XML/1998/namespace'   name='xml:a'  => name='xml:a' prefix='xml' uri='http://www.w3.org/XML/1998/namespace' local='a' value=''
uri='http://www.w3.org/XML/1998/namespace'   name='1:x'    => DOMException 5 Invalid Character Error
uri='http://www.w3.org/XML/1998/namespace'   name='x y'    => DOMException 5 Invalid Character Error
uri='http://www.w3.org/XML/1998/namespace'   name='a:b:c'  => DOMException 5 Invalid Character Error
uri='http://www.w3.org/XML/1998/namespace'   name=':x'     => DOMException 5 Invalid Character Error
uri='http://www.w3.org/XML/1998/namespace'   name=''       => DOMException 5 Invalid Character Error
uri='http://www.w3.org/XML/1998/namespace'   name='p:'     => DOMException 5 Invalid Character Error
== and the SAME answers on a document with none ==
int(0)
== and nothing is respelled on insert ==
in=<r/>                   uri='urn:u'                                  name=p:a
    pre  name='p:a' prefix='p' uri='urn:u'
    post name='p:a' prefix='p' uri='urn:u' owner='r'
    read 'V'
in=<r/>                   uri='urn:u'                                  name=a
    pre  name='a' prefix=NULL uri='urn:u'
    post name='a' prefix=NULL uri='urn:u' owner='r'
    read 'V'
in=<r xmlns:q="urn:u"/>   uri='urn:u'                                  name=z:b
    pre  name='z:b' prefix='z' uri='urn:u'
    post name='z:b' prefix='z' uri='urn:u' owner='r'
    read 'V'
in=<r xmlns="urn:u"/>     uri='urn:u'                                  name=z:b
    pre  name='z:b' prefix='z' uri='urn:u'
    post name='z:b' prefix='z' uri='urn:u' owner='r'
    read 'V'
in=<r xmlns="urn:u"/>     uri='urn:u'                                  name=b
    pre  name='b' prefix=NULL uri='urn:u'
    post name='b' prefix=NULL uri='urn:u' owner='r'
    read 'V'
in=<r xmlns:z="urn:o"/>   uri='urn:u'                                  name=z:b
    pre  name='z:b' prefix='z' uri='urn:u'
    post name='z:b' prefix='z' uri='urn:u' owner='r'
    read 'V'
in=<r/>                   uri='http://www.w3.org/XML/1998/namespace'   name=xml:a
    pre  name='xml:a' prefix='xml' uri='http://www.w3.org/XML/1998/namespace'
    post name='xml:a' prefix='xml' uri='http://www.w3.org/XML/1998/namespace' owner='r'
    read 'V'
in=<r/>                   uri='http://www.w3.org/XML/1998/namespace'   name=a
    pre  name='a' prefix=NULL uri='http://www.w3.org/XML/1998/namespace'
    post name='a' prefix=NULL uri='http://www.w3.org/XML/1998/namespace' owner='r'
    read 'V'
in=<r/>                   uri=NULL                                     name=a
    pre  name='a' prefix=NULL uri=NULL
    post name='a' prefix=NULL uri=NULL owner='r'
    read 'V'
