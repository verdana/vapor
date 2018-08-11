--TEST--
Check for vapor Engine::setFileExtension()
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
$v1 = new Vapor\Engine('/tmp');
var_dump($v1->extension);

$v1->setFileExtension('php');
var_dump($v1->extension);

$v1->setFileExtension('php5');
var_dump($v1->extension);
?>
--EXPECT--
NULL
string(3) "php"
string(4) "php5"
