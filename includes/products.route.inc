<?php
function _pos_api_expose_load_special_prices($vid) {
	$prices = array();
	$role_prices = db_query("SELECT rid, price FROM {uc_price_per_role_prices} WHERE vid = '%d'", $vid);
	while($role_price = db_fetch_object($role_prices)) {
		$prices[] = array('role' => $role_price->rid, 'price' => $role_price->price);
	}

	return $prices;
}

function _pos_api_expose_get_category($nid) {
	return db_result(db_query("SELECT gsrt.invoice_category FROM {term_node} tn INNER JOIN {ggw_state_reporting_terms} gsrt ON tn.tid = gsrt.vid WHERE tn.nid = '%d'", $nid));
}

function _pos_api_expose_map_product_datum($item, $special_price = true) {
	if(empty($item->filepath)) {
		$item->filepath = 'sites/general-goods.com/files/imagefield_default_images/notfound_0.png';
	}

	$product = array(
		'id' => $item->nid,
		'sku' => $item->model,
		'list_price' => $item->list_price,
		'packaging' => $item->field_prod_packing_value,
		'sell_price' => $item->sell_price,
		'name' => trim($item->title),
		'image' => imagecache_create_url('pos_client_search', $item->filepath),
		'thumbnail' => imagecache_create_url('uc_thumbnail', $item->filepath)
	);

	if($special_price) {
		$product['special_prices'] = _pos_api_expose_load_special_prices($item->vid);
	}

	$category = _pos_api_expose_get_category($item->nid);
	if($category) {
		$product['category'] = $category;
	}

	if(!empty($item->qty)) {
		$product['qty'] = (int)$item->qty;
	}

	return $product;
}

function _pos_api_expose_map_product_rma($item) {
	if(empty($item->filepath)) {
		$item->filepath = 'sites/general-goods.com/files/imagefield_default_images/notfound_0.png';
	}

	$product = array(
		'date' => format_date($item->created, 'custom', variable_get('uc_date_format_default', 'm/d/Y')),
		'ticketId' => $item->order_id,
		'id' => $item->order_product_id,
		'sku' => $item->model,
		'packaging' => $item->field_prod_packing_value,
		'sell_price' => $item->price,
		'name' => trim($item->title),
		'image' => imagecache_create_url('pos_client_search', $item->filepath),
		'thumbnail' => imagecache_create_url('uc_thumbnail', $item->filepath),
		'qty' => (int)$item->qty
	);
	/*
	$category = _pos_api_expose_get_category($item->nid);
	if($category) {
		$product['category'] = $category;
	}*/

	return $product;
}

