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

function pos_api_expose_new_ticket() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not create a new ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			//Initial ticket is always for guest customers for now. Be set active user on ticket id change to
			//ignore any active customer selections done while the ticket is being created in the db.

			$order = new stdClass();
			$order->uid = 0;
			$order->order_status = 'pos_in_progress';
			$order->order_total = 0;
			$order->product_count = 0;
			$order->delivery_zone = 0;
			$order->delivery_country = 840;
			$order->billing_zone = 0;
			$order->billing_country = 840;
			$order->data = array();
			$order->created = time();
			$order->modified = time();
			$order->host = ip_address();
			$order->currency = 'USD';

			drupal_write_record('uc_orders', $order);
			pos_api_expose_respond(array('status' => true,
										 'ticketId' => $order->order_id,
										 'ticketStatus' =>$order->order_status,
										 'customerUid' => $order->uid), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_update_customer() {
	//Todo: update prices on database based on user role using data from special price module.
	$failed_response = array(
		'status' => false,
		'error' => 'Could not update a ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$customerUid = $request->customerUid;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			if($ticketId) {
				db_query("UPDATE {uc_orders} SET uid = '%d' WHERE order_id = '%d'", $customerUid, $ticketId);
			
				if($customerUid) {
					//Get the minumum role price later like client, right now getting last.
					$rids = db_query("SELECT ur.rid FROM {users_roles} ur WHERE ur.uid = '%d'", $customerUid);

					while($role = db_fetch_object($rids)) {
						$rid = $role->rid;
						$nodes = db_query("SELECT uop.nid, upprp.price FROM {uc_order_products} uop 
							INNER JOIN {uc_price_per_role_prices} upprp on uop.nid = upprp.nid
							WHERE uop.order_id = '%d' AND upprp.rid = '%d'", $ticketId, $rid);
						while($node = db_fetch_object($nodes)) {
							if($node->price) {
								db_query("UPDATE {uc_order_products} SET price = '%f' WHERE order_id = '%d' AND nid = '%d'", $node->price, $ticketId, $node->nid);
							}
						}
					}
				} else {
					//Reset to sell price
					$nodes = db_query("SELECT uop.nid, up.sell_price FROM {uc_order_products} uop
							 INNER JOIN {uc_products} up ON uop.nid = up.nid
							 WHERE uop.order_id = '%d'", $ticketId);

					while($node = db_fetch_object($nodes)) {
						db_query("UPDATE {uc_order_products} SET price = '%f' WHERE order_id = '%d' AND nid = '%d'", $node->sell_price, $ticketId, $node->nid);
					}
				}
			}

			pos_api_expose_respond(array('status' => true, 'message' => 'Customer id has been set.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_update_product_qty() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not update a ticket product quantity.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$productId = $request->productId;
		$qty = $request->qty;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			if($ticketId) {
				db_query("UPDATE {uc_order_products} SET qty = '%d' WHERE order_id = '%d' AND nid = '%d'", $qty, $ticketId, $productId);
			}

			pos_api_expose_respond(array('status' => true, 'message' => 'Product qty updated.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_current() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not get ticket current customer.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			$sql = "SELECT uo.order_id, uo.uid, uo.order_status, os.title AS order_status_en
					FROM uc_orders uo
					INNER JOIN uc_order_statuses os ON uo.order_status = os.order_status_id
					WHERE uo.order_id = '%d'";
			$item = db_fetch_object(db_query($sql, $ticketId));
			pos_api_expose_respond(array('status' => true,
				'ticket' => array('ticketId' => $item->order_id,
										 'status' => $item->order_status,
										 'status_en' => $item->order_status_en,
										 'customerUid' => $item->uid)), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_search($token, $query) {
	if(!empty($token)) {
		$uid = _pos_api_expose_uid_from_token($token);

		if($uid) {
			//By Search Company Name with Fluid Revelevance
			$fulltext_sql = "SELECT nv.vid, MATCH(nv.title) AGAINST ('%s') AS relevance
							 FROM {node_revisions} nv
							 HAVING relevance > 0";

			//Search by Account ID
			$accountid_search = "SELECT n.vid, 10 AS relevance
								 FROM node n
								 INNER JOIN users u ON n.uid = u.uid
								 WHERE u.name LIKE '%s%%'";

			//Search by Phone Number
			$phone_searches = "SELECT ctp.vid, 9 AS relevance
							   FROM content_type_profile ctp
							   WHERE ctp.field_profile_company_phone_number LIKE '%s%%' OR
							   ctp.field_profile_phone_number LIKE '%s%%'";

			//Get all possible customers form search query.
			$ticket_customers = "SELECT u.uid
					FROM node n
					INNER JOIN users u ON n.uid = u.uid
					INNER JOIN ((".$fulltext_sql.") UNION (".$accountid_search.") UNION (".$phone_searches."))
					search ON search.vid = n.vid
					WHERE n.type = 'profile'";

			$sql = "SELECT order_id, uid, order_status, os.title AS order_status_en
					FROM uc_orders uo
					INNER JOIN uc_order_statuses os ON uo.order_status = os.order_status_id
					WHERE uo.uid IN (".$ticket_customers.") OR uo.order_id LIKE '%s%%'
					ORDER BY uo.modified DESC";

			$dataset = array();
			$items = db_query($sql, $query, $query, $query, $query, $query);

			while($item = db_fetch_object($items)) {
				$dataset[] = array('ticketId' => $item->order_id,
										 'ticketStatus' => $item->order_status,
										 'ticketStatus_en' => $item->order_status_en,
										 'customerUid' => $item->uid);
			}

			pos_api_expose_respond($dataset, true);
		}
	}

	pos_api_expose_respond(array());
}

function pos_api_expose_ticket_add_product() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not add product to ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);

		$uid = _pos_api_expose_uid_from_token($request->token);

		if($uid) {
			$order_product = new stdClass();
			$order_product->order_id = $request->ticketId;
			$order_product->nid = $request->productId;
			$order_product->title = $request->name;
			$order_product->model = $request->sku;
			$order_product->price = $request->price;

			$order_product->cost = 0;
			$order_product->manufacturer = '';
			$order_product->weight = 0;
			$order_product->qty = 1;
			$order_product->data = array(
				'attributes' => array()
			);

			drupal_write_record('uc_order_products', $order_product);

			pos_api_expose_respond(array('status' => true,
										 'ticketProductId' => $order->order_product_id), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_remove_product() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not remove product from ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$productId = $request->productId;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			db_query("DELETE FROM {uc_order_products} WHERE order_id = '%d' AND nid = '%d'", $ticketId, $productId);
			pos_api_expose_respond(array('status' => true,
										 'message' => 'Ticket product removed.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_update_totals() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not update totals on ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$total = $request->total;
		$product_count = $request->productCount;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			db_query("UPDATE {uc_orders} SET order_total = '%f', product_count = '%d' WHERE order_id = '%d'", $total, $product_count, $ticketId);
			pos_api_expose_respond(array('status' => true,
										 'message' => 'Ticket totals updated.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_update_zone() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not update delivery zone on ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			db_query("UPDATE {uc_orders} SET data = '%s' WHERE order_id = '%d'", serialize(array('zone' => 1)), $ticketId);
			pos_api_expose_respond(array('status' => true,
										 'message' => 'Ticket delivery zone updated.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}