<?php
namespace Database;

/**
 * Field set for SELECT statements.
 * 
 */
class FieldSet
  implements \Countable, \ArrayAccess, \IteratorAggregate {
  private $set = [];
  private $hidden = [];
  private $protected = [];
  private $linked = [];

  function __construct( $set = [], $prefix = null ) {
    $this->add($set, $prefix);
  }

  function __get( $name ) {
    switch( $name ) {
      case 'protected':
        return $this->protected;
      case 'hidden':
        return $this->hidden;
      case 'linked':
        return $this->linked;
      default:
        if( array_key_exists($name, $this->set) ) {
          return $this->set[$name];
        }
    }
    return(null);
  }

  public function count( $mode = 'COUNT_NORMAL' ) {
    return count((array) $this->set);
  }

  public function getIterator() {
    return new \ArrayIterator((array) $this->set);
  }

  public function offsetSet( $offset, $value ) {
    $this->set[$offset] = $value;
  }

  public function offsetExists( $offset ) {
    return isset($this->set[$offset]);
  }

  public function offsetUnset( $offset ) {
    unset($this->set[$offset]);
  }

  public function offsetGet( $offset ) {
    return isset($this->set[$offset]) ? $this->set[$offset] : null;
  }

  /**
   * *MAGIC* string representation of set.
   * 
   * @return string '*' for empty set
   */
  function __toString() {
    if( $this->set ) {
      return implode(',', array_map(
          function($key, $val) {
          if( is_numeric($key) ) {
            return($val);
          } else {
            if( !$val ) {
              return "`$key`";
            }
            return "{$val} AS `{$key}`";
          }
        }, array_keys($this->set), $this->set));
    }
    return('*');
  }

  public function hide( $param ) {
    if( is_string($param) ) {
      $this->hidden[$param] = true;
    } elseif( $param ) {
      foreach( $param as $key ) {
        $this->hidden[(string) $key] = true;
      }
    }
    return $this->hidden;
  }

  public function linked( $name ) {
    return isset($this->linked[$name]) ? $this->linked[$name] : false;
  }

  /**
   * Add fields from string/array/SimpleXMLElement to the set.
   * 
   * Accept notations:
   * - as string, i.e. "id, price as theprice, max(term) as `maxterm`, `t`.`name` as tname"
   * - as array, i.e. ['id','theprice'=>'price', 'maxterm'=>'max(term)', 'tname'=>'`t`.`name`']
   * - as SimpleXMLElement, i.e. 
   * <pre>
   *  <userid type="int">id</userid>
   *  <name type="string" size="255" protected="protected">username</name>
   *  <password type="string" size="32" hidden="hidden">md5pass</password>
   *  <group type="int" link="tablegroup" hidden="hidden"></group>
   * </pre>   
   * 
   * @param mixed $fields SimpleXMLElement | array | string
   * @param string $prefix
   * @return FieldSet $this for chaining
   */
  public function add( $fields = [], $prefix = null ) {
    $match = [];
    if( is_string($fields) ) {
      $fields = preg_split('~[,\s]+~', $fields);
    }
    if( fields ) {
      foreach( $fields as $key => $val ) {
        $hidden = $protected = $linked = false;
        if( $val instanceof \SimpleXMLElement ) {
          $hidden = $val['hidden'];
          $protected = $val['protected'];
          $linked = $val['link'];
        }
        if( preg_match('~(.+)\s+AS\s+(\S+)~', (string) $val, $match) ) {
          $key = $match[2];
          $val = $match[1];
        }
        $key = str_replace('`', '', $key);
        if( $protected ) {
          $this->protected[$key] = true;
        }
        if( $linked ) {
          $this->linked[$key] = $linked;
        }
        if( $hidden ) {
          $this->hidden[$key] = true;
        } elseif( !$this->hidden || !array_key_exists($val, $this->hidden) ) {
          if( strpbrk($val, '.(),`' === FALSE) ) {
            $val = ($prefix ? "`{$prefix}`." : '') . "`{$val}`";
          }
          if( is_numeric($key) ) {
            $this->set[] = $val;
          } else {
            $this->set[$key] = $val;
          }
        }
      }
    }
    return($this);
  }

}

/**
 * Set for INSERT or UPDATE statements.
 * 
 */
