<?php
define("CUSTOMER_NO_TAX", 11);
define("CUSTOMER_TAX", 9);

function _pos_api_expose_get_roles($uid) {
	$user_roles = array();
	$sql = "SELECT rid FROM {users_roles} ur WHERE ur.uid = '%d'";
	$roles = db_query($sql, $uid);
	while($role = db_fetch_object($roles)) {
		$user_roles[] = $role->rid; 
	}

	return $user_roles;
}

function _pos_api_expose_order_history($user) {
  $header = array(
    array('data' => t('Date'), 'field' => 'o.created', 'sort' => 'desc'),
    array('data' => t('Order #'), 'field' => 'o.order_id'),
    array('data' => t('Status'), 'field' => 'os.title'),
    array('data' => t('Products'), 'field' => 'products'),
    array('data' => t('Total'), 'field' => 'o.order_total')
  );

  $rows = array();

  $context = array(
    'revision' => 'themed-original',
    'type' => 'amount',
  );

  $result = pager_query("SELECT o.order_id, o.uid, o.created, os.title, SUM(op.qty) AS products, o.order_total AS total, o.order_status FROM {uc_orders} AS o LEFT JOIN {uc_order_statuses} AS os ON o.order_status = os.order_status_id LEFT JOIN {uc_order_products} AS op ON o.order_id = op.order_id WHERE o.uid = %d AND o.order_status IN ('pos_completed', 'pos_in_progress', 'pos_quote', 'pos_return', 'pos_return_closed') GROUP BY o.order_id, o.uid, o.created, os.title, o.order_total, o.order_status". tablesort_sql($header), 20, 0, "SELECT COUNT(*) FROM {uc_orders} WHERE uid = %d AND order_status NOT IN ". uc_order_status_list('specific', TRUE), $user->uid);

  // Build a table based on the customer's orders.
  while ($order = db_fetch_object($result)) {
    $context['subject'] = array('order' => $order);

    $link = $order->order_id;

    if (user_access('view all orders')) {
      $link .= '<span class="order-admin-icons">'. uc_order_actions($order, TRUE) .'</span>';
    }

    $rows[] = array(
      array('data' => format_date($order->created, 'custom', variable_get('uc_date_format_default', 'm/d/Y'))),
      array('data' => $link),
      array('data' => check_plain($order->title)),
      array('data' => (!is_null($order->products) ? $order->products : 0), 'align' => 'center'),
      array('data' => uc_price($order->total, $context), 'align' => 'right'),
    );
  }

  if (empty($rows)) {
    $rows[] = array(array('data' => t('No orders available.'), 'colspan' => 5));
  }

  $content = theme('table', $header, $rows, array('class' => 'pure-table uc-order-history'))
       . theme('pager', NULL, 20, 0);

  return str_replace('src="/misc/', 'src="images/misc/', $content);
}

