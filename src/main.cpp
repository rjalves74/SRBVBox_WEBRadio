#include "Arduino.h"
#include <SPI.h>
#include <WiFi.h>
#include "vs1053_ext.h"
#include "Preferences.h"

// Display OLED
#include <Adafruit_SSD1306.h>

//GPIO OLED
//VIN	3.3V
//GND	GND
//SCL	GPIO 22
//SDA	GPIO 21

//Web Page
#include <DNSServer.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <LittleFS.h>

// Captive Portal Name 
#ifndef APSSID
#define APSSID "SRBV Box"
#endif

bool DEBUG = true;  // if "true" all info will be shown in serial monitor; id "false" no info is shown on serial monitor


const char *softAP_ssid = APSSID;

// Don't set this wifi credentials. They are configurated at runtime and stored on NVRAM
String ssidWifi;
String passwordWifi;


// Digital I/O (VS1053)
#define VS1053_CS     32
#define VS1053_DCS    33
#define VS1053_DREQ   35
#define VS1053_MOSI   23
#define VS1053_MISO   19
#define VS1053_SCK    18
#define VS1053_RST    EN

Preferences preferences; // instance to acces NVRAM

// Digital I/O for  VOLUME and STATION button
#define VOL_DOWN 12
#define VOL_UP 13
#define STATION_DOWN 15
#define STATION_UP 4

static uint8_t volume = 0; // 0...21
const uint8_t volume_step = 1;
uint8_t cur_station  = 0;   //current station(nr), will be set later
uint8_t cur_volume   = 15;   //will be set from stored preferences
unsigned long debounce = 0;

#define RESET_BUTTON 2


// create a player object connected to VS1053 decoder
VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ, VSPI, VS1053_MOSI, VS1053_MISO, VS1053_SCK);

//Define screen OLED
#define SCREEN_WIDTH 128 // OLED width,  in pixels
#define SCREEN_HEIGHT 64 // OLED height, in pixels

// create an OLED display object connected to I2C
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Display control
bool display_on = false;

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server
AsyncWebServer server(80);

// Soft AP network parameters - setting ip 8.8.8.8 for AP because of Android devices
IPAddress apIP(8, 8, 8, 8);
IPAddress netMsk(255, 255, 255, 0);

// Main page of AP
const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<title>Super RBV</title>
<style>
body {
	color: #434343; 
	font-family: "Helvetica Neue",Helvetica,Arial,sans-serif; 
	font-size: 14px; 
	background-color: #eeeeee; 
	margin-top: 100px;}

	img {
	height: auto;
	display: block;
	margin-left: auto;
	margin-right: auto;
	}
	.container {
		margin: 0 auto; 
		max-width: 400px; 
		padding: 35px; 
		box-shadow: 0 10px 20px rgba(0,0,0,0.19), 0 6px 6px rgba(0,0,0,0.23); 
		background-color: #ffffff; 
		border-radius: 10px;
	}
	h2 {
		text-align: center; 
		margin-bottom: 20px; 
		margin-top: 0px; 
		color: #0ee6b1; 
		font-size: 35px;
	}

	#titleBlue {color: #0000FF;}
	#titleBlack {color: #000000;}

	h3 {
		text-align: center; 
		margin-bottom: 40px; 
		margin-top: 0px; color: #336859; 
		font-size: 35px;
	}

	form .field-group {
		box-sizing: border-box; 
		clear: both; 
		padding: 4px 0; 
		position: relative; 
		margin: 1px 0; 
		width: 100%;
	}
	
	.text-field {
		font-size: 15px; 
		margin-bottom: 4%; 
		webkit-appearance: none; 
		display: block; 
		background: #fafafa; 
		color: #636363; 
		width: 100%; 
		padding: 15px 0px 15px 0px; 
		text-indent: 10px; 
		border-radius: 5px; 
		border: 1px solid #e6e6e6; 
		background-color: transparent;
	}

	.text-field:focus {
		border-color: #00bcd4; 
		outline: 0;
	}
	
	.button-container {
		box-sizing: border-box; 
		clear: both; 
		margin: 1px 0 0; 
		padding: 4px 0; 
		position: relative; 
		width: 100%;
	}

	.button {
		background: #00E1AA; 
		border: none; 
		border-radius: 5px; 
		color: #ffffff; 
		cursor: pointer; 
		display: block; 
		font-weight: bold; 
		font-size: 16px; 
		padding: 15px 0; 
		text-align: center; 
		text-transform: uppercase; 
		width: 100%; 
		-webkit-transition: background 250ms ease; 
		-moz-transition: background 250ms ease; 
		-o-transition: background 250ms ease; 
		transition: background 250ms ease;
	}

	p {
		text-align: center; 
		text-decoration: none; 
		color: #87c1d3; 
		font-size: 18px;
	}

	a {
		text-decoration: none; 
		color: #ffffff; 
		margin-top: 0%;
	}

	#status {
		text-align: center; 
		text-decoration: none; 
		color: #336859; 
		font-size: 14px;
	}