class Set
  implements \Countable, \ArrayAccess, \IteratorAggregate {
  private $set = [];
  private $protected = [];

  function __construct( $array = [] ) {
    $this->add($array);
  }

  function __toString() {
    return implode(',', array_map(
        function($key, $val) {
        return "`{$key}`={$val}";
      }, array_keys($this->set), $this->set));
  }

  public function count( $mode = 'COUNT_NORMAL' ) {
    return count((array) $this->set);
  }

  public function getIterator() {
    return new \ArrayIterator((array) $this->set);
  }

  public function offsetSet( $offset, $value ) {
    $this->set[$offset] = $value;
  }

  public function offsetExists( $offset ) {
    return isset($this->set[$offset]);
  }

  public function offsetUnset( $offset ) {
    unset($this->set[$offset]);
  }

  public function offsetGet( $offset ) {
    return isset($this->set[$offset]) ? $this->set[$offset] : null;
  }

  /**
   * Set protected fields.
   * 
   * @param mixed $fields SimpleXMLElement | array | string
   */
  function protect( $fields ) {
    if( is_string($fields) ) {
      $fields = preg_split('~[,\s]+~', $fields);
    }
    if( $fields ) {
      foreach( $fields as $value ) {
        $this->protected[$value] = $value;
      }
    }
    return($this);
  }

  /**
   * Add elements to the set.
   * 
   * @param mixed $array SimpleXMLElement | array
   * @return type
   */
  function add( $array = [] ) {
    foreach( $array as $key => $field ) {
      if( $field instanceof \SimpleXMLElement ) {
        $key = $field['name'];
      }
      if( $key && !is_numeric($key) && !isset($this->protected[$key]) ) {
        $field = (string) $field;
        $this->set[$key] = $field;
      }
    }
    return($this);
  }

}

/**
 * Nested WHERE element.
 * 
 */
class Where {
  private $glue = '';
  private $where = [];

  function __construct( $array, $glue = '' ) {
    if(  is_string($array)) {
      $array = [$array];
    }
    $this->glue = $glue ? strtoupper($glue) : 'OR';
    foreach( $array as $where ) {
      if( ($where instanceof \SimpleXMLElement) && ($where->count() > 1) ) {
        $this->where[] = new Where($where->children(), $where['type']);
      } elseif( is_array($where) ) {
        $this->where[] = new Where($where, $this->glue == 'OR' ? 'AND' : 'OR');
      } else {
        $this->where[] = (string) $where;
      }
    }
  }

  function __toString() {
    if( count($this->where) > 1 ) {
      return '(' . implode(" {$this->glue} ", $this->where) . ')';
    } elseif( $this->where ) {
      return($this->where[0]);
    }
    return('');
  }

}

class Limitator {
  public $size = 0;
  public $page = 0;
  public $start = 0;
  public $limit = 0;

  function __construct( $param = null ) {
    if( $param ) {
      $this->size = isset($param['size']) ? $param['size'] : 0;
      $this->start = isset($param['start']) ? $param['start'] : 0;
      $this->limit = isset($param['limit']) ? $param['limit'] : 0;
      $this->page = isset($param['page']) ? $param['page'] : 0;
    }
  }

  function __invoke() {
    return ($this->size || $this->page || $this->start || $this->limit);
  }

  function __toString() {
    $ret = '';
    if( $this->__invoke() ) {
      if( $this->page ) {
        if( !$this->size ) {
          $this->size = 1;
        }
        $this->start = $this->size * ($this->page - 1);
        $this->limit = $this->size;
      }
      // MySQL format!!!
      if( $this->limit ) {
        $ret = "LIMIT {$this->limit}";
      }
      if( $this->start ) {
        $ret .= " OFFSET {$this->start}";
      }
    }
    return($ret);
  }

  public function clear() {
    $this->size = 0;
    $this->start = 0;
    $this->limit = 0;
    $this->page = 0;
  }

}

class Join {
  static private $instance = 0;
  private $type = '';
  private $table = '';
  private $alias = '';
  private $primary = '';
  private $fields = null;

