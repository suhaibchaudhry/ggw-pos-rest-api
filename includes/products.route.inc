<?php
function pos_api_expose_product_search($query) {
	/*
	$request = json_decode($_POST['request']);
	$failed_response = array(
		'error' => 'Could not parse request.',
		'results' => false
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;

		$uid = db_result(db_query("SELECT paet.uid FROM {pos_api_expose_tokens} paet WHERE paet.token = '%s'", $token));
		if($uid) {
			

			pos_api_expose_respond($response, true);
		}
	}

	pos_api_expose_respond($failed_response);
	*/

	$sku_sql = "SELECT n.nid, n.title, p.model, p.list_price, p.sell_price FROM {uc_products} p
			INNER JOIN {node} n ON n.vid = p.vid
			INNER JOIN {node_revisions} nv ON nv.vid = n.vid
			WHERE p.model LIKE '%s%%'
			LIMIT 0, 15";

	$search_sql = "SELECT n.nid, n.title, p.model, p.list_price, p.sell_price FROM {uc_products} p
			INNER JOIN {node} n ON n.vid = p.vid
			INNER JOIN {node_revisions} nv ON nv.vid = n.vid
			INNER JOIN (SELECT nv.nid, nv.vid, nv.title FROM {node_revisions} nv
					   WHERE MATCH(title) AGAINST ('%s')) AS search ON n.vid = search.vid
			LIMIT 0, 15";

	$sql = $sku_sql.' UNION '.$search_sql;

	$dataset = array();

	$items = db_query($sql, $query, $query);
	while($item = db_fetch_object($items)) {
		$dataset[] = array(
			'nid' => $item->nid,
			'sku' => $item->model,
			'list_price' => $item->list_price,
			'sell_price' => $item->sell_price,
			'name' => $item->title
		);
	}

	print json_encode($dataset);
}