</style>
<script>

function validateForm() {
	var ssid = document.forms["myForm"]["ssid"].value;
	var password = document.forms["myForm"]["password"].value;
	var status = document.getElementById("statusDiv");
	if (ssid == "" && password == "") {
		status.innerHTML = "<p id='status' style='color:red;'>Insira SSID e senha.</p>";
		return false;
	}
	else if (ssid == "") {
		status.innerHTML = "<p id='status' style='color:red;'>Insira SSID.</p>";
		return false;
	}
	else if (password == "") {
		status.innerHTML = "<p id='status' style='color:red;'>Insira senha.</p>";
		return false;
	}
	else {
		status.innerHTML = "<p id='status'>Conectando...</p>";
		return true;
	}
	}

</script>
</head>
	<body>
		<div class="container">
			<img src="SRBVLogo">

			<h3>Conecte-se ao seu WiFi</h3>
			
			<form name="myForm" action="/action_new_connection" onsubmit="return validateForm()" method="post">
				<div class="field-group">
					<input class='text-field' name='ssid' placeholder="Rede">
				</div>
			<br>
			<div class="field-group">
				<input class="text-field" type="password" name="password" length=64 placeholder="Senha">
			</div>
			<br>
			<div id="statusDiv">
				<br><br>
			</div>
				<div class="button-container">
				<input class="button" type="submit" value="Conectar">
			</div>
			</form>
		</div>
	</body>
</html>
)=====";

// Setting available stations
String stations[] ={
        "9000.brasilstream.com.br/stream",
        "8802.brasilstream.com.br/stream",
        "8431.brasilstream.com.br/stream",
        "8915.brasilstream.com.br/stream",
        "8268.brasilstream.com.br/stream",
        "8469.brasilstream.com.br/stream"
};

String stationsname[] ={
        "SPO",
        "RIO",
        "SDR",
        "BSA",
        "RPO",
        "POA"
};

