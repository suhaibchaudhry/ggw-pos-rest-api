<?php
//Privileged Roles
define("ROLE_ADMIN", 8);
define("ROLE_BACKOFFICE", 6);
define("ROLE_DEVELOPER", 3);
define("ROLE_FLOOR_MANAGER", 5);

//Unprivileged Roles
define("ROLE_FLOOR_AGENT", 4);

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

		//Check to make sure user has sufficient roles.
		if($user->login_valid == 1) {
			$privileged = db_result(db_query("SELECT COUNT(*) FROM users_roles WHERE uid = '%d' AND rid IN ('".ROLE_ADMIN."', '".ROLE_BACKOFFICE."', '".ROLE_DEVELOPER."', '".ROLE_FLOOR_MANAGER."')", $user->uid));

			$unprivileged = db_result(db_query("SELECT COUNT(*) FROM users_roles WHERE uid = '%d' AND rid  = '".ROLE_FLOOR_AGENT."'", $user->uid));

			$admin = db_result(db_query("SELECT COUNT(*) FROM users_roles WHERE uid = '%d' AND rid IN ('".ROLE_ADMIN."', '".ROLE_DEVELOPER."')", $user->uid));

			$last_ticket = db_fetch_object(db_query("SELECT tl.ticket_id, o.order_status, o.uid, o.product_count FROM pos_api_expose_ticket_log tl INNER JOIN uc_orders o ON tl.ticket_id = o.order_id WHERE tl.uid = '%d' ORDER BY tl.timestamp DESC LIMIT 0,1", $user->uid));
		}

		if($user->login_valid == 1 && ($privileged || $unprivileged)) {
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

			$response['privileged'] = (bool)$privileged;
			$response['admin'] = (bool)$admin;
			if(property_exists($last_ticket, 'ticket_id')) {
				$last_ticket->ticket_id = (int)$last_ticket->ticket_id;
			}
			if(property_exists($last_ticket, 'uid')) {
				$last_ticket->uid = (int)$last_ticket->uid;
			}
			if(property_exists($last_ticket, 'product_count')) {
				$last_ticket->product_count = (int)$last_ticket->product_count;
			}
			if($last_ticket->uid == 0 && $last_ticket->product_count == 0 && $last_ticket->order_status == 'pos_quote') {
				$time = time();
				db_query("UPDATE uc_orders SET `created` = '%s', `modified` = '%s' WHERE order_id = '%d'", $time, $time, $last_ticket->ticket_id);
			}
			$response['last_ticket'] = $last_ticket;

			//Send token back on authentication
			$response['token'] = $token;
			pos_api_expose_respond($response, true);
		}
	}

	pos_api_expose_respond($failed_response);
}