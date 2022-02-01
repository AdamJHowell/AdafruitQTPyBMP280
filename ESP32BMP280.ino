/*
 * This sketch is a branc of my PubSubWeather sketch.
 * This sketch will use a AHT20/BMP280 combination sensor to show temperature, pressure, and humidity.
 * The ESP-32 SDA pin is GPIO21, and SCL is GPIO22.
 */
#include "WiFi.h"						// This header is part of the standard library.  https://www.arduino.cc/en/Reference/WiFi
#include <Wire.h>						// This header is part of the standard library.  https://www.arduino.cc/en/reference/wire
#include <PubSubClient.h>			// PubSub is the MQTT API.  Author: Nick O'Leary  https://github.com/knolleary/pubsubclient
#include <Adafruit_BMP280.h>		// The AdaFruit BMP280 library	https://github.com/adafruit/Adafruit_BMP280_Library
#include "privateInfo.h"			// I use this file to hide my network information from random people browsing my GitHub repo.
#include <Adafruit_NeoPixel.h>	// The Adafruit NeoPixel library to drive the RGB LED on the QT Py.	https://github.com/adafruit/Adafruit_NeoPixel

// How many internal neopixels do we have? some boards have more than one!
#define NUMPIXELS        1

/**
 * Declare network variables.
 * Adjust the commented-out variables to match your network and broker settings.
 * The commented-out variables are stored in "privateInfo.h", which I do not upload to GitHub.
 */
//const char* wifiSsid = "yourSSID";				// Typically kept in "privateInfo.h".
//const char* wifiPassword = "yourPassword";		// Typically kept in "privateInfo.h".
//const char* mqttBroker = "yourBrokerAddress";	// Typically kept in "privateInfo.h".
//const int mqttPort = 1883;							// Typically kept in "privateInfo.h".
const char* mqttTopic = "ajhWeather";
const String sketchName = "ESP32BMP280";
const float seaLevelPressure = 1024.0;		// Adjust this to the sea level pressure (in hectopascals) for your local weather conditions.
const char* notes = "Adafruit ESP32-S2 QT Py";
char ipAddress[16];
char macAddress[18];
int loopCount = 0;
int mqttPublishDelayMS = 60000;
int pirPin = 8;											// The GPIO that the PIR "out" pin is connected to.

// Create class objects.
WiFiClient espClient;							// Network client.
PubSubClient mqttClient( espClient );		// MQTT client.
Adafruit_BMP280 bmp;
Adafruit_NeoPixel pixels( NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800 );


/**
 * The setup() function runs once when the device is booted, and then loop() takes over.
 */
void setup()
{
	// Start the Serial communication to send messages to the computer.
	Serial.begin( 115200 );
	while ( !Serial )
		delay( 100 );
	Wire.setPins( SDA1, SCL1 );
	Wire.begin();

#if defined( NEOPIXEL_POWER )
	// If this board has a power control pin, we must set it to output and high in order to enable the NeoPixels.
	// We put this in an #ifdef so it can be reused for other boards without compilation errors.
	pinMode( NEOPIXEL_POWER, OUTPUT );
	digitalWrite( NEOPIXEL_POWER, HIGH );
#endif
	// Initialize the NeoPixel.
	pixels.begin();
	pixels.setBrightness( 20 );

	delay( 10 );
	Serial.println( '\n' );
	Serial.println( sketchName + " is beginning its setup()." );
//	Wire.begin();	// Join I2C bus.

	// Set the ipAddress char array to a default value.
	snprintf( ipAddress, 16, "127.0.0.1" );

	Wire1.setPins( SDA1, SCL1 );

	Serial.println( "Initializing the BMP280 sensor..." );
	unsigned status;
	status = bmp.begin( BMP280_ADDRESS_ALT, BMP280_CHIPID );
//	status = bmp.begin();
	if ( !status )
	{
		Serial.println( F( "Could not find a valid BMP280 sensor, check wiring or try a different address!" ) );
		Serial.print( "Status: " );
		Serial.println( status );
		Serial.print( "SensorID was: 0x");
		Serial.println( bmp.sensorID(), 16 );
		Serial.print( "        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n" );
		Serial.print( "   ID of 0x56-0x58 represents a BMP 280,\n" );
		Serial.print( "        ID of 0x60 represents a BME 280.\n" );
		Serial.print( "        ID of 0x61 represents a BME 680.\n" );
		while( 1 )
			delay( 10 );
	}

	/* Default settings from datasheet. */
	bmp.setSampling( Adafruit_BMP280::MODE_NORMAL,		/* Operating Mode. */
		Adafruit_BMP280::SAMPLING_X2,			/* Temp. oversampling */
		Adafruit_BMP280::SAMPLING_X16,		/* Pressure oversampling */
		Adafruit_BMP280::FILTER_X16,			/* Filtering. */
		Adafruit_BMP280::STANDBY_MS_500 );	/* Standby time. */

	// Set the MQTT client parameters.
	mqttClient.setServer( mqttBroker, mqttPort );

	// Get the MAC address and store it in macAddress.
	snprintf( macAddress, 18, "%s", WiFi.macAddress().c_str() );

	// Try to connect to the configured WiFi network, up to 10 times.
	wifiConnect( 20 );
} // End of setup() function.