// Logo SRBV image for OLED display
const unsigned char logoSRBV [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 
	0x00, 0xff, 0xfd, 0xf8, 0x07, 0xdf, 0xff, 0xe0, 0x0f, 0xff, 0xef, 0xff, 0xe2, 0x00, 0x00, 0x00, 
	0x03, 0xff, 0xfd, 0xf8, 0x07, 0xdf, 0xff, 0xf8, 0x3f, 0xff, 0xef, 0xff, 0xf8, 0x00, 0x00, 0x00, 
	0x07, 0xff, 0xfd, 0xf0, 0x07, 0xdf, 0xff, 0xf8, 0x7f, 0xff, 0xef, 0xff, 0xfc, 0x00, 0x00, 0x00, 
	0x07, 0xff, 0xfd, 0xf0, 0x0f, 0xdf, 0xff, 0xfc, 0x7f, 0xff, 0xef, 0xff, 0xfc, 0x00, 0x00, 0x00, 
	0x0f, 0xff, 0xfd, 0xf0, 0x0f, 0xdf, 0xff, 0xfc, 0xff, 0xff, 0xcf, 0xff, 0xfe, 0x00, 0x00, 0x00, 
	0x0f, 0xff, 0xfd, 0xf0, 0x0f, 0xdf, 0xff, 0xfc, 0xff, 0x9f, 0xcf, 0xff, 0xfe, 0x00, 0x00, 0x00, 
	0x0f, 0x80, 0x01, 0xf0, 0x0f, 0xdf, 0x80, 0x7c, 0xf8, 0x00, 0x0f, 0xc0, 0x3e, 0x00, 0x00, 0x00, 
	0x0f, 0x80, 0x01, 0xf0, 0x0f, 0xdf, 0x80, 0x7d, 0xf8, 0x00, 0x1f, 0x80, 0x3e, 0x00, 0x00, 0x00, 
	0x0f, 0xff, 0xe1, 0xf0, 0x0f, 0x9f, 0x80, 0x7d, 0xff, 0xff, 0xdf, 0x80, 0x7e, 0x00, 0x00, 0x00, 
	0x0f, 0xff, 0xf1, 0xf0, 0x0f, 0x9f, 0xff, 0xfd, 0xff, 0xff, 0xdf, 0xff, 0xfe, 0x00, 0x00, 0x00, 
	0x0f, 0xff, 0xf9, 0xf0, 0x0f, 0x9f, 0xff, 0xfd, 0xff, 0xff, 0xdf, 0xff, 0xfe, 0x00, 0x00, 0x00, 
	0x07, 0xff, 0xfb, 0xf0, 0x0f, 0x9f, 0xff, 0xf9, 0xff, 0xff, 0xdf, 0xff, 0xfc, 0x00, 0x00, 0x00, 
	0x07, 0xff, 0xff, 0xf0, 0x0f, 0xbf, 0xff, 0xf9, 0xff, 0xff, 0xdf, 0xff, 0xfc, 0x00, 0x00, 0x00, 
	0x01, 0xff, 0xff, 0xf0, 0x0f, 0xbf, 0xff, 0xf1, 0xf8, 0x00, 0x1f, 0xff, 0xf8, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x7f, 0xf0, 0x1f, 0xbf, 0xff, 0xc1, 0xf8, 0x00, 0x1f, 0xff, 0xf0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x7f, 0xff, 0xff, 0xbf, 0x00, 0x01, 0xf8, 0x00, 0x1f, 0x83, 0xf0, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xfb, 0xff, 0xff, 0xbf, 0x00, 0x01, 0xff, 0xff, 0x9f, 0x83, 0xf0, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xfb, 0xff, 0xff, 0xbf, 0x00, 0x01, 0xff, 0xff, 0x9f, 0x03, 0xf8, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xf9, 0xff, 0xff, 0xbf, 0x00, 0x01, 0xff, 0xff, 0x9f, 0x01, 0xf8, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xf1, 0xff, 0xff, 0xbe, 0x00, 0x00, 0xff, 0xff, 0xbf, 0x01, 0xfc, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xe0, 0xff, 0xff, 0x3e, 0x00, 0x00, 0x7f, 0xff, 0xbf, 0x00, 0xfc, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xc0, 0x7f, 0xff, 0x3e, 0x00, 0x00, 0x3f, 0xff, 0xbf, 0x00, 0xfc, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x1f, 0xfc, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xff, 0xfc, 0x1f, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x1f, 0xfc, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xff, 0xfe, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x3f, 0xf8, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x3f, 0xf8, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xff, 0xff, 0xdf, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x7f, 0xf0, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xff, 0xff, 0xdf, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x7f, 0xf0, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0xff, 0xe0, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0xff, 0xe0, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x01, 0xff, 0xc0, 0x00, 0x00, 0x00, 
	0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x01, 0xff, 0xc0, 0x00, 0x00, 0x00, 
	0x1f, 0xf8, 0x00, 0x3f, 0xff, 0xf8, 0x00, 0x3f, 0xff, 0xf8, 0x03, 0xff, 0x80, 0x00, 0x00, 0x00, 
	0x1f, 0xf8, 0x00, 0x1f, 0xff, 0xf8, 0x00, 0x1f, 0xff, 0xf8, 0x03, 0xff, 0x80, 0x00, 0x00, 0x00, 
	0x1f, 0xf8, 0x00, 0x1f, 0xff, 0xf8, 0x00, 0x1f, 0xff, 0xf8, 0x07, 0xff, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xf8, 0x00, 0x1f, 0xff, 0xf8, 0x00, 0x1f, 0xff, 0xfc, 0x07, 0xff, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xf0, 0x00, 0x1f, 0xff, 0xf8, 0x00, 0x7f, 0xff, 0xfc, 0x0f, 0xfe, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xf0, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x0f, 0xfe, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xff, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x1f, 0xfc, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xfe, 0x1f, 0xfc, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x8f, 0xfe, 0x3f, 0xf8, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xff, 0x3f, 0xf8, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0xff, 0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0xff, 0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xff, 0xff, 0xff, 0xbf, 0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xff, 0xff, 0xff, 0x3f, 0xf0, 0x00, 0xff, 0xf3, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 
	0x7f, 0xff, 0xff, 0xfe, 0x3f, 0xf0, 0x00, 0x7f, 0xf3, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 
	0x7f, 0xf1, 0x1f, 0xfe, 0x7f, 0xf0, 0x00, 0x3f, 0xf1, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
	0x7f, 0xe0, 0x0f, 0xfe, 0x7f, 0xe0, 0x00, 0x7f, 0xf1, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
	0x7f, 0xe0, 0x0f, 0xff, 0x7f, 0xe0, 0x00, 0x7f, 0xf1, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x7f, 0xe0, 0x07, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x7f, 0xe0, 0x07, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x7f, 0xe0, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x7f, 0xe0, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x7f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x7f, 0xe0, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x7f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x7f, 0xe0, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x7f, 0xc0, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x3f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xff, 0xc0, 0x00, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xff, 0xc0, 0x00, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x0f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 1040)