  function __construct( $type, $table, $alias = null ) {
    $this->type = strtoupper($type);
    $this->table = isset($table['name']) ? $table['name'] : '';
    $this->primary = isset($table['primary']) ? $table['primary'] : '';
    $this->alias = $alias ? $alias : 'join' . self::$instance++;
    if( $table instanceof \SimpleXMLElement ) {
      $this->fields = new FieldSet(($table->fields->count() ? $table->fields->children() : []), $this->alias);
    } elseif( isset($table['fields']) ) {
      $this->fields = new FieldSet($table['fields'], $this->alias);
    } else {
      $this->fields = new FieldSet();
    }
  }

  function __invoke( $target ) {
    if( $this->table && $this->primary && $target ) {
      $link = '`' . implode('`.`', explode('.', str_replace('`', '', $target))) . '`';
      return "{$this->type} JOIN `{$this->table}` AS `{$this->alias}` ON `{$this->alias}`.`{$this->primary}`={$link}";
    }
    return('');
  }

  function fields( $array = [] ) {
    return $this->fields->add($array, $this->alias);
  }

}

/**
 * Database wrapper for build the query in the chain-style.
 * 
 * Example:
 * Core\Database::table('table')->where('price','<',$price)->limit(5)->select();
 * 
 * @property-read array $error Array of errors.
 * @property-read array $comment Array of comments.
 * @property-read integer $affected Affected rows (ALL).
 * @property-read integer $lastid Last insert id (INSERT).
 * @property-read integer $paginator Paginator (SELECT).
 */
