/*
 Name:		subSistemaC.ino
 Created:	20/03/2021 17:54:54
 Author:	josep
*/

// the setup function runs once when you press reset or power the board

#include <WiFi.h>
#include <Preferences.h>
#include "BluetoothSerial.h"
#include "Esp32MQTTClient.h"

#define INTERVAL 10000
#define MESSAGE_MAX_LEN 256

static const char* connectionString = "HostName=SubSistemaC.azure-devices.net;DeviceId=ESP32;SharedAccessKey=84KvSToJV8XDwBWGiaRHJGt2Xq5NydODVBSV1MqEsVA=";
const char* messageData = "{\"messageId\":%d, \"nombreDerrivada\":%f, \"dataDerrivada\":%f}";
int messageCount = 1;
static bool messageSending = true;
static uint64_t send_interval_ms;

String ssids_array[50];
String network_string;
String connected_string;

const char* pref_ssid = "";
const char* pref_pass = "";
String client_wifi_ssid;
String client_wifi_password;

const char* bluetooth_name = "Holter";

long start_wifi_millis;
long wifi_timeout = 10000;
bool bluetooth_disconnect = false;

enum wifi_setup_stages { NONE, SCAN_START, SCAN_COMPLETE, SSID_ENTERED, WAIT_PASS, PASS_ENTERED, WAIT_CONNECT, CHECK_STATUS, LOGIN_FAILED };
enum wifi_setup_stages wifi_stage = NONE;

BluetoothSerial SerialBT;
Preferences preferences;

void setup() {

	Serial.begin(9600);
	Serial.println("Booting...");

	preferences.begin("wifi_access", false);

	if (!init_wifi()) { // Connect to Wi-Fi fails
		SerialBT.register_callback(callback);
	}
	else {
		SerialBT.register_callback(callback_show_ip);
	}

	SerialBT.begin(bluetooth_name);

}


// the loop function runs over and over again until power down or reset
void loop() {


	if (bluetooth_disconnect)
	{
		disconnect_bluetooth();
	}
	switch (wifi_stage)
	{
	case SCAN_START:
		SerialBT.println("Scanning Wi-Fi networks");
		Serial.println("Scanning Wi-Fi networks");
		scan_wifi_networks();
		SerialBT.println("Please enter the number for your Wi-Fi");
		wifi_stage = SCAN_COMPLETE;
		break;

	case SSID_ENTERED:
		SerialBT.println("Please enter your Wi-Fi password");
		Serial.println("Please enter your Wi-Fi password");
		wifi_stage = WAIT_PASS;
		break;

	case PASS_ENTERED:
		SerialBT.println("Please wait for Wi-Fi connection...");
		Serial.println("Please wait for Wi_Fi connection...");
		wifi_stage = WAIT_CONNECT;
		preferences.putString("pref_ssid", client_wifi_ssid);
		preferences.putString("pref_pass", client_wifi_password);
		if (init_wifi()) { // Connected to WiFi
			connected_string = "ESP32 IP: ";
			connected_string = connected_string + WiFi.localIP().toString();
			SerialBT.println(connected_string);
			Serial.println(connected_string);
			bluetooth_disconnect = true;
			wifi_stage = CHECK_STATUS;
		}
		else { // try again
			wifi_stage = LOGIN_FAILED;
		}
		break;
	case CHECK_STATUS:
		SerialBT.println("Preparando para enviar eventos...");
		if (check_status_wifi && check_status_iot) {
			if (messageSending &&
				(int)(millis() - send_interval_ms) >= INTERVAL) {
				char messagePayload[MESSAGE_MAX_LEN];

				const char* Derrivada = "1;2;3;4;5";

				snprintf(messagePayload, MESSAGE_MAX_LEN, messageData, messageCount++, "I", Derrivada);
				Serial.println(messagePayload);
				EVENT_INSTANCE* message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);
				Esp32MQTTClient_SendEventInstance(message);
				send_interval_ms = millis();
			}
			else {
				Esp32MQTTClient_Check();
			}
			delay(10);
		}
		else {
			connect_bluetooth();
		}
		break;
	case LOGIN_FAILED:
		SerialBT.println("Wi-Fi connection failed");
		Serial.println("Wi-Fi connection failed");
		delay(2000);
		wifi_stage = SCAN_START;
		break;
	default:
		break;
	}
}


