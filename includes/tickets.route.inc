<?php
function pos_api_expose_ticket_statuses() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not load ticket stasuses.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			$ticket_stasuses = array();
			$sql = "SELECT order_status_id, title FROM {uc_order_statuses}";
			$order_stasuses = db_query($sql);
			while($order_stasus = db_fetch_object($order_stasuses)) {
				if(strpos($order_stasus->title, 'Ecom ::') === FALSE) { //Take away ecommerce ticket stasuses.
					$ticket_stasuses[$order_stasus->order_status_id] = $order_stasus->title;
				}
			}

			pos_api_expose_respond(array('status' => true, 'stasuses' => $ticket_stasuses), true);
		}
	}

	pos_api_expose_respond($failed_response);
}