class Query
  extends Core
  implements \ArrayAccess, \Countable, \IteratorAggregate {
  /**
   * Database controller.
   * @var \database
   */
  protected $database = null;
  /**
   * Array of errors.
   * @var string[] 
   */
  protected $error = [];
  /**
   * Array of comments
   * @var string[] 
   */
  protected $comments = [];
  /**
   * Array of result set.
   * @var array
   */
  protected $result = [];
  /**
   * Affected rows.
   * @var int
   */
  protected $affected = 0;
  /**
   * Last insert id.
   * @var int
   */
  protected $lastid = 0;
  /**
   * Table name in the database.
   * @var string
   */
  protected $table = '';
  /**
   * Table alias in the query.
   * @var string
   */
  protected $alias = '';
  /**
   * Filter for process each record in the recordset (SELECT).
   * @var callable
   */
  protected $filters = null;
  /**
   * Field list.
   * If the key is not numeric - used as a alias of the field.
   * Value is field real name or a function.
   * @var FieldSet
   */
  protected $fields = null;
  /**
   * Field list with values.
   * Key is the name of the field.
   * Value is new value.
   * @var Set
   */
  protected $sets = null;
  /**
   * Joins list.
   * @var array 
   */
  protected $joins = [];
  /**
   * Wheres list.
   * @var Where 
   */
  protected $wheres = null;
  /**
   * Havings list.
   * @var Where 
   */
  protected $havings = null;
  /**
   * Orders list.
   * @var array 
   */
  protected $orders = null;
  /**
   * Array of field names for grouping.
   * @var string[]
   */
  protected $astree = [];
  /**
   * Pagination element.
   * 
   * @var Limitator
   */
  protected $paginator = null;
  /**
   * Array of the raw parts for query build.
   * @var string[] 
   */
  protected $prepared = [];
  /**
   * Prepared PDOStatement.
   * @var PDOStatement
   */
  protected $ready = null;

  public function getIterator() {
    return new \ArrayIterator((array) $this->result);
  }

  public function count() {
    return count((array) $this->result);
  }

  public function offsetSet( $offset, $value ) {
    $this->result[$offset] = $value;
  }

  public function offsetExists( $offset ) {
    return isset($this->result[$offset]);
  }

  public function offsetUnset( $offset ) {
    unset($this->result[$offset]);
  }

  public function offsetGet( $offset ) {
    return isset($this->result[$offset]) ? $this->result[$offset] : null;
  }

  /**
   * @param database $database
   * @param string $name
   * @param string $alias
   */
  function __construct( $database, $name, $alias = '' ) {
    $this->database = $database;
    $this->table = $name;
    $this->alias = $alias ? $alias : $name;
    $this->fields = new FieldSet();
    $this->sets = new Set();
    $this->paginator = new Limitator();
  }

  function __get( $name ) {
    switch( $name ) {
      case 'error':
        return($this->error);
      case 'comment':
        return($this->comments);
      case 'affected':
      case 'count':
        return($this->affected);
      case 'lastid':
        return($this->lastid);
      case 'paginator':
        return $this->paginator;
    }
    return null;
  }

  /**
   * Get result as array.
   * 
   * @return array|false The result array | FALSE on error
   */
  function asArray() {
    if( $this->error ) {
      return(false);
    }
    return($this->result);
  }

  /**
   * Return JSON as string representation.
   * 
   * @return string
   */
  function __toString() {
    return $this->asJson();
  }

  /**
   * Get object as JSON.
   * 
   * @return string Object in JSON notation.
   */
  function asJson() {
    return json_encode([
      'error' => $this->error,
      'comment' => $this->comments,
      'lastid' => $this->lastid,
      'affected' => $this->affected,
      'paginator' => $this->paginator,
      'result' => $this->result,
    ]);
  }

  /**
   * Init before execution.
   * 
   * @param type $savePrepared True on the repeating request.
   * @return bool
   */
  protected function initQuery( $savePrepared = false ) {
    $this->result = [];
    $this->affected = 0;
    $this->lastid = 0;
    $this->comments = [];
    if( !$savePrepared ) {
      $this->ready = null;
      $this->prepared = [];
    }

    return(!$this->error);
  }

  /**
   * Perform preparing the links and limitations of the request.
   * 
   * @param bool $skipLimit True if skip 'LIMIT' part (for pagination).
   * @return string SQL part: JOIN... WHERE... HAVING... ORDER... LIMIT...
   */
  protected function preparedAll( bool $skipLimit = false ) {
    $prepared = [];
    if( $this->joins ) {
      foreach( $this->joins as $link => $join ) {
        $prepared[] = $join($link);
      }
    }
    // where
    if( $this->wheres && $this->wheres() ) {
      $prepared[] = 'WHERE ' . $this->wheres;
    }
    // having
    if( $this->havings && $this->havings() ) {
      $prepared[] = 'HAVING ' . $this->havings;
    }
    // order
    if( $this->orders ) {
      $prepared[] = 'ORDER';
      $arr = [];
      foreach( $this->orders as $order => $dir ) {
        $arr[] = "{$order} {$dir}";
      }
      $prepared[] = implode(',', $arr);
    }
    // limit
    if( !$skipLimit && $this->paginator() ) {
      $prepared[] = (string) $this->paginator;
    }
    return implode(' ', $prepared);
  }

  /**
   * Select associative array based on $index with values from $field.
   * 
   * Example:
   * <code>
   * $pairs = Core\Database::table('dictionary')->associate('term','meaning')->asArray();
   * </code>
   * 
   * @param string $index Key field name
   * @param string $field Value field name
   * @return Query|false This query | FALSE on error
   */
  public function associate( string $index, string $field ) {
    $this->initQuery();

    if( $result = $this->database->getIndCol($index, 'SELECT ?n, ?n FROM ?n ?p', $index, $field, $this->table, $this->preparedAll()) ) {
      $this->prepared[] = $this->database->sql;
      $this->result = $result;
      $this->affected = count($result);
      return($this);
    } else {
      $this->error[] = $this->database->error;
    }
    return false;
  }

  /**
   * Perform SELECT on prepared query.
   * 
   * Example:
   * <code>
   * echo Core\Database::table('mytable')->select()->asJSON();
   * </code>
   * 
   * @param mixed $fields Fields for adding to prepared list or numeric page number (see pagination)
   * @return Query | false
   */
  public function select( $fields = [] ) {
    $this->initQuery();
    if( is_numeric($fields) ) {
      $this->paginator->page = intval($fields);
    } else {
      $this->fields->add($fields);
    }

    if( $this->astree ) {
      $result = $this->database->getInd($this->astree, 'SELECT ?p FROM ?n ?p', (string) $this->fields, $this->table, $this->preparedAll());
    } else {
      $result = $this->database->getAll('SELECT ?p FROM ?n ?p', (string) $this->fields, $this->table, $this->preparedAll());
    }
    $this->prepared[] = $this->database->sql;

    if( is_array($result) ) {
      if( is_callable($this->filters) ) {
        array_walk($result, $this->filters, $this);
        $this->result = array_filter($result);
      } else {
        $this->result = $result;
      }
      $this->affected = count($this->result);
      return $this;
    }

    $this->error[] = $this->database->error;
    return false;
  }

  /**
   * Perform DELETE on prepared query.
   * 
   * Example:
   * <code>
   * $result = Core\Database::table('queue')->where('finished = 1')->delete();
   * </code>
   * 
   * @return Query|false
   */
  public function delete() {
    $this->initQuery();

    if( $result = $this->database->query('DELETE FROM ?n ?p', $this->table, $this->preparedAll()) ) {
      $this->prepared[] = $this->database->sql;
      $this->affected = $this->database->affectedRows;
      return $this;
    }

    $this->error[] = $this->database->error;
    return false;
  }

  /**
   * Perform UPDATE on prepared query.
   * 
   * Example:
   * <code>
   * $result = Core\Database::table('women')->where('groomed = 1')->update(['married'=>'married+1','groomed'=>0]);
   * </code>
   * 
   * @param array $fields Fields values as key=>value pairs
   * @return Query|false
   */
  public function update( array $fields ) {
    $this->initQuery();

    if( $result = $this->database->query('UPDATE ?n SET ?u ?p', $this->table, $this->set($fields), $this->preparedAll()) ) {
      $this->prepared[] = $this->database->sql;
      $this->affected = $this->database->affectedRows;
      return $this;
    }

    $this->error[] = $this->database->error;
    return false;
  }

  /**
   * Perform INSERT on prepared query.
   * 
   * Example:
   * <code>
   * $result = Core\Database::table('men')->insert(['name'=>$name,'age'=>$age]);
   * </code>
   * 
   * @param array $fields Fields values for insert as key=>value pairs
   * @param array $conflicted Fields values on the unique conflict as key=>value pairs
   * @return Query|false
   */
  public function insert( array $fields ) {
    $this->initQuery();

    if( $result = $this->database->query('INSERT INTO ?n SET ?u', $this->table, $this->set($fields)) ) {
      $this->prepared[] = $this->database->sql;
      $this->affected = $this->database->affectedRows;
      $this->lastid = $this->database->lastId;
      return $this;
    }

    $this->error[] = $this->database->error;
    return false;
  }

  /**
   * Alias for {@see prepare}.
   * 
   * @param string $sql In the extended notation for parsing.
   * @param array $params
   * @return Query | false
   */
  public function raw( string $sql, array $params = [] ) {
    $this->prepare($sql, $params);
  }

  /**
   * Add raw element to query.
   *  
   * Example:
   * <code>
   * $langcode = 'sw';
   * $request = Core\Database::table()->prepare('INSERT INTO ?n',['table_'.$langcode])->prepare('SET ?n=?, ?n=?',['word','meaning']);
   * </code>
   * will produce the prepared query for PDO executing {@see prepare}:
   * <pre>
   * INSERT INTO `table_sw` 
   * SET `word`=?, `meaning`=?
   * </pre>
   * 
   * @param string $sql In the extended notation for parsing.
   * @param array $params Parameters for extended notation.
   * @return Query | false
   */
  public function prepare( string $sql, array $params = [] ) {
    $this->ready = null;
    array_unshift($params, $sql);
    if( ($prepared = $this->database->parse($params)) && !$this->database->error ) {
      $this->prepared[] = $prepared;
      return($this);
    } elseif( $this->database->error ) {
      $this->error[] = $this->database->error;
    }
    return false;
  }

  /**
   * Perform new raw query or execute prepared.
   * 
   * Example:
   * use {@see prepare} for preparing and then use
   * <code>
   * foreach($dictionary as $word=>$meaning) {
   *    $request->execute($word,$meaning);
   * }
   * </code>
   * 
   * @param array $params For PDO notation
   * @return Query | false
   */
  public function execute( array $params = [] ) {
    $this->initQuery(true);

    if( $this->ready ) {
      if( !$this->database->execute($params, $this->ready) ) {
        $this->error[] = $this->database->error;
        return(false);
      }
    } else {
      if( !$this->prepared ) {
        $this->prepared = $this->prepareQuery();
      }
      if( !($this->ready = $this->database->perform(implode(' ', $this->prepared), $params)) ) {
        $this->error[] = $this->database->error;
        return(false);
      }
    }
    if( $this->result = $this->database->fetchAll(PDO::FETCH_ASSOC) ) {
      $this->affected = count($this->result);
    } else {
      $this->affected = $this->database->affectedRows;
    }
    $this->lastid = $this->database->lastId;
    return($this);
  }

  /**
   * Set pagination for query.
   * 
   * @param int $pagesize Page size (default=10)
   * @return Query
   */
  public function paginate( int $pagesize = 10 ) {
    $this->ready = null;
    $this->paginator->size = $pagesize;
    return($this);
  }

  /**
   * Set active page for query.
   * 
   * @param int $page
   * @return Query
   */
  public function page( $page = 0 ) {
    $this->ready = null;
    $this->paginator->page = $page;
    return($this);
  }

  /**
   * Set start (OFFSET) for query.
   * 
   * @param int $start
   * @return Query
   */
  public function start( $start = 0 ) {
    $this->ready = null;
    $this->paginator->start = $start;
    return($this);
  }

  /**
   * Set LIMIT for query.
   * 
   * @param int $limit
   * @return Query
   */
  public function limit( $limit = 0 ) {
    $this->ready = null;
    $this->paginator->limit = $limit;
    return($this);
  }

  /**
   * Count records for prepared query.
   * 
   * @param string $name Field name for COUNT()
   * @return int
   */
  public function pagesCount( $name = '*' ) {
    if( $row = $this->database->getRow("SELECT count({$name}) as `count` FROM ?n ?p", $this->table, $this->preparedAll(true)) ) {
      return $row['count'];
    }
    return(0);
  }

  /**
   * Link JOIN from model.
   * 
   * @param string $type
   * @param string $link
   * @param mixed $table SimpleXMLElement | array
   * @param string $alias
   * @return Query | false
   */
  public function link( $type, $link, $table, $alias = null, $fields = false ) {
    $this->ready = null;
    if( $table ) {
      $join = new Join($type, $table, $alias);
      $this->joins[$link] = $join;
      if( $fields ) {
        $this->fields->add($join->fields());
      }
      return($this);
    }
    $this->error[] = 'No model for join ' . $link;
    return(false);
  }

  /**
   * Add a raw JOIN to the query.
   * 
   * @param string $type
   * @param string $link
   * @param string $table
   * @param string $primary
   * @param string $alias
   * @param FieldSet $fields
   * @return Query
   */
  public function join( $type, $link, $table, $primary, $alias = null, $fields = [] ) {
    $this->ready = null;
    $join = new Join($type, ['name' => $table, 'primary' => $primary, 'fields' => $fields], $alias);
    $this->joins[$link] = $join;
    if( $fields ) {
      $this->fields->add($join->fields());
    }
    return($this);
  }

  /**
   * Add WHERE to the query.
   * 
   * @param mixed $param String | array | SimpleXMLElement | Where
   * @param string $glue
   * @return Query
   */
  public function where( $param, $glue = '' ) {
    $this->ready = null;
    $this->wheres[] = new Where((array) $param, $glue);
    return($this);
  }

  /**
   * Add HAVING to the query.
   * 
   * @param mixed $param String | array | SimpleXMLElement | Where
   * @param string $glue
   * @return Query
   */
  public function having( $param, $glue = '' ) {
    $this->ready = null;
    $this->havings[] = new Where((array) $param, $glue);
    return($this);
  }

  /**
   * Add ORDER to the query.
   * 
   * @param mixed $_ Unlimited number of arguments.
   * @return Query
   */
  public function order( $_ = null ) {
    $this->ready = null;
    $match = [];
    foreach( func_get_args() as $element ) {
      if( is_string($element) ) {
        $parts = preg_split('~[,\s]+~i', $element);
      } else {
        $parts = (array) $element;
      }
      foreach( $parts as $part ) {
        preg_match('~([\+\-]?)(\S*)~i', $part, $match);
        if( $match[1] == '-' ) {
          $this->orders[$match[2]] = 'DESC';
        } else {
          $this->orders[$match[2]] = '';
        }
      }
    }
    return($this);
  }

}