function pos_api_rma_process() {
	//Todo: scan for additional barcodes and unit item barcodes.

	$failed_response = array(
		'status' => false,
		'error' => 'Could not process RMA request.'
	);
	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$uid = _pos_api_expose_uid_from_token($request->token);
		$item_barcode = $request->item_barcode;
		$customer_uid = $request->customer_uid;

		if($uid) {
			$items = db_query("SELECT uop.order_product_id, uo.created, uop.order_id, uop.nid, uop.title, uop.model, uop.price, uop.qty, ctp.field_prod_packing_value, f.filepath
				FROM {uc_order_products} uop
				INNER JOIN {uc_orders} uo ON uop.order_id = uo.order_id
				LEFT JOIN {content_type_product} ctp ON uop.nid = ctp.nid
				LEFT JOIN {content_field_image_cache} cfic ON uop.nid = cfic.nid
				LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
				WHERE uo.uid = '%d' AND uop.model = '%s' AND uo.order_status = 'pos_completed' AND uop.order_product_id NOT IN (SELECT order_product_id FROM pos_api_expose_rma_returned_products)", $customer_uid, $item_barcode);

			if($items) {
				$products = array();

				while($item = db_fetch_object($items)) {
					$products[] = _pos_api_expose_map_product_rma($item);
				}

				if(count($products) > 0) {
					$response = array(
						'status' => true,
						'products' => $products
					);

					pos_api_expose_respond($response, true);
				} else {
					$failed_response = array(
						'status' => false,
						'error' => 'Item was not found in customer history.'
					);
				}
			} else {
				$failed_response = array(
					'status' => false,
					'error' => 'Item was not found in customer history.'
				);
			}
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_product_search($token, $query) {
	if(!empty($token)) {
		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			$additional_barcode_sql = "SELECT cfab.vid FROM {content_field_additional_barcodes} cfab WHERE cfab.field_additional_barcodes_value LIKE '%s%%'";

			$sku_sql = "SELECT p.vid, 1 AS relevance
						FROM {uc_products} p
						LEFT JOIN {content_type_product} ctp ON p.vid = ctp.vid
						WHERE p.model LIKE '%s%%' OR
							  ctp.field_prod_unit_barcode_value LIKE '%s%%' OR
							  p.vid IN (".$additional_barcode_sql.")";

			$fulltext_sql = "SELECT nv.vid, MATCH(nv.title) AGAINST ('%s') AS relevance
							 FROM {node_revisions} nv
							 HAVING relevance > 0";

			//UNION ALL appears to be significantly faster than UNION.
			$sql = 'SELECT n.nid, n.vid, n.title, p.model, p.list_price, p.sell_price, f.filepath, ctp_wrap.field_prod_packing_value
					FROM {uc_products} p
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
					LEFT JOIN {content_type_product} ctp_wrap ON n.vid = ctp_wrap.vid

					INNER JOIN (('.$sku_sql.') UNION ALL ('.$fulltext_sql.')) fts ON n.vid = fts.vid

					ORDER BY fts.relevance DESC
					LIMIT 0, 12';

			$dataset = array();

			$items = db_query($sql, $query, $query, $query, $query);
			while($item = db_fetch_object($items)) {
				$dataset[] = _pos_api_expose_map_product_datum($item);
			}

			pos_api_expose_respond($dataset, true);
		}
	}

	pos_api_expose_respond(array());
}

function pos_api_expose_load_ticket_products() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not load ticket products.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$ticketId = $request->ticketId;

		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			_pos_api_expose_log_ticket_load($uid, $ticketId);

			$ticketSql = "SELECT nid, qty FROM {uc_order_products} WHERE order_id = '%d'";

			$sql = 'SELECT n.nid, n.vid, n.title, p.model, p.list_price, p.sell_price, f.filepath, ctp_wrap.field_prod_packing_value, ts.qty
					FROM {uc_products} p
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
					LEFT JOIN {content_type_product} ctp_wrap ON n.vid = ctp_wrap.vid

					INNER JOIN ('.$ticketSql.') ts ON ts.nid = n.nid';

		    $items = db_query($sql, $ticketId);
			$dataset = array();
			while($item = db_fetch_object($items)) {
				$dataset[] = _pos_api_expose_map_product_datum($item);
			}	

			pos_api_expose_respond(array('status' => true, 'products' => $dataset), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_rma_products() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not load return item products.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$ticketId = $request->ticketId;

		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			$sql = "SELECT n.nid, n.vid, n.title, p.model, p.list_price, p.sell_price, f.filepath, ctp_wrap.field_prod_packing_value, rp.qty_returned AS qty
					FROM {pos_api_expose_rma_returned_products} rp
					INNER JOIN {uc_order_products} uop ON rp.order_product_id = uop.order_product_id
					INNER JOIN {uc_products} p ON p.nid = uop.nid
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
					LEFT JOIN {content_type_product} ctp_wrap ON n.vid = ctp_wrap.vid

					WHERE uop.order_id = '%d'";

			$products = db_query($sql, $ticketId);
			$data = array();
			while($product = db_fetch_object($products)) {
				$data[] = _pos_api_expose_map_product_datum($product, false);
			}

			$total = db_result(db_query("SELECT SUM(refunds.total_refund) FROM (SELECT DISTINCT rl.pid, uop.order_id, rl.total_refund FROM pos_api_expose_rma_refund_log rl
					INNER JOIN pos_api_expose_rma_returned_products rp ON rl.pid = rp.refund_log_pid
					INNER JOIN uc_order_products uop ON uop.order_product_id = rp.order_product_id) refunds"));
			
			pos_api_expose_respond(array('status' => true, 'products' => $data, 'total' => $total), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_product_scan() {
	$failed_response = array(
		'scan' => false,
		'error' => 'Could not scan barcode.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$barcode = trim($request->barcode);

		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			$additional_barcode_sql = "SELECT cfab.vid FROM {content_field_additional_barcodes} cfab WHERE cfab.field_additional_barcodes_value = '%s'";

			$sku_sql = "SELECT p.vid
						FROM {uc_products} p
						LEFT JOIN {content_type_product} ctp ON p.vid = ctp.vid
						WHERE p.model = '%s' OR
							  ctp.field_prod_unit_barcode_value = '%s' OR
							  p.vid IN (".$additional_barcode_sql.")";

			$sql = 'SELECT n.nid, n.vid, n.title, p.model, p.list_price, p.sell_price, f.filepath, ctp_wrap.field_prod_packing_value
					FROM {uc_products} p
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
					LEFT JOIN {content_type_product} ctp_wrap ON n.vid = ctp_wrap.vid

					WHERE p.vid IN ('.$sku_sql.')
					LIMIT 0, 1';

			$product = db_fetch_object(db_query($sql, $barcode, $barcode, $barcode));

			$datum = _pos_api_expose_map_product_datum($product);

			if($product) {
				$response = array(
					'scan' => true,
					'product' => $datum
				);
			} else {
				$response = array(
					'scan' => false,
					'error' => 'Could not find barcode.'
				);
			}

			pos_api_expose_respond($response, true);
		}
	}

	pos_api_expose_respond($failed_response);
}