const int epd_bitmap_allArray_LEN = 1;
const unsigned char* epd_bitmap_allArray[1] = {
	logoSRBV
};


// Is this an IP? 
boolean isIp(String str)
{
    for (size_t i = 0; i < str.length(); i++)
    {
        int c = str.charAt(i);
        if (c != '.' && (c < '0' || c > '9'))
        {
            return false;
        }
    }
    return true;
}

// IP to String? 
String toStringIp(IPAddress ip)
{
    String res = "";
    for (int i = 0; i < 3; i++)
    {
        res += String((ip >> (8 * i)) & 0xFF) + ".";
    }
    res += String(((ip >> 8 * 3)) & 0xFF);
    return res;
}


// Store WLAN credentials on NVRAM
void saveCredentials(String ssidWifi, String passwordWifi)
{
    preferences.begin("preferences", false);
    preferences.putString("ssidWifi", ssidWifi); 
    preferences.putString("passwordWifi", passwordWifi);

	if (DEBUG == true) {
		Serial.printf("SSID (save): %s\n", ssidWifi);
		Serial.printf("Password (save): %s\n", passwordWifi);
	    Serial.println("Network Credentials Saved using Preferences");
	}	

}


// Captive Portal
class CaptiveRequestHandler : public AsyncWebHandler
{
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}
    bool canHandle(AsyncWebServerRequest *request)
    {
        request->addInterestingHeader("ANY");
        return true;
    }
    void handleRequest(AsyncWebServerRequest *request)
    {
        request->send_P(200, "text/html", MAIN_page);
    }
};

