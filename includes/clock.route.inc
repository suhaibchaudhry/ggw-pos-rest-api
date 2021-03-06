<?php
//Privileged Roles
define("ROLE_ADMIN", 8);
define("ROLE_BACKOFFICE", 6);
define("ROLE_DEVELOPER", 3);
define("ROLE_FLOOR_MANAGER", 5);

//Unprivileged Roles
define("ROLE_FLOOR_AGENT", 4);
define("ROLE_INVENTORY_MANAGER", 16);

function pos_api_expose_clockState() {
	$failed_response = array(
		'clock' => false,
		'lunch' => false
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$register_id = $request->register_id;

		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			$privileged = db_result(db_query("SELECT COUNT(*) FROM users_roles WHERE uid = '%d' AND rid IN ('".ROLE_ADMIN."', '".ROLE_DEVELOPER."', '".ROLE_FLOOR_MANAGER."')", $uid));

			$admin = db_result(db_query("SELECT COUNT(*) FROM users_roles WHERE uid = '%d' AND rid IN ('".ROLE_ADMIN."', '".ROLE_DEVELOPER."')", $uid));

			$inventory = db_result(db_query("SELECT COUNT(*) FROM users_roles WHERE uid = '%d' AND rid IN ('".ROLE_INVENTORY_MANAGER."')", $uid));

			$last_ticket = db_fetch_object(db_query("SELECT tl.ticket_id, o.order_status, o.uid, o.product_count FROM pos_api_expose_ticket_log tl INNER JOIN uc_orders o ON tl.ticket_id = o.order_id WHERE tl.uid = '%d' ORDER BY tl.timestamp DESC LIMIT 0,1", $uid));

			$last_event = _pos_api_expose_last_event($uid, $event_type);

			//Default clock states for user
			$last_event_clock = _pos_api_expose_last_event($uid, 'clock');
			$last_event_lunch = _pos_api_expose_last_event($uid, 'lunch');

			$response = array();
			
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
			$response['inventory'] = (bool)$inventory;
			if($last_ticket) {
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
			}
			if($last_ticket->ticket_id > 0 && !empty($register_id)) {
				$lock_status = file_get_contents("http://www.general-goods.com/lock/index.php?ticket_id=".$last_ticket->ticket_id."&register_id=".$register_id."&op=acquire");
				if(!empty($lock_status)) {
					$lock_status = json_decode($lock_status);
					if(!$lock_status->status) {
						$last_ticket = false;
					}
				}
			}
			$response['last_ticket'] = $last_ticket;

			pos_api_expose_respond($response, true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_clock() {
	$failed_response = array(
		'checkin' => false,
		'error' => 'Provided login information is incorrect.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$event_type = $request->event_type;

		$uid = _pos_api_expose_uid_from_token($token);
		
		if($uid) {
			$last_event = _pos_api_expose_last_event($uid, $event_type);

			if(!empty($last_event->checkin) && empty($last_event->checkout)) {
				db_query("UPDATE {pos_api_expose_timesheet} SET checkout = '%s' WHERE eid = '%d'", time(), $last_event->eid);
				$checkin = false;
			} else {
				db_query("INSERT INTO {pos_api_expose_timesheet} (uid, checkin, event_type) VALUES ('%d', '%s', '%s')", $uid, time(), $event_type);
				$checkin = true;
			}

			$response = array(
				'checkin' => $checkin,
			);

			pos_api_expose_respond($response, true);
		}
	}

	pos_api_expose_respond($failed_response);
}