<?php

class PDOUtil {
    static public function init()
    {
        $pdo = self::create();
        if (! self::tableExists($pdo, 'test_swow_pgsql_users')) {
            $pdo->exec('CREATE TABLE test_swow_pgsql_users (
                id SERIAL primary key NOT NULL,
                name character varying(32),
                age integer)');
        }

        $pdo->exec('TRUNCATE test_swow_pgsql_users');
    }

    static public function tableExists($pdo, $table) {

        try {
            $result = $pdo->query("SELECT 1 FROM {$table} LIMIT 1");
        } catch (Exception $e) {
            return false;
        }
    
        return $result !== false;
    }

    static public function create($classname = 'PDO'): PDO {
        $host = TEST_POSTGRES_HOST;
        $port = TEST_POSTGRES_PORT;
        $user = TEST_POSTGRES_USER;
        $password = TEST_POSTGRES_PASSWORD;
        $dbname = TEST_POSTGRES_DBNAME;

        $dsn = "pgsql:host={$host};port={$port};dbname={$dbname}";

        $db = new $classname($dsn, $user, $password);

        if (!$db) {
            die("Could not create PDO object (DSN=$dsn, user=$user)\n");
        }
        return $db;
    }
}
