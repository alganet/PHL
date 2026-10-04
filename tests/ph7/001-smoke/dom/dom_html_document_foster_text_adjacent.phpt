--TEST--
Two runs fostered out past a table become one text node, not two
--FILE--
<?php
/* The tree may never carry two adjacent text nodes: a character run written
 * where the node immediately before it is already text APPENDS to that node.
 * Both insertion paths owe it.  The ordinary one -- a run inserted under the
 * current node -- gets it from xmlAddChild, but a run FOSTERED out past a
 * table is inserted before the table element, and that is where a second node
 * used to open beside the first.  Serialized markup cannot tell the two shapes
 * apart, so this walks the tree and prints the nodes themselves. */
$cases = [
	/* Two runs fostered out of the same table, with furniture between. */
	'cell-between'  => '<table>a<td>b</td>c</table>',
	'row-between'   => '<table>x<tr>y</tr>z</table>',
	'three-runs'    => '<table><tbody>a<tr>b</tr>c</tbody></table>',
	'full-row'      => '<table>a<tr><td>q</td></tr>b</table>',
	'cells-between' => '<table><td>x</td>a<td>y</td>b</table>',

	/* The text ALREADY before the table is the same insertion position. */
	'text-before'   => '<div>d<table>a<td>b</td>c</table>e</div>',

	/* A node that is not text between them keeps the runs apart. */
	'comment-split' => '<table>a<!--k-->b</table>',
	'element-split' => '<table>a<b>bold</b>c</table>',

	/* The column group and a nested table foster through the same door. */
	'col-between'   => '<table>1<col>2</table>',
	'caption'       => '<table><caption>c</caption>x<tr></tr>y</table>',
];
function walk($oNode, $nDepth = 0) {
	foreach ($oNode->childNodes as $oKid) {
		if ($oKid->nodeType === XML_TEXT_NODE) {
			$zWhat = '#text ' . var_export($oKid->nodeValue, true);
		} elseif ($oKid->nodeType === XML_COMMENT_NODE) {
			$zWhat = '#comment';
		} else {
			$zWhat = $oKid->nodeName;
		}
		printf("%s%s\n", str_repeat('  ', $nDepth + 1), $zWhat);
		if ($oKid->nodeType === XML_ELEMENT_NODE) {
			walk($oKid, $nDepth + 1);
		}
	}
}
foreach ($cases as $zName => $zSrc) {
	printf("%s\n", $zName);
	$oDoc = \Dom\HTMLDocument::createFromString($zSrc, LIBXML_NOERROR);
	walk($oDoc->body);
}
--EXPECT--
cell-between
  #text 'ac'
  TABLE
    TBODY
      TR
        TD
          #text 'b'
row-between
  #text 'xyz'
  TABLE
    TBODY
      TR
three-runs
  #text 'abc'
  TABLE
    TBODY
      TR
full-row
  #text 'ab'
  TABLE
    TBODY
      TR
        TD
          #text 'q'
cells-between
  #text 'ab'
  TABLE
    TBODY
      TR
        TD
          #text 'x'
        TD
          #text 'y'
text-before
  DIV
    #text 'dac'
    TABLE
      TBODY
        TR
          TD
            #text 'b'
    #text 'e'
comment-split
  #text 'ab'
  TABLE
    #comment
element-split
  #text 'a'
  B
    #text 'bold'
  #text 'c'
  TABLE
col-between
  #text '12'
  TABLE
    COLGROUP
      COL
caption
  #text 'xy'
  TABLE
    CAPTION
      #text 'c'
    TBODY
      TR