// Start WebServer
void setupWebServer()
{

	// Startup Web Page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
		{ request->send_P(200, "text/html", MAIN_page); 
		Serial.println("Client Connected"); 
	} );

	// get logo from LittleFS
	server.on("/SRBVLogo", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send(LittleFS, "/srbv_logo.webp", "image/webp");
	} );

	// When click on connect button
	server.on("/action_new_connection", HTTP_POST, [](AsyncWebServerRequest *request) {
		int params = request->params();
		for(int i=0;i<params;i++){
			AsyncWebParameter* p = request->getParam(i);
			if(p->isPost()){
				if (p->name() == "ssid") {
					ssidWifi = p->value().c_str();
				}

				if (p->name() == "password") {
					passwordWifi = p->value().c_str();
				}
			}
		}

		int count = 0;

		if (DEBUG == true) Serial.println(F("Conectando-se ao WiFi"));

		WiFi.begin(ssidWifi.c_str(), passwordWifi.c_str());     // Conect to WI-Fi
		
		delay(2000); // Wait 2 seconds

		// If successfully connected
		if (WiFi.status() == WL_CONNECTED) {
			Serial.println("");
			Serial.println("");
		
			// If success on connect to WI-Fi
			if (DEBUG == true) {
				Serial.println("Conectado ao WiFi");
				Serial.print("IP address: ");
				Serial.println(WiFi.localIP());  //IP Address
			}

			String responsePage = (const __FlashStringHelper*) MAIN_page; // Read HTML page again

			// Change text status to Connected
			responsePage.replace("<br><br>", "<p id='status'>Conectado! IP: " + toStringIp(WiFi.localIP()) + "</p>");
			request->send(200, "text/html", responsePage);

			// Save Credentials on NVRAM
			saveCredentials(ssidWifi, passwordWifi);
			
			delay(2000); // Wait 2 seconds
			
			
			ESP.restart(); // Restart device

			return;
		}
		else  {
			String responsePage = (const __FlashStringHelper*) MAIN_page; // Read HTML page again
			
			// Change text status to Failed
			responsePage.replace("<br><br>", "<p id='status' style='color:red;'>Falha na conexão.</p>");
			request->send(200, "text/html", responsePage ); 
			return;
		}
		
	} ); 	

}

// Display volume
void display_volume(const int new_volume){ 
//	oled.setTextColor(WHITE);    // set text color
	oled.setTextSize(2);         // set text size
	oled.clearDisplay(); 		// clear display
	oled.setCursor(0, 0);       // set position to display
	oled.println("Volume"); 
	oled.setTextSize(5);       	// set text size
	oled.setCursor(40, 25);    	// set position to display
	oled.println(new_volume); 	// volume atual
	oled.display();             // display on OLED
	display_on=true;

}

// Display station
void display_station(const int new_station){ 
	oled.setTextSize(2);         // set text size
	oled.clearDisplay(); 		// clear display
	oled.setCursor(0, 0);       // set position to display
	oled.println("SRBV"); 
	oled.setTextSize(4);        // set text size
	oled.setCursor(10, 25);     // set position to display
	oled.println(stationsname[new_station]); 
	oled.display();            // display on OLED
	display_on=true;

}

// Change station
void change_radiostation(const int new_radioStation){ 
	player.connecttohost(stations[new_radioStation].c_str());
	preferences.putUInt("radioStation",new_radioStation); // save station in nvram
}

// Change volume
void change_volume(const int new_volume){ 
   player.setVolume(new_volume);
   preferences.putUInt("volume",new_volume); // save volume in nvram
}

// Startup WebServer
void launchWebServer()
{
    Serial.println("Configuring access point...");

    // your other setup stuff...
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(softAP_ssid);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("Setting up Async WebServer");
    setupWebServer();
    Serial.println("Starting DNS Server");

    /* Setup the DNS server redirecting all the domains to the apIP */
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", apIP);

    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    // only when requested from AP
    // more handlers...
    server.begin();
    Serial.println("HTTP server started");

}

