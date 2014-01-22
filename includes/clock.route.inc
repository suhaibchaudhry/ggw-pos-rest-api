<?php
function pos_api_expose_clockState() {
	$request = json_decode($_POST['request']);
	$failed_response = array(
		'clock' => false,
		'lunch' => false
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;

		$uid = db_result(db_query("SELECT paet.uid FROM {pos_api_expose_tokens} paet WHERE paet.token = '%s'", $token));
		if($uid) {
			$last_event = last_event($uid, $event_type);

			//Default clock states for user
			$last_event_clock = last_event($uid, 'clock');
			$last_event_lunch = last_event($uid, 'lunch');

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

			pos_api_expose_respond($response, true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_clock() {
	$request = json_decode($_POST['request']);
	$failed_response = array(
		'checkin' => false,
		'error' => 'Provided login information is incorrect.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$event_type = $request->event_type;

		$uid = db_result(db_query("SELECT paet.uid FROM {pos_api_expose_tokens} paet WHERE paet.token = '%s'", $token));
		
		if($uid) {
			$last_event = last_event($uid, $event_type);

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