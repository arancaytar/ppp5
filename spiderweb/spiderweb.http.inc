<?php
// $Id$


/**
 * @file spiderweb.http.inc
 *   Contains functions for connecting to the Spiderweb forums via HTTP.
 */

define('SPIDERWEB_URL', 'http://www.spiderwebforums.com/forum');
define('SPIDERWEB_CHARSET', 'WINDOWS-1252');
define('PIPERBOT_USER_AGENT', 'PiperBot/0.1alpha (+http://dev.ermarian.net/ppp)');

/**
 * Get stored cookies, or store new cookie.
 * It will not log in automatically.
 *
 * @param $new_cookies
 *   When setting new cookies, pass them as an array [ string $name => string $value ],
 *
 * @return
 *   If cookies for login are stored or set, a string of cookies that can be sent to the server as is.
 */
function spiderweb_cookies($new_cookies = array()) {
  static $cookies = array();

  if ($new_cookies) {
    foreach ($new_cookies as $name => $info) {
      $cookies[$name] = $info;
    }
    variable_set('spiderweb_cookies', $cookies);
    return;
  }

  if (empty($cookies)) {
    $cookies = variable_get('spiderweb_cookies', array());
  }

  $out = array();
  foreach ($cookies as $name => $info) {
    $out[] = "$name={$info['value']}";
  }
  return implode('; ', $out);
}

/**
 * Perform a login at the Spiderweb boards. The resulting session cookies
 * will be automatically stored.
 *
 * @param string $user
 *   The login name of the account to use.
 * @param string $password
 *   The password of the account.
 *
 * @return
 *   The resulting session cookies.
 */
function spiderweb_login($user, $pass) {
  $form = array(
    'ubb' => 'start_page',
    'Loginname' => $user,
    'Loginpass' => $pass,
    'rememberme' => 1,
    'firstlogin' => 1,
    'ON_COMPLETION_URL' => '',
    'buttlogin' => 'Log In',
  );

  $response = spiderweb_post($form);
  return $response->cookies;
}

/**
 * Send a POST request to the Spiderweb forum.
 *
 * @param array $fields
 *   An array of form fields to send, [ string $name => string $value ]
 * @param string $page
 *   If specified, a non-default page on the server will be targeted. Default is ubbthreads.php
 * @param boolean $cookies
 *   [optional] If set to FALSE, spiderweb_cookies() will not be called. Beware of accidental recursion.
 *
 * @return
 *   A full HTTP response object as returned by drupal_http_request().
 */
function spiderweb_post($fields, $page = 'ubbthreads.php', $cookies = NULL) {
  $url = SPIDERWEB_URL . "/$page";
  $options = array(
    'method' => 'POST',
    'headers' => array(
      'Content-Type' => 'application/x-www-form-urlencoded',
      'Cookie' => $cookies,
    ),
    'data' => drupal_http_build_query($fields),
  );
  return drupal_http_request($url, $options);
}

/**
 * Send a GET request to the Spiderweb forum,
 *
 * @param array $fields
 *   An array of form fields to send, [ string $name => string $value ]
 *
 * @return
 *   A full HTTP response object as returned by drupal_http_request().
 */
function spiderweb_get($fields, $page = 'ubbthreads.php', $cookies = "") {
  $url = SPIDERWEB_URL . "/$page";
  if (!$cookies) $cookies = 'ubbt_myid=0; PHPSESSID=00000000000000000000000000000000';
  $options = array(
    'headers' => array(
      'Cookie' => $cookies,
      'User-Agent' => PIPERBOT_USER_AGENT,
    ),
  );
  $query = drupal_http_build_query($fields);
  $response = spiderweb_http_curl("$url?$query", $options);
  if (empty($response->data)) {
    throw new Exception("Received no data from Spiderweb. <pre>" . print_r($response, TRUE) . "</pre>");
  }

  // Make sure we get back valid UTF-8.
  // We can't trust UBB's headers. They claim ISO-8859-1, but send Cp1252/Windows.
  $response->data = iconv(SPIDERWEB_CHARSET, 'UTF-8', $response->data);

  // Also, decode all character references, as stored data will be re-encoded before output.
  // Convert only specific entities, to avoid re-encoding characters already in UTF-8
  $response->data = preg_replace_callback('/&[^;]+;/', '_spiderweb_decode_character', $response->data);
  return $response;
}

function _spiderweb_decode_character($reference) {
  return mb_convert_encoding($reference[0], 'UTF-8', 'HTML-ENTITIES');
}

function spiderweb_http_curl($url, $options) {
  $curl = &drupal_static(__FUNCTION__, curl_init());

  curl_setopt($curl, CURLOPT_URL, $url);
  curl_setopt($curl, CURLOPT_RETURNTRANSFER, TRUE);
  if (!empty($options['method'])) {
    curl_setopt($curl, CURLOPT_CUSTOMREQUEST, $options['method']);
  }
  if (!empty($options['headers'])) {
    $headers = array();
    foreach ($options['headers'] as $name => $value) {
      $headers[] = "$name: $value";
    }
    curl_setopt($curl, CURLOPT_HTTPHEADER, $headers);
  }
  if (!empty($options['data'])) {
    curl_setopt($curl, CURLOPT_POSTFIELDS, $options['data']);
  }

  $data = curl_exec($curl);
  return (object) (array('data' => $data));
}