// Test Wi-Fi Connection
bool testWifi(void)
{
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while ( c < 10 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  return false;
}

bool listDir() {
  File root = LittleFS.open("/"); // Abre o "diretório" onde estão os arquivos na SPIFFS
  //                                e passa o retorno para
  //                                uma variável do tipo File.
  if (!root) // Se houver falha ao abrir o "diretório", ...
  {
    // informa ao usuário que houve falhas e sai da função retornando false.
    Serial.println(" - falha ao abrir o diretório");
    return false;
  }
  File file = root.openNextFile(); // Relata o próximo arquivo do "diretório" e
  //                                    passa o retorno para a variável
  //                                    do tipo File.
  int qtdFiles = 0; // variável que armazena a quantidade de arquivos que
  //                    há no diretório informado.
  while (file) { // Enquanto houver arquivos no "diretório" que não foram vistos,
    //                executa o laço de repetição.
    Serial.print("  FILE : ");
    Serial.print(file.name()); // Imprime o nome do arquivo
    Serial.print("\tSIZE : ");
    Serial.println(file.size()); // Imprime o tamanho do arquivo
    qtdFiles++; // Incrementa a variável de quantidade de arquivos
    file = root.openNextFile(); // Relata o próximo arquivo do diretório e
    //                              passa o retorno para a variável
    //                              do tipo File.
  }
  if (qtdFiles == 0)  // Se após a visualização de todos os arquivos do diretório
    //                      não houver algum arquivo, ...
  {
    // Avisa o usuário que não houve nenhum arquivo para ler e retorna false.
    Serial.print(" - Sem arquivos para ler. Crie novos arquivos pelo menu ");
    Serial.println("principal, opção 2.");
    return false;
  }
  return true; // retorna true se não houver nenhum erro
}

void setup() {

    Serial.begin(115200);
    SPI.begin();

	// pin I/O station and volume
    pinMode(VOL_DOWN, INPUT_PULLUP);
    pinMode(VOL_UP, INPUT_PULLUP);
    pinMode(STATION_DOWN, INPUT_PULLUP);
    pinMode(STATION_UP, INPUT_PULLUP);

	// pin I/O reset button
    pinMode(RESET_BUTTON, INPUT_PULLUP);

    preferences.begin("preferences", false);

	// Clear on preferences on NVRAM
	//preferences.clear();
    
	// Get credentials 
	ssidWifi = preferences.getString("ssidWifi", ""); 
    passwordWifi = preferences.getString("passwordWifi", "");

	delay(1000);

	// Startup LittleFS
	if(!LittleFS.begin(true)){
		Serial.println("An Error has occurred while mounting SPIFFS");
		return;
	}
	else
	{ 
		listDir();
	}

    if (ssidWifi == "" || passwordWifi == ""){
        Serial.println("No values saved for ssid or password");
        launchWebServer();
    }
    else 
    { 
        Serial.println("Connecting to wifi...");
        WiFi.begin(ssidWifi.c_str(), passwordWifi.c_str());
    }

    if (testWifi())
    {
        Serial.println("Succesfully Connected!!!");

		//preferences.begin("preferences", false);
		cur_station = preferences.getUInt("radioStation",0);
		cur_volume = preferences.getUInt("volume",3);

		//Ignore audio/station preferences
		//cur_station=0;
		cur_volume=20;

		player.begin();
		player.setVolume(cur_volume);

			// initialize OLED display with I2C address 0x3C
		if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
			if (DEBUG == true) Serial.println(F("failed to start OLED"));
			while (1);
		}

		//Display initial greetings
		oled.clearDisplay(); // clear display
		oled.setTextSize(1);         // set text size
		oled.setTextColor(WHITE);    // set text color
		oled.setCursor(10, 55);       // set position to display
		oled.drawBitmap(10, 0, logoSRBV, 128, 64, WHITE);
		oled.display();              // display on OLED
		delay(3000);
		oled.clearDisplay(); // clear display
		oled.display();              // display on OLED

		//player.connecttospeech("São Paulo", "pt");
		player.connecttohost(stations[cur_station].c_str());

    }
    else
    {
        Serial.println("Turning the HotSpot On");
        launchWebServer();
    }






}


