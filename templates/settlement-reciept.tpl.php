<?php
/*Payment invoice reciept*/
$uid = $customer->uid;
if($uid) {
  $profile = content_profile_load('profile', $uid);
}
?>
<!DOCTYPE HTML>
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
  <link rel="stylesheet" type="text/css" href="<?php print base_path() . drupal_get_path('theme', 'ggw_backend'); ?>/css/invoice.css" media="all" />
  <title><?php print $profile->title ?> (<?php print $profile->name ?>)</title>
  <style type="text/css" media="all">
  body {
    -webkit-print-color-adjust:exact;
  }
  table td.numeric-item {
    text-align: right;
  }
  table td.qty-item {
    text-align: center;
  }
  table th.left-item {
    text-align: left;
  }
  .cust-address {
  	width: 100%;
  }
  .cust-address span.label {
  	margin: 0 0 1em 0;
  }
  table tr td {
  	text-align: center;
  }
  .cust-address {
    margin: 0 0 2em;
  }
  </style>
</head>
<body>
  <div class="invoice">
    <div class="header">
      <div class="logo column"></div>
      <div class="address column">
        <p>8000 Harwin dr. Suite #200 Houston, TX 77036</p>
        <p><span class="label">Email: </span>info@general-goods.com</p>
        <p><span class="label">Website: </span>info@general-goods.com</p>
        <p><span class="label">Phone: </span>713-780-3636 <span class="label">Fax: </span>713-780-1718</p>
        <?php if($profile->field_tobacco_permit_id[0]['value']) : ?><p><span class="label">Tobacco ID: </span>93044639</p><?php endif; ?>
      </div>
      <div class="invoice-info column">
        <p class="invoiceid" style="font-size: 1.5em;"><span class="label">Payment Receipt</span></p>
        <p class="invoiceid"><span class="label">Reciept #:</span> <span class="value"><?php echo $payment->sid ?></span></p>
        <p class="invoicedate"><span class="label">Reciept Date:</span> <span class="value"><?php echo date("n/j/Y g:ia", $payment->settlement_date); ?></span></p>
      </div>
    </div>

    <div class="customer-info box">
      <div class="header-line"></div>
      <div class="billing-address cust-address">
        <span class="label">Customer:</span>
        <span class="value">
          <?php if($uid) { ?>
          <?php print $profile->title ?> (<?php print $profile->name ?>)<br />
          <?php print $profile->field_contact_remarks[0]['value'] ?><br />
          <?php print $profile->field_company_address[0]['street1'] ?><br />
          <?php print $profile->field_company_address[0]['city'] ?>, <?php print $profile->field_company_address[0]['state'] ?> <?php print $profile->field_company_address[0]['zip'] ?><br />
          <?php } else { ?>
            Walk-in Customer
          <?php } ?>
        </span>
      </div>
    </div>

    <div class="products">
      <?php
      $header = array('#', 'Payment Date', 'Payment', 'Invoice #', 'Invoiced Amount', 'Settlement', 'Remaining Balance', 'Days');
      $rows = array();
      foreach($breakdown as $line_item) {
      	/*if($line_item->settlement_amount > 0 && $line_item->settlement_amount < .01) {
      		$line_item->settlement_amount = .01;
      	}*/

      	$rows[] = array($line_item->sid, date("n/j/Y", $payment->settlement_date), user_term_credits_recursive_round($payment->paid), $line_item->order_id, uc_currency_format($line_item->credit_amount, false), uc_currency_format($line_item->settlement_amount, false), uc_currency_format($line_item->balance, false), (int)($line_item->days/86400));
      }

      print theme('table', $header, $rows);
      ?>
    </div>

    <div class="line-items">
    <?php
    $items = split("\n", $payment->remarks);
    $payment_forms = array();
    foreach($items as $item) {
    	$components = split("\|", $item);
    	$payment_forms[$components[0]] = $components[1];
    }

    $rows = array();
    if($payment_forms['CASH'] > 0 || $payment_forms['CHANGE'] > 0) {
    	$rows[] = array('CASH', uc_currency_format($payment_forms['CASH'], false));
    	$rows[] = array('CHANGE', uc_currency_format($payment_forms['CHANGE'], false));
    }

    if($payment_forms['CHECK'] > 0) {
    	$rows[] = array('CHECK', uc_currency_format($payment_forms['CHECK'], false));
    }

    if($payment_forms['PCHECK'] > 0) {
      $rows[] = array('POST DATED CHECK', uc_currency_format($payment_forms['PCHECK'], false));
    }

    if($payment_forms['MO'] > 0) {
    	$rows[] = array('MONEY ORDER', uc_currency_format($payment_forms['MO'], false));
    }

    if($payment_forms['CC'] > 0) {
    	$rows[] = array('CREDIT CARD', uc_currency_format($payment_forms['CC'], false));
    }

    print theme('table', array(), $rows);
    ?>
    </div>
    <div class="payment-details"></div>
  </div>
</body>
</html>
