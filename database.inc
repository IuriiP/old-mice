<?php

require_once('Controller.inc');
require_once('Helper.inc');

require_once('database/namespace.inc');

/**
 * Core class for unified database access
 *
 * <pre>
 * plugin name="database" engine="mysql" host="localhost" db="database" user="username" pass="password" logsql="on"
 * </pre>
 *
 * Safe and convenient way to handle SQL queries utilizing type-hinted placeholders.
 *
 * Key features:
 * - set of helper functions to get the desired result right out of query, like in PEAR::DB
 * - conditional query building using parse() method to build queries of whatever comlexity,
 *   while keeping extra safety of placeholders
 * - type-hinted placeholders
 *
 *  Type-hinted placeholders are great because:
 * - safe, as any other [properly implemented] placeholders
 * - no need for manual escaping or binding, makes the code extra DRY
 * - allows support for non-standard types such as identifier or array, which saves A LOT of pain in the back.
 *
 * Supported placeholders at the moment are:
 * - ?s ("string")  - strings (also DATE, FLOAT and DECIMAL)
 * - ?i ("integer") - the name says it all
 * - ?f ("fields")  - array of field names indexed by alias (substituted with string of '`value` as `key`, ...')
 * - ?n ("name")    - identifiers (table and field names)
 * - ?a ("array")   - complex placeholder for IN() operator  (substituted with string of 'a','b','c' format, without parentesis)
 * - ?u ("update")  - complex placeholder for SET operator (substituted with string of `field`='value',`field`='value' format)
 * - ?& ("where AND")  - complex placeholder for WHERE operator (substituted with string of `field`='value' AND `field`='value' format)
 * - ?| ("where OR")  - complex placeholder for WHERE operator (substituted with string of `field`='value' OR `field`='value' format)
 * - ?p ("parsed") - special type placeholder, for inserting already parsed statements without any processing, to avoid double parsing.
 * - ?r ("raw parsed") - array of ?p.
 * - ?? ("auto escaped") - autodetect ?i or ?s
 * - ? - special type placeholder, for use later preparing by PDO.
 *
 * Some examples:
 * <pre>
 * $connect = 'user/database@localhost:3333';
 * $db = new database($connect,$pass); // with some of the default settings overwritten
 *
 *
 * $data = $db->getInd('id','SELECT * FROM ?n WHERE id IN ?a','table', array(1,2));
 * $data = $db->getAll("SELECT * FROM ?n WHERE mod=?s LIMIT ?i",$table,$mod,$limit);
 *
 * $ids  = $db->getIndCol("SELECT id,tagname FROM tags WHERE tagname = ?s",$tag);
 * $data = $db->getAll("SELECT * FROM table WHERE category IN (?a)",$ids);
 *
 * $data = array('offers_in' => $in, 'offers_out' => $out);
 * $sql  = "INSERT INTO stats SET pid=?i,dt=CURDATE(),?u ON DUPLICATE KEY UPDATE ?u";
 * $db->query($sql,$pid,$data,$data);
 *
 * if ($var === NULL) {
 *     $sqlpart = "field is NULL";
 * } else {
 *     $sqlpart = $db->parse("field = ?s", $var);
 * }
 * $data = $db->getAll("SELECT * FROM table WHERE ?p", $bar, $sqlpart);
 * </pre>
 * 
 * Based on
 * @author <col.shrapnel@gmail.com>
 * @property-read string $sql Last SQL
 * @property-read int $lastid Last autoincrement
 * @property-read string $error Last error description
 */