bool init_wifi()
{
	Serial.println("Entrado al proceso: init_wifi");

	String temp_pref_ssid = preferences.getString("pref_ssid");
	String temp_pref_pass = preferences.getString("pref_pass");
	pref_ssid = temp_pref_ssid.c_str();
	pref_pass = temp_pref_pass.c_str();

	Serial.println(pref_ssid);
	Serial.println(pref_pass);

	WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);

	start_wifi_millis = millis();
	WiFi.begin(pref_ssid, pref_pass);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
		if (millis() - start_wifi_millis > wifi_timeout) {
			WiFi.disconnect(true, true);
			Serial.println("init_wifi retorno: false");
			return false;
		}
	}
	Serial.println("init_wifi retorno: true");
	return true;
}

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param)
{

	Serial.println("Entrado al proceso: callback");


	if (event == ESP_SPP_SRV_OPEN_EVT) {
		wifi_stage = SCAN_START;
	}

	if (event == ESP_SPP_DATA_IND_EVT && wifi_stage == SCAN_COMPLETE) { // data from phone is SSID
		int client_wifi_ssid_id = SerialBT.readString().toInt();
		client_wifi_ssid = ssids_array[client_wifi_ssid_id];
		wifi_stage = SSID_ENTERED;
	}

	if (event == ESP_SPP_DATA_IND_EVT && wifi_stage == WAIT_PASS) { // data from phone is password
		client_wifi_password = SerialBT.readString();
		client_wifi_password.trim();
		wifi_stage = PASS_ENTERED;
	}

}

void callback_show_ip(esp_spp_cb_event_t event, esp_spp_cb_param_t* param)
{
	Serial.println("Entrado al proceso: callback_show_ip");
	if (event == ESP_SPP_SRV_OPEN_EVT) {
		SerialBT.print("ESP32 IP: ");
		SerialBT.println(WiFi.localIP());
		bluetooth_disconnect = true;
	}
}

void disconnect_bluetooth()
{
	Serial.println("Entrado al proceso: disconnect_bluetooth");
	delay(1000);
	Serial.println("BT stopping");
	SerialBT.println("Bluetooth disconnecting...");
	delay(1000);
	SerialBT.flush();
	SerialBT.disconnect();
	SerialBT.end();
	Serial.println("BT stopped");
	delay(1000);
	bluetooth_disconnect = false;
}

void connect_bluetooth()
{
	Serial.println("Entrado al proceso: disconnect_bluetooth");
	delay(1000);
	Serial.println("BT init");
	SerialBT.begin(bluetooth_name);
	delay(1000);
	Serial.println("BT start");
	delay(1000);
}

void scan_wifi_networks()
{
	WiFi.mode(WIFI_STA);
	// WiFi.scanNetworks will return the number of networks found
	int n = WiFi.scanNetworks();
	if (n == 0) {
		SerialBT.println("no networks found");
	}
	else {
		SerialBT.println();
		SerialBT.print(n);
		SerialBT.println(" networks found");
		delay(1000);
		for (int i = 0; i < n; ++i) {
			ssids_array[i + 1] = WiFi.SSID(i);
			Serial.print(i + 1);
			Serial.print(": ");
			Serial.println(ssids_array[i + 1]);
			network_string = i + 1;
			network_string = network_string + ": " + WiFi.SSID(i) + " (Strength:" + WiFi.RSSI(i) + ")";
			SerialBT.println(network_string);
		}
		wifi_stage = SCAN_COMPLETE;
	}
}

bool check_status_wifi() {

	start_wifi_millis = millis();
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
		if (millis() - start_wifi_millis > wifi_timeout) {
			WiFi.disconnect(true, true);
			Serial.println("init_wifi retorno: false");
			return false;
		}
	}
	Serial.println("init_wifi retorno: true");
	return true;

}


bool check_status_iot() {

	if (!Esp32MQTTClient_Init((const uint8_t*)connectionString, true))
	{
		Serial.println("Initializing IoT hub failed.");
		return false;
	}
	else {
		Esp32MQTTClient_SetSendConfirmationCallback(SendConfirmationCallback);
		Esp32MQTTClient_SetMessageCallback(MessageCallback);
		Esp32MQTTClient_SetDeviceTwinCallback(DeviceTwinCallback);
		Esp32MQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);
		Serial.println("Start sending events.");
		randomSeed(analogRead(0));
		send_interval_ms = millis();
		return true;
	}

}


static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
	if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
	{
		Serial.println("Send Confirmation Callback finished.");
	}
}

static void MessageCallback(const char* payLoad, int size)
{
	Serial.println("Message callback:");
	Serial.println(payLoad);
}

static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char* payLoad, int size)
{
	char* temp = (char*)malloc(size + 1);
	if (temp == NULL)
	{
		return;
	}
	memcpy(temp, payLoad, size);
	temp[size] = '\0';
	// Display Twin message.
	Serial.println(temp);
	free(temp);
}

static int DeviceMethodCallback(const char* methodName, const unsigned char* payload, int size, unsigned char** response, int* response_size)
{
	LogInfo("Try to invoke method %s", methodName);
	const char* responseMessage = "\"Successfully invoke device method\"";
	int result = 200;

	if (strcmp(methodName, "start") == 0)
	{
		LogInfo("Start sending temperature and humidity data");
		messageSending = true;
	}
	else if (strcmp(methodName, "stop") == 0)
	{
		LogInfo("Stop sending temperature and humidity data");
		messageSending = false;
	}
	else
	{
		LogInfo("No method %s found", methodName);
		responseMessage = "\"No method found\"";
		result = 404;
	}

	*response_size = strlen(responseMessage) + 1;
	*response = (unsigned char*)strdup(responseMessage);

	return result;
}