/**
 * The Query model.
 * 
 */
class Model
  extends Query {
  /**
   * @var SimpleXMLElement[] 
   */
  protected static $models = [];
  private $name = '';

  /**
   * @param database $database
   * @param SimpleXMLElement $model
   */
  function __construct( $database, $model ) {
    $this->database = $database;
    $this->addModel($model);
  }

  /**
   * @param SimpleXMLElement $model
   */
  public function add( $model ) {
    $this->name = $name = $model['name'];
    if( !array_key_exists($name, self::$models) ) {
      self::$models[$name] = $model;
      while( count($model->model) ) {
        $include = $model->model[0];
        if( $filename = $this->database->findModel($include['name'] . '.xml') ) {
          $this->addModel(simplexml_load_file($filename));
        } else {
          $this->comment[] = 'Model not found: ' . $include['name'];
        }
        unset($model->model[0]);
      }
    }
    return($this);
  }

  /**
   * @param string $request Request name: 'modelName.sqlName' for find in the model,
   *  'sqlName' for find across all models.
   * @param array $params Parameters for request.
   * @return Query|false FALSE on model not found.
   */
  public function request( $request, $params = [] ) {
    list($fname, $lname) = explode('.', $request);
    if( !$lname ) {
      foreach( self::$models as $model ) {
        if( ($sql = $model->xpath("//sql[@name='{$fname}']")) && count($sql) ) {
          return $this->activateRequest($sql[0], $params);
        }
      }
    } elseif( array_key_exists($fname, self::$models) ) {
      if( ($sql = $model->xpath("//sql[@name='{$lname}']")) && count($sql) ) {
        return $this->activateRequest($sql[0], $params);
      }
    }
    return(false);
  }

  /**
   * 
   * @param SimpleXMLElement $xml
   * @param array $param
   */
  private function activateRequest( $xml, $param ) {
    $this->initQuery();
    $type = $xml['type'];
    $this->alias = $xml['table'];
    if( $table = $this->useTable($this->alias) ) {
      $this->table = $table['name'];
      switch( $type ) {
        case 'insert':
          return $this->insert($xml->set);
        case 'delete':
          return $this->where($xml->where)->delete();
        case 'update':
          return $this->where($xml->where)->update($xml->set);
        case 'select':
          if( count($xml->fields) && $xml->fields[0]->count() ) {
            $this->fields = new FieldSet($xml->fields[0], $this->alias);
          } else {
            $this->fields = new FieldSet($table->fields, $this->alias);
          }
          if( count($xml->link) ) {
            foreach( $xml->link as $link ) {
              $name = $link['name'];
              $alias = $link['alias'] ? $link['alias'] : $name;
              if( $linked = $this->fields->linked($name) ) {
                $joined = $this->useTable($linked);
                $this->join($link['type'], $this->alias . '.' . $name, $joined['name'], $joined['primary'], $alias, $link->children());
              }
            }
          }
          return $this->
            $this->prepared[] = 'SELECT';
          if( count($xml->link) ) {
            foreach( $xml->link as $link ) {
              $name = $link['name'];
              $alias = $link['alias'] ? $link['alias'] : $name;
              if( $linked = $this->fields->linked($name) ) {
                $joined = $this->useTable($linked);
                $this->join($link['type'], $this->alias . '.' . $name, $joined['name'], $joined['primary'], $alias, $link->children());
              }
            }
          }
          $this->prepared[] = (string) $this->fields;
          $this->prepared[] = "FROM `{$this->table}`";
          if( $this->joins ) {
            foreach( $this->joins as $link => $join ) {
              $this->prepared[] = $join($link);
            }
          }
          $this->where($xml->where);
          if( $this->wheres() ) {
            $this->prepared[] = 'WHERE ' . $this->where;
          }
          $this->having($xml->having);
          if( $this->havings() ) {
            $this->prepared[] = 'HAVING ' . $this->havings;
          }
          $this->order($xml->order);
          if( $this->orders ) {
            $this->prepared[] = 'ORDER'
              . implode(',', array_map($this->orders, function($val, $key) {
                  return $val ? $key . ' ' . $val : $key;
                }));
          }
          $this->pager($xml->pager);
          if( $this->paginator() ) {
            $this->prepared[] = (string) $this->paginator;
          }
          break;
      }
      return $this->execute($param);
    }
    return(false);
  }

  /**
   * Use table from specified model.
   * 
   * @param type $modelName
   * @return SimpleXMLElement|null Table model or null
   */
  private function useTable( $modelName ) {
    if( array_key_exists($modelName, self::$models) && count(self::$models[$modelName]->table) ) {
      return self::$models[$modelName]->table[0];
    }
    return(null);
  }

  public function pager( $param ) {
    $this->paginator = new Limitator($param);
  }

}