class database
  extends \Controller {
  private $attrs = null;
  private $logsql = null;
  private $engine = null;
  private $_sql = null;
  private $_statement = null;
  private $_affected = 0;
  private $_lastid = 0;
  private $_error = '';
  private $intransaction = 0;

  /**
   * Try connect to PDO database.
   *
   */
  function __construct( $args = [] ) {
    $args = (array) $args;
    if( $this->logsql = isset($args['logsql']) ) {
      unset($args['logsql']);
    }

    try {
      $this->engine = new PDO(\Helper::eliminate($args, 'engine', 'database'), \Helper::eliminate($args, 'user'), \Helper::eliminate($args, 'pass'), $args);
      $this->attrs = $args;
      register_shutdown_function(array(&$this, 'shutdown_transaction'));
    }
    catch( PDOException $e ) {
      $this->engine = null;
      $this->error($e->getMessage());
    }
  }

  /**
   * Invoking.
   *
   * @return Controller $this on DB ready | null on error
   */
  function __invoke( $args = [] ) {
    return($this->engine ? $this : null);
  }

  /**
   * Readonly properties.
   */
  public function __get( $name ) {
    switch( $name ) {
      case 'sql':
        return $this->_sql;
      case 'error':
        return $this->_error;
      case 'lastId':
        return $this->_lastid;
      case 'affectedRows':
        return $this->_affected;
      default:
        return null;
    }
  }

  /**
   * Log error text.
   *
   * @return bool FALSE (for chaining)
   */
  private function error( $err ) {
    $this->spy($this->_error = $err);
    return FALSE;
  }

  /**
   * Start transaction.
   * 
   * @return bool Is in transaction now
   */
  public function transaction() {
    if( !$this->intransaction ) {
      $this->_error = null;

      if( $mode = $this->engine->beginTransaction() ) {
        if( $this->logsql ) {
          $this->spy('transaction: start');
        }
        $this->intransaction++;
      } elseif( $this->logsql ) {
        $this->spy('transaction: not supported');
      }
    } else {
      if( $this->logsql ) {
        $this->spy('transaction: start #' . $this->intransaction);
      }
      $mode = true;
      $this->intransaction++;
    }
    return($mode);
  }

  public final function shutdown_transactions() {
    if( $this->intransaction ) {
      if( $this->logsql ) {
        $this->spy('transaction: autorollback');
      }
      $this->intransaction = 0;
      $this->engine->rollBack();
    }
  }

  public function commit() {
    if( $this->intransaction ) {
      if( $this->_error ) {
        return($this->rollback());
      } elseif( --$this->intransaction ) {
        if( $this->logsql ) {
          $this->spy('transaction: commit #' . $this->intransaction);
        }
      } else {
        if( $this->logsql ) {
          $this->spy('transaction: commit');
        }
        $this->engine->commit();
      }
    } else {
      if( $this->logsql ) {
        $this->spy('transaction: not active (commit)');
      }
    }
    return(true);
  }

  public function rollback() {
    if( $this->intransaction ) {
      if( !$this->_error ) {
        $this->_error = 'Rolled back';
      }
      if( --$this->intransaction ) {
        if( $this->logsql ) {
          $this->spy('transaction: rollback #' . $this->intransaction);
        }
      } else {
        if( $this->logsql ) {
          $this->spy('transaction: rollback');
        }
        $this->engine->rollBack();
      }
    } else {
      if( $this->logsql ) {
        $this->spy('transaction: not active (rollback)');
      }
      return(true);
    }
    return(false);
  }

  /**
   * Conventional function to run a query with placeholders. 
   * 
   * A mysqli_query wrapper with placeholders support.
   *
   * Examples:
   * <pre>
   * $db->query("DELETE FROM table WHERE id=?i", $id);
   * </pre>
   *
   * @param string $query An SQL query with placeholders
   * @param mixed $arg,... Unlimited number of arguments to match placeholders in the query
   * @return resource|false Whatever PDO::execute() returns
   */
  public function query( string $query ) {
    if( $this->intransaction && $this->error ) {
      return(false);
    }
    return $this->rawQuery($this->prepareQuery(func_get_args()));
  }

  /**
   * Prepare and execute query in PDO style. 
   * 
   * @param string $sql
   * @param array $params
   * @return PDOStatement | false
   */
  public function perform( string $sql, array $params = [] ) {
    if( $this->intransaction && $this->error ) {
      return(false);
    }
    if( $this->_statement = $this->engine->prepare($sql) ) {
      if( $this->_statement->execute($params) ) {
        return($this->_statement);
      } else {
        $this->_error[] = $this->_statement->errorInfo[2];
      }
    } else {
      $this->_error[] = $this->engine->errorInfo[2];
    }
    return(false);
  }

  /**
   * Execute prepared request in PDO style.
   * 
   * @param array $params
   * @param PDOStatement $statement
   * @return PDOStatement | false
   */
  public function execute( $params = [], $statement = null ) {
    if( !$statement ) {
      $statement = $this->_statement;
    }
    if( ($this->intransaction && $this->error) || !$statement || $statement->errorInfo[1] ) {
      return(false);
    }
    if( $statement->execute($params) ) {
      if( $this->logsql ) {
        $this->spy($this->_sql = $statement->queryString);
      }
      return($this->_statement = $statement);
    } else {
      $this->_error[] = $statement->errorInfo[2];
    }
    return(false);
  }

  /**
   * Conventional function to free the PDOStatement.
   * 
   * @param mixed $chain Return value for chaining
   * @return mixed
   */
  public function free( $chain ) {
    if( $this->_statement ) {
      $this->_statement->closeCursor();
    }
    return($chain);
  }

  /**
   * Conventional function to fetch next row of the resultset.
   *
   * @return array Associative fields
   */
  public function fetch( $style = PDO::FETCH_ASSOC ) {
    if( $this->_statement && $this->_statement->columnCount() ) {
      return $this->_statement->fetch($style);
    }
    return [];
  }

  /**
   * Conventional function to fetch all rows of the resultset.
   *
   * @return array Array of associative fields
   */
  public function fetchAll( $style = PDO::FETCH_ASSOC ) {
    if( $this->_statement && $this->_statement->columnCount() ) {
      return $this->free($this->_statement->fetchAll($style));
    }
    return [];
  }

  /**
   * Helper function to get single row right out of query and optional arguments.
   *
   * Examples:
   * <pre>
   * $data = $db->getRow("SELECT * FROM table WHERE id=1");
   * $data = $db->getRow("SELECT * FROM table WHERE id=?i", $id);
   * </pre>
   * 
   * @param string $query An SQL query with placeholders
   * @param mixed  $arg,... Unlimited number of arguments to match placeholders in the query
   * @return array Either associative array contains first row of resultset | false if none found
   */
  public function getRow( $query ) {
    if( $this->rawQuery($this->prepareQuery(func_get_args())) ) {
      return $this->free($this->fetch());
    }
    return false;
  }

  /**
   * Helper function to get all the rows of resultset right out of query and optional arguments.
   *
   * Examples:
   * <pre>
   * $data = $db->getAll("SELECT * FROM table");
   * $data = $db->getAll("SELECT * FROM table LIMIT ?i,?i", $start, $rows);
   * </pre>
   *
   * @param string $query An SQL query with placeholders
   * @param mixed  $arg,... Unlimited number of arguments to match placeholders in the query
   * @return array Enumerated 2d array contains the resultset. Empty if no rows found.
   */
  public function getAll( $query ) {
    $ret = array();
    if( $this->rawQuery($this->prepareQuery(func_get_args())) && $this->_statement->columnCount() ) {
      return $this->_statement->fetchAll(PDO::FETCH_ASSOC);
    }
    return false;
  }

  /**
   * Helper function to get all the rows of resultset into indexed array right out of query and optional arguments
   *
   * Examples:
   * $data = $db->getInd("id owner", "SELECT * FROM table");
   * $data = $db->getInd("id", "SELECT * FROM table LIMIT ?i,?i", $start, $rows);
   *
   * @param string $index - names of the fields which value is used to index resulting array
   * @param string $query - an SQL query with placeholders
   * @param mixed  $arg,... unlimited number of arguments to match placeholders in the query
   * @return array - associative 2d array contains the resultset. Empty if no rows found.
   */
  public function getInd( $index, $query ) {
    $args = func_get_args();
    $idx = array_shift($args);
    if( is_string($idx) ) {
      $index = array_reverse(array_filter(explode(' ', str_replace(',', ' ', $idx))));
    } else {
      $index = array_reverse((array) $idx);
    }

    $ret = array();
    if( $this->rawQuery($this->prepareQuery($args)) ) {
      while( $row = $this->fetch() ) {
        $ret = array_merge_recursive($ret, array_reduce($index, function($carry, $item) {
            return(array($item => $carry));
          }, $row));
      }
      return $this->free($ret);
    }
    return false;
  }

  /**
   * Helper function to get a dictionary-style array right out of query and optional arguments.
   *
   * Examples:
   * <pre>
   * $data = $db->getIndCol("name", "SELECT name, id FROM cities");
   * </pre>
   *
   * @param string $index Name of the field which value is used to index resulting array
   * @param string $query An SQL query with placeholders
   * @param mixed  $arg,... Unlimited number of arguments to match placeholders in the query
   * @return array Associative array contains key=value pairs out of resultset. Empty if no rows found.
   */
  public function getIndCol( $index, $query ) {
    $args = func_get_args();
    $index = array_shift($args);
    $ret = array();
    if( $this->rawQuery($this->prepareQuery($args)) ) {
      while( $row = $this->fetch() ) {
        $key = $row[$index];
        unset($row[$index]);
        $ret[$key] = reset($row);
      }
      return $this->free($ret);
    }
    return false;
  }

  /**
   * Function to parse placeholders either in the full query or a query part
   * unlike native prepared statements, allows ANY query part to be parsed.
   *
   * Useful for debug
   * and EXTREMELY useful for conditional query building
   * like adding various query parts using loops, conditions, etc.
   * Already parsed parts have to be added via ?p placeholder.
   *
   * Examples:
   * <pre>
   * $query = $db->parse("SELECT * FROM table WHERE foo=?s AND bar=?s", $foo, $bar);
   * echo $query;
   *
   * if ($foo) {
   *     $qpart = $db->parse(" AND foo=?s", $foo);
   * }
   * $data = $db->getAll("SELECT * FROM table WHERE bar=?s ?p", $bar, $qpart);
   * <pre>
   *
   * @param mixed $query Whatever expression contains placeholders or array
   * @param mixed $arg,... Unlimited number of arguments to match placeholders in the expression
   * @return string Initial expression with placeholders substituted with data.
   */
  public function parse( $query ) {
    if( is_array($query) ) {
      return $this->prepareQuery($query);
    }
    return $this->prepareQuery(func_get_args());
  }

  /**
   * Function to implement whitelisting feature.
   * 
   * Sometimes we can't allow a non-validated user-supplied data to the query even through placeholder
   * especially if it comes down to SQL OPERATORS.
   *
   * Example:
   * <pre>
   * $order = $db->whiteList($_GET['order'], array('name','price'));
   * $dir   = $db->whiteList($_GET['dir'],   array('ASC','DESC'));
   * if (!$order || !dir) {
   *     throw new http404(); //non-expected values should cause 404 or similar response
   * }
   * $sql  = "SELECT * FROM table ORDER BY ?p ?p LIMIT ?i,?i"
   * $data = $db->getArr($sql, $order, $dir, $start, $per_page);
   * </pre>
   *
   * @param string $input Field name to test
   * @param array $allowed An array with allowed variants
   * @param string $default Optional variable to set if no match found. Default to false.
   * @return string|FALSE Either sanitized value or FALSE
   */
  public function whiteList( $input, $allowed, $default = FALSE ) {
    $found = array_search($input, $allowed);
    return ($found === FALSE) ? $default : $allowed[$found];
  }

  /**
   * Function to filter out arrays, for the whitelisting purposes.
   * 
   * Useful to pass entire superglobal to the INSERT or UPDATE query.
   * OUGHT to be used for this purpose,
   * as there could be fields to which user should have no access to.
   *
   * Example:
   * <pre>
   * $allowed = array('title','url','body','rating','term','type');
   * $data    = $db->filterArray($_POST,$allowed);
   * $sql     = "INSERT INTO ?n SET ?u";
   * $db->query($sql,$table,$data);
   * </pre>
   *
   * @param array $input Source array
   * @param array $allowed An array with allowed field names
   * @return array Filtered out source array
   */
  public function filterArray( $input, $allowed ) {
    foreach( array_keys($input) as $key ) {
      if( !in_array($key, $allowed) ) {
        unset($input[$key]);
      }
    }
    return($input);
  }

  /**
   * Check if a table exists in the database.
   *  EXPERIMENTAL!
   *
   * @param string $table Table to search for.
   * @return bool TRUE if table exists, FALSE if no table found.
   */
  function tableExists( $table ) {
    try {
      $result = $this->engine->query("SELECT 1 FROM `{$table}` LIMIT 1");
    }
    catch( Exception $e ) {
      return FALSE;
    }
    return($result !== FALSE);
  }

  /**
   * Get names of table fields.
   *  EXPERIMENTAL!
   * 
   * @param string $name Table name
   * @return array
   */
  public function tableFields( $name ) {
    $result = $this->engine->query("SELECT * FROM `{$table}` LIMIT 1");
    return array_keys($result->fetch(PDO::FETCH_ASSOC));
  }

  /**
   * Query builder model init.
   * 
   * @param string $name Model name
   * @return Model Prepared Query object
   */
  public function model( $name ) {
    if( ($model = $this->findModel($name . '.xml') ) && ( $xml = simplexml_load_file($model) ) ) {
      return new Database\QueryModel($this, $xml);
    }
    return(false);
  }

  /**
   * Query builder init.
   * 
   * @param string $name Model/table name
   * @return Query Prepared Query object
   */
  public function table( $name ) {
    if( $model = $this->model($name) ) {
      return($model);
    }
    return new Database\Query($this, $name);
  }

  /**
   * Function which actually runs a query against Mysql server.
   * 
   * Also logs some stats like profiling info and error message
   *
   * @param string $query A regular SQL query
   * @return resource Whatever mysqli returns
   */
  private function rawQuery( $query ) {
    if( $this->intransaction && $this->_error ) {
      return(false);
    }

    $this->_sql = null;
    $this->_error = null;
    $this->_statement = null;

    if( is_array($query) ) {
      $query = implode(PHP_EOL, $query);
    }
    if( is_string($query) ) {
      $this->_sql = $query;
      $this->_statement = $this->engine->query($query);
    } else {
      $this->_sql = 'unknown query type: ' . gettype($query);
    }

    if( $this->logsql ) {
      $this->spy($this->_sql);
    }

    if( !($this->_statement) ) {
      return $this->error($this->engine->errorInfo[2]);
    } elseif( $this->_statement->errorInfo[1] ) {
      return $this->error($this->_statement->errorInfo[2]);
    }
    $this->_lastid = $this->_statement->lastInsertId;
    $this->_affected = $this->_statement->rowCount;
    return($this->_statement);
  }

  /**
   * Function for prepare raw query.
   * 
   * @param array $args A list of arguments
   * @return string SQL query
   */
  private function prepareQuery( $args ) {
    if( $this->intransaction && $this->_error ) {
      return(false);
    }
    $this->_error = '';
    $raw = array_shift($args);

    $callback = function($match) use (&$args) {
      if( $this->_error ) {
        // do nothing
      } elseif( list($key, $value) = each($args) ) {
        switch( $match[1] ) {
          case 'n':
            $part = $this->escapeIdent($value);
            break;
          case 'f':
            $part = $this->createLIST($value);
            break;
          case 's':
            $part = $this->escapeString($value);
            break;
          case 'i':
            $part = $this->escapeInt($value);
            break;
          case 'a':
            $part = $this->createIN($value);
            break;
          case 'u':
            $part = $this->createSET($value);
            break;
          case 'w':
          case '&':
            $part = $this->createWHERE($value, 'AND');
            break;
          case 'y':
          case '|':
            $part = $this->createWHERE($value, 'OR');
            break;
          case 'r':
            if( is_object($value) ) {
              $value = \Helper::obj2arr($value);
            }
            if( is_array($value) ) {
              $part = implode(' ', $value);
              break;
            }
          case 'p':
            $part = $value;
            break;
          case '?':
            $part = $this->escape($value);
            break;
          default:
            $part = "/*({$match[0]}) unknown*/";
            $this->_error = "Unknown placeholder '{$match[0]}'";
        }
        return($part);
      } else {
        $this->_error = "Number of args doesn't match number of placeholders";
      }
      return("/*({$match[0]}) skipped*/");
    };

    return preg_replace_callback('~\?([nfsiuapwy\&\|\?])~ui', $callback, $raw);
  }

  private function escape( $value ) {
    if( is_numeric($value) ) {
      return($value);
    } else {
      return $this->escapeString($value);
    }
  }

  private function escapeInt( $value ) {
    if( $value === NULL ) {
      return 'NULL';
    }
    if( !is_numeric($value) ) {
      $this->_error = 'Integer (?i) placeholder expects numeric value, ' . ($type = gettype($value)) . ' given';
      return "/*(?i) {$type}*/";
    } elseif( is_float($value) ) {
      $value = number_format($value, 0, '.', ''); // may lose precision on big numbers
    }
    return $value;
  }

  private function escapeString( $value ) {
    if( $value === NULL ) {
      return 'NULL';
    }
    return $this->engine->quote($value);
  }

  private function escapeIdent( $value ) {
    if( $value && !is_numeric($value) && is_string($value) ) {
      return "`" . \implode('`.`', \explode('.', str_replace('`', '', $value))) . "`";
    }
    $this->_error = 'Identifier (?n) placeholder uncorrect value ' . ($type = gettype($value));
    return "/*(?n) {$type}*/";
  }

  private function createIN( $data ) {
    if( !is_array($data) ) {
      $this->_error = 'IN (?a) placeholder expects array, ' . ($type = gettype($value)) . ' given';
      return "/*(?a) {$type}*/";
    }
    if( !$data ) {
      return 'NULL';
    }
    $query = array();
    foreach( $data as $value ) {
      $query[] = $this->escape($value);
    }
    return implode(',', $query);
  }

  private function createSET( $data ) {
    if( !is_array($data) ) {
      $this->_error = 'SET (?u) placeholder expects array, ' . ($type = gettype($value)) . ' given';
      return "/*(?u) {$type}*/";
    }
    if( !$data ) {
      $this->_error = 'SET (?u) placeholder empty array';
      return "/*(?u) empty*/";
    }
    $query = array();
    foreach( $data as $key => $value ) {
      $query[] = $this->escapeIdent($key) . '=' . $this->escape($value);
    }
    return implode(',', $query);
  }

  private function createWHERE( $data, $glue ) {
    if( !is_array($data) ) {
      $this->_error = 'WHERE (?w) placeholder expects array, ' . ($type = gettype($value)) . ' given';
      return "/*(?w) {$type}*/";
    }
    if( !$data ) {
      $this->_error = 'WHERE (?w) placeholder empty array';
      return "/*(?w) empty*/";
    }
    $query = array();
    foreach( $data as $key => $value ) {
      $query[] = $this->escapeIdent($key) . '=' . $this->escape($value);
    }
    return '(' . implode(' ' . $glue . ' ', $query) . ')';
  }

  /**
   * Database selector.
   * 
   * Format:
   * event name="select" table="table_name" [group=""]... // selector
   *  where 
   *   uid 
   *   
   *   /uid
   *  /where
   *  field name="field_name" [editable="editable"] ... // on scalar field
   *  field name="field_name" ...
   *    // on complex field 
   *  /field
   * /event
   * 
   * @param DOMElement $node Event element
   */
  final public function event_select( $node ) {
    if( ($table = $node->getAttribute('table') ) ) {
      if( $wheres = $node->getElementsByTagName('where') ) {
        while( $wheres->length > 0 ) {
          $w_node = $wheres->item(0);
          if( !($glue = $w_node->getAttribute('connect')) ) {
            $glue = 'AND';
          }
          foreach( $w_node->childNodes as $cond ) {
            $fname = $cond->tagName;
            if( !($oper = $cond->getAttribute('operator')) ) {
              $oper = '=';
            }
          }
        }
      }
    }


    if( $group = $node->getAttribute('group') ) {
      
    }

    return(false);
  }

}