// pressed button
bool pressed( const int pin )
{
	if (millis() > (debounce + 500))
	{
	    if (digitalRead(pin) == LOW)
	    {
	      debounce = millis();
	      return true;
	    }
	}
	return false;
}

void loop()
{
	if (WiFi.status() != WL_CONNECTED) 
    {
        dnsServer.processNextRequest();
    }    
	    else 
    { 
	    player.loop();
	}

    bool updateVolume = false;
    bool updateStation = false;

	if (pressed(VOL_UP))
	{
		if (cur_volume <= (21-volume_step))
		{
			// Increase volume
            cur_volume++;
			updateVolume = true;
		} 
	}

	if (pressed(VOL_DOWN))
	{
		if (cur_volume > volume_step)
		{
			// Decrease volume
            cur_volume--;
			updateVolume = true;
		} 
	}

	if (updateVolume)
	{
		// Volume change requested
		if (DEBUG == true) Serial.printf("Volume %d\n", cur_volume);
		change_volume(cur_volume); // 0...21
		display_volume(cur_volume);
	}

	if (pressed(STATION_DOWN))
	{
		if (cur_station > 0)
		{
		cur_station--;
		updateStation = true;
		}
	}

	if (pressed(STATION_UP))
	{
		if (cur_station < (sizeof(stations) / sizeof(stations[0])-1)) 
		{
		cur_station++;
		updateStation = true;
		}
	}

	if (pressed(RESET_BUTTON))
	{
		preferences.begin("preferences", false);
		preferences.clear(); // Clear Preferences
		delay (5000); // Wait 5sec
		ESP.restart(); // Restart device
	}

	if (updateStation)
	{
        // Station change requested
        if (DEBUG == true) Serial.printf("Station %d\n", cur_station);
		display_station(cur_station);
        change_radiostation(cur_station);
	}

	if (display_on) 
	{
		if (millis() > (debounce + 3000))
		{
			oled.clearDisplay();
			oled.display();
			display_on=false;
		}
	}


}


// next code is optional:
void vs1053_info(const char *info) {                // called from vs1053
    Serial.print("DEBUG:        ");
    Serial.println(info);                           // debug infos
}
void vs1053_showstation(const char *info){          // called from vs1053
    Serial.print("STATION:      ");
    Serial.println(info);                           // Show station name
}
void vs1053_showstreamtitle(const char *info){      // called from vs1053
    Serial.print("STREAMTITLE:  ");
    Serial.println(info);                           // Show title
}
void vs1053_showstreaminfo(const char *info){       // called from vs1053
    Serial.print("STREAMINFO:   ");
    Serial.println(info);                           // Show streaminfo
}
void vs1053_eof_mp3(const char *info){              // called from vs1053
    Serial.print("vs1053_eof:   ");
    Serial.print(info);                             // end of mp3 file (filename)
}
void vs1053_bitrate(const char *br){                // called from vs1053
    Serial.print("BITRATE:      ");
    Serial.println(String(br)+"kBit/s");            // bitrate of current stream
}
void vs1053_commercial(const char *info){           // called from vs1053
    Serial.print("ADVERTISING:  ");
    Serial.println(String(info)+"sec");             // info is the duration of advertising
}
void vs1053_icyurl(const char *info){               // called from vs1053
    Serial.print("Homepage:     ");
    Serial.println(info);                           // info contains the URL
}
void vs1053_eof_speech(const char *info){           // called from vs1053
    Serial.print("end of speech:");
    Serial.println(info);
}
void vs1053_lasthost(const char *info){             // really connected URL
    Serial.print("lastURL:      ");
    Serial.println(info);
}