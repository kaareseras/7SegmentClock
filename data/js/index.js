var url = "ws://" + window.location.hostname + ":1337/";
var output;
var button;
var canvas;
var context;

// This is called when the page finishes loading
function init() {

    // Assign page elements to variables
    button = document.getElementById("toggleButton");
    output = document.getElementById("output");
    canvas = document.getElementById("led");
    
    // Draw circle in canvas
    context = canvas.getContext("2d");
    context.arc(25, 25, 15, 0, Math.PI * 2, false);
    context.lineWidth = 3;
    context.strokeStyle = "black";
    context.stroke();
    context.fillStyle = "black";
    context.fill();
    
    // Connect to WebSocket server
    wsConnect(url);
}

// Call this to connect to the WebSocket server
function wsConnect(url) {
    
    // Connect to WebSocket server
    websocket = new WebSocket(url);
    
    // Assign callbacks
    websocket.onopen = function(evt) { onOpen(evt) };
    websocket.onclose = function(evt) { onClose(evt) };
    websocket.onmessage = function(evt) { onMessage(evt) };
    websocket.onerror = function(evt) { onError(evt) };
}

// Called when a WebSocket connection is established with the server
function onOpen(evt) {

    // Log connection state
    console.log("Connected");
    
    // Enable button
    button.disabled = false;
    
    // Get the current state of the LED
    doSend('{"command": "getLEDState"}');
	doSend('{"command": "getBrightness"}');
	doSend('{"command": "getColor"}');
}

// Called when the WebSocket connection is closed
function onClose(evt) {

    // Log disconnection state
    console.log("Disconnected");
    
    // Disable button
    button.disabled = true;
    
    // Try to reconnect after a few seconds
    setTimeout(function() { wsConnect(url) }, 2000);
}

// Called when a message is received from the server
function onMessage(evt) {

    // Print out our received message
    console.log("Received: " + evt.data);
	
	var response = JSON.parse(evt.data); 
    
    switch(response.tag) {
		case "led_state":
			console.log("led_state");
			if (response.value == 0){
				console.log("LED is off");
				context.fillStyle = "black";
				context.fill();
			}
			else {
				console.log("LED is on");
				context.fillStyle = "red";
				context.fill();
			}
			break;
		case "color":
		    console.log("color");
			var r = response.r;
			var g = response.g;
			var b = response.b;
			var colorHex = rgbToHex(r,g,b);
			console.log("Color is now: " + colorHex);
			$('#colorHex').val(colorHex);
			break;
		case "brightness":
			console.log("Brightness is now: " + response.value);
			$('#rangeBrightness').val(response.value);
			break;
		default:
			break;
    }
}

// Called when a WebSocket error occurs
function onError(evt) {
    console.log("ERROR: " + evt.data);
}

// Sends a message to the server (and prints it to the console)
function doSend(message) {
    console.log("Sending: " + message);
    websocket.send(message);
}

// Called whenever the HTML button is pressed
function onPress() {
    doSend('{"command": "toggleLED"}');
    doSend('{"command": "getLEDState"}');
}

// When brightness is changed
function onPressBrightness() {
	var brightness = $('#rangeBrightness').val();
	console.log(brightness);
	doSend('{"command": "setBrightness", "value": '+ brightness +'}');
	doSend('{"command": "getBrightness"}');
}

function onPressColor() {
	var colorHex = $('#colorHex').val();
	console.log(colorHex);
	var obj = { command: "setColor", r: hexToRgb(colorHex).r, g: hexToRgb(colorHex).g, b: hexToRgb(colorHex).b };
	var json = JSON.stringify(obj)
	console.log(json);
	doSend(json);
	doSend('{"command": "getColor"}');
}

function hexToRgb(hex) {
  var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
  return result ? {
    r: parseInt(result[1], 16),
    g: parseInt(result[2], 16),
    b: parseInt(result[3], 16)
  } : null;
}//alert(hexToRgb("#0033ff").g); // "51";

function rgbToHex(r, g, b) {
  return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
}

//alert(rgbToHex(0, 51, 255)); // #0033ff

// Call the init function as soon as the page loads
window.addEventListener("load", init, false);