void wifiConnect( int maxAttempts )
{
	// Announce WiFi parameters.
	String logString = "WiFi connecting to SSID: ";
	logString += wifiSsid;
	Serial.println( logString );

	// Connect to the WiFi network.
	Serial.printf( "Wi-Fi mode set to WIFI_STA %s\n", WiFi.mode( WIFI_STA ) ? "" : " - Failed!" );
	WiFi.begin( wifiSsid, wifiPassword );

	int i = 0;
	/*
     WiFi.status() return values:
     0 : WL_IDLE_STATUS when WiFi is in process of changing between statuses
     1 : WL_NO_SSID_AVAIL in case configured SSID cannot be reached
     3 : WL_CONNECTED after successful connection is established
     4 : WL_CONNECT_FAILED if wifiPassword is incorrect
     6 : WL_DISCONNECTED if module is not configured in station mode
  */
	// Loop until WiFi has connected.
	while( WiFi.status() != WL_CONNECTED && i < maxAttempts )
	{
		delay( 1000 );
		Serial.println( "Waiting for a connection..." );
		Serial.print( "WiFi status: " );
		Serial.println( WiFi.status() );
		logString = ++i;
		logString += " seconds";
		Serial.println( logString );
	}

	// Print that WiFi has connected.
	Serial.println( '\n' );
	Serial.println( "WiFi connection established!" );
	Serial.print( "MAC address: " );
	Serial.println( macAddress );
	Serial.print( "IP address: " );
	snprintf( ipAddress, 16, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
	Serial.println( ipAddress );
} // End of wifiConnect() function.


// mqttConnect() will attempt to (re)connect the MQTT client.
void mqttConnect( int maxAttempts )
{
	int i = 0;
	// Loop until MQTT has connected.
	while( !mqttClient.connected() && i < maxAttempts )
	{
		Serial.print( "Attempting MQTT connection..." );
		// Connect to the broker using the MAC address for a clientID.  This guarantees that the clientID is unique.
		if( mqttClient.connect( macAddress ) )
		{
			Serial.println( "connected!" );
		}
		else
		{
			Serial.print( " failed, return code: " );
			Serial.print( mqttClient.state() );
			Serial.println( " try again in 2 seconds" );
			// Wait 5 seconds before retrying.
			delay( 5000 );
		}
		i++;
	}
} // End of mqttConnect() function.


/**
 * The loop() function begins after setup(), and repeats as long as the unit is powered.
 */
void loop()
{
	loopCount++;
	Serial.println();
	Serial.println( sketchName );

	// Set color to red and wait a half second.
	pixels.fill( 0xFF0000 );
	pixels.show();
	delay( 500 );

	// Check the mqttClient connection state.
	if( !mqttClient.connected() )
	{
		// Reconnect to the MQTT broker.
		mqttConnect( 10 );
	}
	// The loop() function facilitates the receiving of messages and maintains the connection to the broker.
	mqttClient.loop();

	// Get temperature and humidity data from the BMP280.
	float bmpTemp = bmp.readTemperature();
	float bmpPressure = bmp.readPressure();
	float bmpAlt = bmp.readAltitude( seaLevelPressure );

	// Print the BMP280 data.
	Serial.print( "BMP280 Temperature:  " );
	Serial.print( bmpTemp, 2 );
	Serial.print( " C\t" );
	Serial.print( "Pressure: " );
	Serial.print( bmpPressure, 0 );
	Serial.print( " Pa\t" );
	Serial.print( "Altitude: " );
	Serial.print( bmpAlt, 2 );
	Serial.print( " m\n" );

	// Set color to blue and wait a half second.
	pixels.fill( 0x0000FF );
	pixels.show();
	delay( 500 );

	// Prepare a String to hold the JSON.
	char mqttString[256];
	// Write the readings to the String in JSON format.
	snprintf( mqttString, 256, "{\n\t\"sketch\": \"%s\",\n\t\"mac\": \"%s\",\n\t\"ip\": \"%s\",\n\t\"tempC\": %.2f,\n\t\"pressure\": %.1f,\n\t\"altitude\": %.1f,\n\t\"uptime\": %d,\n\t\"notes\": \"%s\"\n}", sketchName, macAddress, ipAddress, bmpTemp, bmpPressure, bmpAlt, loopCount, notes );
	if( mqttClient.connected() )
	{
		// Publish the JSON to the MQTT broker.
		mqttClient.publish( mqttTopic, mqttString );
	}
	else
	{
		Serial.println( "Lost connection to the MQTT broker between the start of this loop and now!" );
	}
	// Print the JSON to the Serial port.
	Serial.println( mqttString );

	String logString = "loopCount: ";
	logString += loopCount;
	Serial.println( logString );
	Serial.println( "Pausing for 60 seconds..." );

	// Set color to green and wait one minute.
	pixels.fill( 0x00FF00 );
	pixels.show();

	delay( 60000 );	// Wait for 60 seconds.
} // End of loop() function.
