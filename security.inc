<?php

require_once('constants.inc');
require_once('Controller.inc');
require_once('Helper.inc');

/**
 * Core class for basic security level.
 *
 * <pre>
 * plugin name="security" checkip="on" users="user" access="access" email="webmaster@mousite.org"
 * </pre>
 *
 * @property-read string $access Name of access level
 * @property-read string $user Name of user
 * @property-read string $username Name of user
 * @property-read string $error Error text or ''
 * @property-read int $uid User ID
 * @property-read int $aid Access level ID
 * @property-read bool $refresh If need refresh stored session data
 */
class security
  extends \Controller {
  // $_SESSION['user'] == copy of $this->_user
  private $attrs = null;
  private $_user = null;
  private $_access = null;
  private $_error = null;
  private $_model = null;

  /**
   * Eliminate and process $_REQUEST['security'] to perform actions for any user.
   * 
   * Actions:
   * - logout
   * - register
   * - newpass
   * - login
   * or try login by session or cookie.
   * 
   * @param array $args Plugin attributes.
   */
  function __construct( $args = null ) {
    $this->_model = Core\Database::table('security');
    $this->attrs = (array) $args;
    if( !isset($_COOKIE['cid']) )
      setcookie('cid', \Helper::genPass(30, 30), PHP_INT_MAX, '/');
    if( $security = \Helper::getVar($_REQUEST, 'security') ) {
      if( isset($security['logout']) ) {
        $this->logoff($this->_error = 'logout');
      } elseif( isset($security['register']) ) {
        $this->spy('try register');
        $this->register($security);
      } elseif( isset($security['newpass']) ) {
        $this->spy('try newpass');
        $this->newpass($security);
      } elseif( isset($security['login']) && isset($security['password']) ) {
        $this->spy('try login');
        $this->logon($security['login'], $security['password']);
      }
    } elseif( $this->fastLogin() ) {
      $this->spy('fast login');
    } else {
      
    }
    register_shutdown_function(array(&$this, 'done'));
  }

  /**
   *  Store current user data into session
   */
  public function done() {
    $_SESSION['user'] = $this->_user;
    $_SESSION['access'] = $this->_access;
  }

  /**
   * Invoking.
   *
   * Process $_REQUEST['security'] to perform actions for logged user:
   * - confirm
   * - modify
   * 
   * @return $this
   */
  function __invoke( $args = null ) {
    if( $security = \Helper::eliminate($_REQUEST, 'security') ) {
      if( isset($security['confirm']) ) {
        $this->spy('try confirm');
        $this->confirm($security);
      } elseif( isset($security['modify']) ) {
        $this->spy('try modify');
        $this->modify($security);
      }
    }
    return($this);
  }

  /**
   * Readonly properties.
   */
  public function __get( $name ) {
    switch( $name ) {
      case 'error':
        return( $this->_error );
      case 'user':
      case 'username':
        return( \Helper::getVar($this->_user, 'nickname', 'Guest') );
      case 'uid':
        return( \Helper::getVar($this->_user, 'id', 0) );
      case 'refresh':
        return( \Helper::getVar($this->_user, 'refresh', 1) );
      case 'access':
        return( \Helper::getVar($this->_access, 'name', 'Guests') );
      case 'aid':
        return( \Helper::getVar($this->_access, 'id', 0) );
      default:
        return(parent::__get($name));
    }
  }

  /**
   * Set refresh flag for access level and descendants.
   *
   * @param int $aid Access level id (all users when 0)
   * @return bool TRUE on OK | FALSE on error
   */
  public function refreshAccess( $aid = 0 ) {
    if( $this->_model ) {
      if( $aid ) {
        if( $this->_model->request('refreshGroup')->execute([$aid, $aid]) ) {
          return $this->_model->affected;
        }
      } else {
        if( $this->_model->request('refreshAll')->execute() ) {
          return $this->_model->affected;
        }
      }
    }
    return(false);
  }

  /**
   * Log user action into 'history' table.
   *
   * @param string $parent Plugin context
   * @param string $action Performed action
   * @param array|string $info Info for store
   * @param int $userid Another user ID
   * @return bool TRUE on OK | FALSE on database error
   */
  public function notaBene( $parent, $action, $info = null, $userid = null ) {
    return $this->_model && $this->_model->request('history')->execute([$userid ? $userid : $this->uid, $parent, $action,
        \Helper::arr2ini(array_merge(array('ip' => $_SERVER['REMOTE_ADDR'], 'cid' => \Helper::getVar($_COOKIE, 'cid')), (array) $info))]);
  }

  /**
   * Conventional function to forget stored settings.
   *
   * @return bool TRUE (for chaining)
   */
  public function forget() {
    $this->_user = null;
    $this->_access = null;
    if( isset($_COOKIE['user']) ) {
      setcookie('user');
    }
    if( isset($_COOKIE['user_key']) ) {
      setcookie('user_key');
    }
    if( isset($_COOKIE['security']) ) {
      setcookie('security');
    }
    if( isset($_SESSION['user']) ) {
      unset($_SESSION['user']);
    }
    if( isset($_SESSION['access']) ) {
      unset($_SESSION['access']);
    }
    return(true);
  }

  /**
   * Perform internal logoff.
   *
   * @param string $why Cause for debugging
   * @return bool FALSE (for chaining)
   */
  public function logoff( $why = null ) {
    self::reset();
    $this->makeUser(array('id' => 0, 'nickname' => 'Guest', 'access' => 0, 'refresh' => 0), false);
    $this->spy($why);
    return false;
  }

  /**
   * Check if name is valid.
   * 
   * Not allows mixed register, allowed only letters, digits, spaces and '_', '-'.
   *
   * @return string on valid name | null otherwise
   */
  private function checkName( $str ) {
    if( \preg_match('/^([a-zA-Z0-9_\-\s]|[а-яА-ЯЁёЇїІіҐґЎў0-9_\-\s]){4,64}$/u', $str) ) {
      return($str);
    }
    return(null);
  }

  /**
   * Check if e-mail is valid.
   *
   * @return string on valid e-mail | null otherwise
   */
  private function checkEmail( $str ) {
    if( \preg_match(';^([a-z0-9!#$%&\'*+/=?^_`{|}~-]+(?:\.[a-z0-9!#$%&\'*+/=?^_`{|}~-]+)*@'
        . '(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+'
        . '(?:[A-Z]{2}|com|org|net|edu|gov|mil|biz|info|mobi|name|aero|asia|jobs|museum))$;i', $str) ) {
      return($str);
    }
    return(null);
  }

  /**
   *  Try login.
   *
   *  @param string $name User nickname or e-mail
   *  @param string $pass User plain password
   *  @return bool TRUE on OK | FALSE on error
   */
  private function logon( $name, $pass ) {
    self::reset();
    if( $name && $pass ) {
      if( $this->database ) {
        if( $nname = $this->checkName($name) ) {
          $where = array('nickname' => $nname);
        } elseif( $nname = $this->checkEmail($name) ) {
          $where = array('email' => $nname, 'emailcheck' => '');
        } else {
          return $this->logoff($this->_error = 'bad name ' . $name);
        }
        if( $user = $this->database->getRow('SELECT * FROM ?n WHERE ?&', $this->attrs['users'], $where) ) {
          $md5pass = md5($pass);
          if( ($user['md5pass'] == $md5pass) || ($user['newpass'] == $pass) ) {
            if( $user['newpass'] ) {
              $this->database->query('UPDATE ?n SET ?u WHERE `id`=?i', $this->attrs['users'], array('newpass' => '', 'md5pass' => $md5pass), $user['id']);
            }
            $this->notaBene(__NAMESPACE__, 'login', null, $user['id']);
            return $this->makeUser($user, \Helper::getVar($this->attrs, 'cookie'));
          } else {
            $this->notaBene(__NAMESPACE__, 'wrong', array('hack' => $pass), $user['id']);
            return $this->logoff($this->_error = 'wrong login');
          }
        } else {
          return $this->logoff($this->_error = 'unknown user');
        }
      } return $this->logoff($this->_error = 'misconfigured');
    } else {
      return $this->logoff($this->_error = 'empty field');
    }
    return true;
  }

  /**
   * Try login from stored session/cookie.
   *
   * @return bool TRUE on OK | FALSE on error
   */
  private function fastLogin() {
    $id = 0;
    $cookie = null;
    if( isset($_SESSION['user']) && (!isset($this->attrs['checkip']) || ($_SESSION['user']['ip'] == $_SERVER['REMOTE_ADDR']) ) ) {
      $id = $_SESSION['user']['id'];
      $this->spy('use session [' . $id . ']');
    }
    if( \Helper::getVar($this->attrs, 'cookie') && isset($_COOKIE['user']) && isset($_COOKIE['user_key']) && (!isset($_SESSION['user']) || ($id == $_COOKIE['user']) ) ) {
      $id = $_COOKIE['user'];
      $cookie = $_COOKIE['user_key'];
      $this->spy('use cookie [' . $id . ']');
    }
    if( $this->database && $id ) {
      if( ( $user = $this->database->getRow('SELECT * FROM ?n WHERE `id`=?i', $this->attrs['users'], $id)) && (!$cookie || ($user['cookie'] == $cookie) ) ) {
        $this->_access = \Helper::getVar($_SESSION, 'access');
        return $this->makeUser($user, $cookie);
      } else {
        $this->logoff($this->_error = 'identify error');
      }
    } else {
      return $this->logoff();
    }
  }

  /**
   * Make current user active from DB record.
   * Set user access rights.
   *
   * @return bool TRUE (for chaining)
   */
  private function makeUser( $rec, $rotate ) {
    $this->_user = $rec;
    if( isset($this->attrs['checkip']) ) {
      $this->_user['ip'] = $_SERVER['REMOTE_ADDR'];
    }
    $_SESSION['user'] = $this->_user;
    if( !$this->uid || !$this->_access || \Helper::getVar($this->_user, 'refresh', 1) ) {
      $_SESSION['access'] = $this->getAccess($rec['access']);
      $_SESSION['config'] = $_SESSION['expand'] = null;
      if( $this->uid ) {
        $this->database->query('UPDATE ?n SET `refresh`=0 WHERE `id`=?i', $this->attrs['users'], $this->uid);
        $this->_user['refresh'] = 0;
      }
    }
    $this->_access = $_SESSION['access'];

    if( $this->uid && $rotate && $this->database ) {
      $key = mt_rand();
      if( $this->database->query('UPDATE ?n SET ?u WHERE `id`=?i', $this->attrs['users'], array('cookie' => $key), $rec['id']) ) {
        setcookie('user', $_COOKIE['user'] = $this->uid);
        setcookie('user_key', $_COOKIE['user_key'] = $key);
      }
    }
    $this->spy('login [' . $this->uid . ']');
    return true;
  }

  /**
   * Get user access rights from database.
   *
   * @param int $access Access id
   * @param array $current Default return
   * @param bool $refresh Force refresh
   * @return array Access record
   */
  private function getAccess( $access ) {
    if( $this->database && ($acc = $this->database->getRow('SELECT * FROM ?n WHERE `id`=?i', $this->attrs['access'], $access)) ) {
      $acc['list'] = parse_ini_string($acc['list'], true);
      if( $acc['base'] && ($base = $this->getAccess($acc['base'])) ) {
        $acc['name'] = $base['name'] . '/' . $acc['name'];
        $acc['list'] = array_merge_recursive($base['list'], $acc['list']);
      }
      return($acc);
    }
    return(null);
  }

  /**
   *  Send e-mail from hostmaster to user.
   */
  private function sendEmail( $email, $subj, $html ) {
    // Content-type
    $headers[] = 'MIME-Version: 1.0';
    $headers[] = 'Content-type: text/html; charset=utf8';
    // From
    $headers[] = 'From: ' . $this->attrs['email'];
    // 
    mail($email, $subj, wordwrap($html, 70, PHP_EOL), implode(PHP_EOL, $headers));
  }

  /**
   *  Send e-mail to verify.
   */
  private function verifyEmail( $email, $echeck ) {
    $html = '<a href="' . $_SERVER['SERVER_NAME'] .
      '/user?security[email]=' . urlencode($email) .
      '&security[confirm]=' . urlencode($echeck) .
      '">Follow this link for confirm your e-mail</a>';
    $this->sendEmail($email, 'E-mail verification', $html);
  }

  /**
   *  Send e-mail to recover.
   */
  private function recoverEmail( $email, $name, $pass ) {
    $html = 'Request to password recover from IP=' . $_SERVER['REMOTE_ADDR'] . PHP_EOL .
      'The new password is "' . $pass . '"' . PHP_EOL . PHP_EOL .
      'If it is not your request - ignore it and use your old password to login.' . PHP_EOL . PHP_EOL .
      'If it is your request - ' . PHP_EOL .
      '<a href="' . $_SERVER['SERVER_NAME'] .
      '/user?security[login]=' . urlencode($name) .
      '&security[password]=' . urlencode($pass) .
      '"> Follow this link for login and change this autogenerated password </a>';
    $this->sendEmail($email, 'Recover password', $html);
  }

  /**
   * Sanitize user $name and $email fields.
   *
   * @param array $security User fields
   * @return array Sanitized $security
   */
  private function checkLogin( $security ) {
    $sname = \Helper::eliminate($security, 'nickname', '');
    $semail = \Helper::eliminate($security, 'email', '');
    $security['nickname'] = $this->checkName($sname) ? $sname : $this->checkName($semail);
    $security['email'] = $this->checkEmail($semail) ? $semail : $this->checkEmail($sname);
    return($security);
  }

  /**
   * Register new user.
   *
   * @param array $security User fields
   * @return bool TRUE on OK | FALSE on error
   */
  private function register( $security ) {
    $pass = \Helper::eliminate($security, 'password', '');
    $security = $this->checkLogin($security);
    if( $security['nickname'] && $pass && $this->database ) {
      // check doubles
      $fields = array();
      $fields['nickname'] = $security['nickname'];
      if( !($email = $fields['email'] = $security['email']) ) {
        $email = 'EMPTY_EMAIL';
      }
      if( !($user = $this->database->getRow('SELECT * FROM ?n WHERE `nickname`=?s OR `email`=?s', $this->attrs['users'], $security['nickname'], $email)) ) {
        $fields['md5pass'] = md5($pass);
        $fields['emailcheck'] = \Helper::genPass(32, 32);
        if( $fields['email'] && isset($this->attrs['email']) ) {
          $this->verifyEmail($fields['email'], $fields['emailcheck']);
        }
        $fields['access'] = DB_USER_ACCESS_USER;
        if( $this->database->query('INSERT INTO ?n SET ?u', $this->attrs['users'], $fields) ) {
          $fields['id'] = $this->database->lastid;
          $this->notaBene(__NAMESPACE__, 'register', null, $fields['id']);
          $fields['refresh'] = true;
          return $this->makeUser($fields, \Helper::getVar($this->attrs, 'cookie'));
        }
      } else {
        $this->_error = 'already registered';
        return $this->logon($security['nickname'], $pass);
      }
    }
    return $this->logoff($this->_error = 'not registered');
  }

  /**
   * Generate new password once per 24h and send e-mail for confirm.
   *
   * @param array $security User fields
   * @return void
   */
  private function newpass( $security ) {
    $security = $this->checkLogin($security);
    if( $this->database ) {
      if( ( $security['nickname'] && ($users = $this->database->getAll('SELECT * from ?n WHERE `nickname`=?s', $this->attrs['users'], $security['nickname'])) ) || ( $security['email'] && ($users = $this->database->getAll('SELECT * from ?n WHERE `email`=?s', $this->attrs['users'], $security['email'])) )
      ) {
        foreach( $users as $user ) {
          if( $user['emailcheck'] ) {
            $this->logoff($this->_error = 'no confirmed e-mail');
          } elseif( $user['newpass'] && (strtotime($user['timestamp']) + 86400 > time()) ) {
            $this->logoff($this->_error = 'enabled once per day');
          } else {
            $pass = \Helper::genPass();
            if( $this->database->query('UPDATE ?n SET `newpass`=?s WHERE `id`=?i', $this->attrs['users'], $pass, $user['id']) ) {
              $this->recoverEmail($user['email'], $user['nickname'], $pass);
            }
          }
          $this->notaBene(__NAMESPACE__, 'recover', array('user' => $user['nickname']), $user['id']);
        }
      } else {
        $this->logoff($this->_error = 'nothing restore');
      }
    } else {
      $this->logoff($this->_error = 'misconfigured');
    }
  }

  /**
   * Confirmation of registered e-mail.
   *
   * @param array $security User fields
   * @return void
   */
  private function confirm( $security ) {
    $security = $this->checkLogin($security);
    if( $this->database && ($email = $security['email']) && $this->database->query('UPDATE ?n SET `emailcheck`=?s WHERE `id`=?i AND `email`=?s AND `emailcheck`=?s', $this->attrs['users'], '', $user['id'], $security['email'], $security['confirm']) ) {
      if( $this->database->query('UPDATE ?n SET `access`=?i, `refresh`=1 WHERE `id`=?i AND `access`=?i', $this->attrs['users'], DB_USER_ACCESS_KNOWN, $user['id'], DB_USER_ACCESS_USER) ) {
        $this->_access = $this->getAccess(DB_USER_ACCESS_KNOWN);
      }
      $this->_error = 'confirmed';
    } else {
      $this->_error = 'not confirmed';
    }
  }

  /**
   * Update current password and/or e-mail.
   *
   * @param array $security User fields
   * @return void
   */
  private function modify( $security ) {
    $pass = \Helper::eliminate($security, 'password', '');
    $security = $this->checkLogin($security);
    if( $this->database && $this->uid && $pass ) {
      $fields = array();
      if( $security['password1'] && ($security['password1'] != $pass) && ($security['password1'] == $security['password2']) ) {
        // change password
        $fields['md5pass'] = md5($security['password1']);
        $fields['newpass'] = '';
      }
      if( $security['email'] ) {
        $fields['email'] = $security['email'];
        $fields['emailcheck'] = \Helper::genPass(32, 32);
      }
      if( $fields && $this->database->query('UPDATE ?n SET ?u WHERE `id`=?i AND `md5pass`=?s', $this->attrs['users'], $fields, $this->uid, md5($pass)) ) {
        if( $security['email'] && $this->database->query('UPDATE ?n SET `access`=?i, `refresh`=1 WHERE `id`=?i AND `access`=?i', $this->attrs['users'], DB_USER_ACCESS_USER, $user['id'], DB_USER_ACCESS_KNOWN) ) {
          $this->verifyEmail($fields['email'], $fields['emailcheck']);
          $this->_access = $this->getAccess(DB_USER_ACCESS_USER);
        }
        if( $this->uid ) {
          $this->notaBene(__NAMESPACE__, 'modify');
        }
        $this->_error = 'updated';
      } else {
        $this->_error = 'not updated';
      }
    } else {
      $this->_error = 'forbidden';
    }
  }

  /**
   * Check if user has rights for page, action and method.
   *
   * @param string $page Page name or '*'
   * @param string $action Action name or '*'
   * @param string $method Method name or '*'
   * @return bool TRUE on allowed | FALSE on forbidden
   */
  final function checkRights( $page = null, $action = null, $method = null ) {
    if( isset($this->_user) ) {
      if( !$page ) {
        $page = $this->page;
      }
      if( !$action ) {
        $action = $this->action;
      }
      if( !$method ) {
        $method = $this->method;
      }
      if( $this->aid < 0 ) {
        // master
        return(true);
      } elseif( ($page == 'user') || ($page == 'main') ) {
        // all can login or register
        return(true);
      } elseif( $this->_access && ($ptr = $this->_access['list']) ) {
        // $ptr array access by pages
        if( isset($ptr[$page]) ) {
          $ptr = $ptr[$page];
        } elseif( isset($ptr['*']) ) {
          $ptr = $ptr['*'];
        } else {
          return(false);
        }
        // $ptr array page access by actions
        if( isset($ptr[$action]) ) {
          $ptr = $ptr[$action];
        } elseif( isset($ptr['*']) ) {
          $ptr = $ptr['*'];
        } else {
          return(false);
        }
        // $ptr string action access allowed methods
        if( stripos($ptr, $method) !== false ) {
          return(true);
        } elseif( strpos($ptr, '*') ) {
          return(true);
        }
      }
      $this->_error = 'access forbidden';
    } else {
      $this->_error = 'need login';
    }
    return(false);
  }

}

?>