function pos_api_expose_customer_invoices() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not get customer invoices.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$customer_uid = $request->customer_uid;
		$uid = _pos_api_expose_uid_from_token($request->token);

		if($uid) {
			if($customer_uid) {
				$customer = user_load($customer_uid);
				$invoices = _pos_api_expose_order_history($customer);
				pos_api_expose_respond(array('status' => true, 'invoices' => $invoices), true);
			}
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_customer_block() {
	$failed_response = array(
		'status' => false,
		'message' => 'Could not get disable customer.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$customer_uid = $request->customer_uid;
		$uid = _pos_api_expose_uid_from_token($request->token);

		if($uid) {
			if($customer_uid) {
				db_query("UPDATE {users} u SET u.status = '0' WHERE u.uid = '%d'", $customer_uid);
				pos_api_expose_respond(array('status' => true, 'message' => 'Customer was disabled.'), true);
			}
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_customer_info_payments() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not get customer payment info.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$customer_uid = $request->customer_uid;
		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			if($customer_uid) {
				$customer = user_load($customer_uid);
				$rma_credits = db_result(db_query("SELECT IFNULL(SUM(total_refund),0) FROM {pos_api_expose_rma_refund_log} WHERE cuid = '%d' AND credit_usage_id = '0'", $customer_uid));
				pos_api_expose_respond(array('status' => true, 'payments' => utc_credit_report_pos($customer), 'rma_credits' => $rma_credits), true);
			}
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_customer_info_settlements() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not get customer payment info.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$customer_uid = $request->customer_uid;
		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			if($customer_uid) {
				pos_api_expose_respond(array('status' => true, 'payments' => utc_generate_credits_table($customer_uid, false, 10)), true);
			}
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_customer_info() {
	module_load_include('inc', 'user', 'user.pages');

	$failed_response = array(
		'status' => false,
		'error' => 'Could not get customer info.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$customer_uid = $request->customer_uid;
		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			if($customer_uid) {
				$customer = user_load($customer_uid);
				$customer_info = user_view($customer);
				$invoices = _pos_api_expose_order_history($customer);
				$payments = utc_credit_report_pos($customer);
				$rma_credits = db_result(db_query("SELECT IFNULL(SUM(total_refund),0) FROM {pos_api_expose_rma_refund_log} WHERE cuid = '%d' AND credit_usage_id = '0'", $customer_uid));
				pos_api_expose_respond(array('status' => true, 'customer_info' => $customer_info, 'invoices' => $invoices, 'payments' => $payments, 'rma_credits' => $rma_credits), true);
			}
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_credit_summary() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not get customer credit summary.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$customer_uid = $request->customer;
		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			$user = user_load($customer_uid);
			$rma_credits = db_result(db_query("SELECT IFNULL(SUM(total_refund),0) FROM {pos_api_expose_rma_refund_log} WHERE cuid = '%d' AND credit_usage_id = '0'", $customer_uid));
			pos_api_expose_respond(array('status' => true, 'credit_limits' => $user->credit_limits, 'rma_credits' => $rma_credits), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function _pos_api_expose_map_customer_datum($item) {
	return array(
				'id' => $item->uid,
				'account_id' => $item->name,
				'company_name' => $item->title,
				'company_phone' => $item->field_profile_company_phone_number,
				'agent_phone' => $item->field_profile_phone_number,
				'company_address' => $item->company_address,
				'roles' => _pos_api_expose_get_roles($item->uid)
			);
}

function pos_api_expose_get_customer() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not load customer.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$customerUid = $request->customerUid;

		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			$sql = "SELECT u.uid, n.title, u.name,

					ctp.field_profile_company_phone_number,
					ctp.field_profile_phone_number,

					CONCAT(ctp.field_company_address_street1, ' ',
					ctp.field_company_address_street2, '\n',
					ctp.field_company_address_city, ', ',
					ctp.field_company_address_state, ' ',
					ctp.field_company_address_zip) AS company_address

					FROM node n
					INNER JOIN users u ON n.uid = u.uid
					INNER JOIN content_type_profile ctp ON ctp.vid = n.vid
					WHERE u.uid = '%d' AND n.type = 'profile'";

			$item = db_fetch_object(db_query($sql, $customerUid));
			$user = _pos_api_expose_map_customer_datum($item);

			pos_api_expose_respond(array('status' => true, 'customer' => $user), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_customer_search($token) {
	if(!empty($token)) {
		$uid = _pos_api_expose_uid_from_token($token);

		if($uid) {
			$query = $_GET['searchQuery'];

			//By Search Company Name with Fluid Revelevance
			$fulltext_sql = "SELECT nv.vid, MATCH(nv.title) AGAINST ('%s') AS relevance
							 FROM {node_revisions} nv
							 HAVING relevance > 0";

			$company_name = "SELECT nv.vid, 15 AS relevance
							 FROM node_revisions nv
							 WHERE UPPER(nv.title) LIKE UPPER('%s%%')";

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

			//Search by Address
			$address_search = "SELECT ctp.vid, 5 AS relevance
							   FROM content_type_profile ctp
							   WHERE
							   CONCAT(ctp.field_company_address_street1, ' ',
							   ctp.field_company_address_street2) LIKE '%%%s%%'
							   OR ctp.field_company_address_zip LIKE '%s%%'";

			$allowed_customer_roles = "SELECT uid FROM users_roles WHERE rid IN ('".CUSTOMER_NO_TAX."', '".CUSTOMER_TAX."')";

			$sql = "SELECT u.uid, n.title, u.name,

					ctp.field_profile_company_phone_number,
					ctp.field_profile_phone_number,

					CONCAT(ctp.field_company_address_street1, ' ',
					ctp.field_company_address_street2, '\n',
					ctp.field_company_address_city, ', ',
					ctp.field_company_address_state, ' ',
					ctp.field_company_address_zip) AS company_address

					FROM node n
					INNER JOIN users u ON n.uid = u.uid
					INNER JOIN content_type_profile ctp ON ctp.vid = n.vid
					INNER JOIN ((".$fulltext_sql.") UNION
					(".$accountid_search.") UNION
					(".$phone_searches.") UNION (".$address_search.") UNION (".$company_name."))
					search ON search.vid = n.vid
					WHERE n.type = 'profile' AND u.status = '1' AND u.uid IN (".$allowed_customer_roles.")
					ORDER BY search.relevance DESC
					LIMIT 0, 50";

			$dataset = array();
			$items = db_query($sql, $query, $query, $query, $query, $query, $query, $query);

			while($item = db_fetch_object($items)) {
				$dataset[] = _pos_api_expose_map_customer_datum($item);
			}

			pos_api_expose_respond($dataset, true);
		}
	}

	pos_api_expose_respond(array());
}
