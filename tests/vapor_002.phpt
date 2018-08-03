--TEST--
Check for vapor constructor
--SKIPIF--
<?php if (!extension_loaded("vapor")) print "skip"; ?>
--FILE--
<?php
$v1 = new Vapor('/tmp');
echo $v1->basepath . PHP_EOL;
echo $v1->extension . PHP_EOL;

$v2 = new Vapor('/tmp', 'php');
echo $v2->basepath . PHP_EOL;
echo $v2->extension . PHP_EOL;
?>
--EXPECT--
/tmp

/tmp
php
