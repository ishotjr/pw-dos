// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
	function(e) {
		console.log('PebbleKit JS ready!');

		getEmail();
	}
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
	function(e) {
		console.log('AppMessage received!');

		getEmail();
	}                     
);


function getEmail() {
	// Assemble dictionary using our keys
	var dictionary = {
		'KEY_EMAIL_COUNT': 999
	};

	// Send to Pebble
	Pebble.sendAppMessage(dictionary,
		function(e) {
			console.log('Data sent to Pebble successfully!');
		},
		function(e) {
			console.log('Error sending data to Pebble!');
		}
	);

}
