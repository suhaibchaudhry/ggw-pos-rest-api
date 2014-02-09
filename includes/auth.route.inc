<?php
function pos_api_expose_auth() {
	$failed_response = array(
		'login' => false,
		'error' => 'Provided login information is incorrect.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$uname = $request->uname;
		$pass = md5($request->pass);
		
		$user = db_fetch_object(db_query("SELECT u.uid, COUNT(*) AS login_valid FROM {users} u WHERE u.name = '%s' AND u.pass = '%s'", $uname, $pass));
		
		if($user->login_valid == 1) {
			$response = array(
				'login' => true,
				'id' => $user->uid,
				'uname' => $uname
			);
			
			//Fill in auth test and populate token or return existing token. Use 
			$token_count = db_result(db_query("SELECT COUNT(*) FROM {pos_api_expose_tokens} paet WHERE paet.uid = '%d'", $user->uid));

			if($token_count > 0) {
				$token = db_result(db_query("SELECT paet.token FROM {pos_api_expose_tokens} paet WHERE paet.uid = '%d'", $user->uid));
			} else {
				$token = sha1(uniqid($user->uid.'_', true));
				db_query("REPLACE INTO {pos_api_expose_tokens} VALUES ('%d', '%s')", $user->uid, $token);
			}

			//Default clock states for user
			$last_event_clock = _pos_api_expose_last_event($user->uid, 'clock');
			$last_event_lunch = _pos_api_expose_last_event($user->uid, 'lunch');

			if($last_event_clock->checkin) {
				$response['clock'] = !(bool)$last_event_clock->checkout;
			} else {
				$response['clock'] = false;
			}

			if($last_event_lunch->checkin) {
				$response['lunch'] = !(bool)$last_event_lunch->checkout;
			} else {
				$response['lunch'] = false;
			}

			//Send token back on authentication
			$response['token'] = $token;
			pos_api_expose_respond($response, true);
		}
	}

	pos_api_expose_respond($failed_response);
}