--TEST--
Check for vapor Engine::__constructor()
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
$v1 = new Vapor\Engine('/tmp');
echo $v1->basepath . PHP_EOL;
echo $v1->extension . PHP_EOL;

$v2 = new Vapor\Engine('/tmp', 'php');
echo $v2->basepath . PHP_EOL;
echo $v2->extension . PHP_EOL;
?>
--EXPECT--
/tmp

/tmp
php
