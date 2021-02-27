#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

// BLE Service
BLEDfu  bledfu;  // OTA DFU service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble
BLEBas  blebas;  // battery

#define DEVICE_NAME "BLEinky"
// brightness 5 is the minimum to have more options than just RGB
#define DEFAULT_BRIGHTNESS 5
#define MAX_CMD_LIST_LEN 5
#define PIN 11
Adafruit_NeoPixel strip = Adafruit_NeoPixel(12, PIN, NEO_GRB + NEO_KHZ800);

#define COLOR_WAITING 0
#define COLOR_BUSY 1
#define COLOR_FREE 2
#define COLOR_UNKNOWN 3
struct AppState
{
  int color;
} appState;

struct CmdList
{
  int n;
  char* list[MAX_CMD_LIST_LEN];
};

struct CmdConfig
{
  CmdList busy;
  CmdList avail;
  CmdList wave;
} cmdConfig;

void setup()
{
  Serial.begin(115200);

#if CFG_DEBUG
  // Blocking wait for connection when debug mode is enabled via IDE
  while ( !Serial ) yield();
#endif

  Serial.println(DEVICE_NAME);
  Serial.println("---------------------------\n");

  initCmdConfig();

  strip.begin();
  strip.setBrightness(DEFAULT_BRIGHTNESS);
  strip.show(); // Initialize with pixels off'
  
  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behavior, but provided
  // here in case you want to control this LED manually via PIN 19
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName(DEVICE_NAME);
  //Bluefruit.setName(getMcuUniqueID()); // useful testing with multiple central connections
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();

  // Configure and Start BLE Uart Service
  bleuart.begin();

  // Start BLE Battery Service
  blebas.begin();
  blebas.write(100);

  // Set up and start advertising
  startAdv();

  Serial.println("Please use Adafruit's Bluefruit LE app to connect in UART mode");
  Serial.println("Once connected, enter character(s) that you wish to send");
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();

  /* Start Advertising
     - Enable auto advertising if disconnected
     - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
     - Timeout for fast mode is 30 seconds
     - Start(timeout) with timeout = 0 will advertise forever (until connected)

     For recommended advertising interval
     https://developer.apple.com/library/content/qa/qa1931/_index.html
  */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

void loop()
{
  // Forward data from HW Serial to BLEUART
  while (Serial.available())
  {
    // Delay to wait for enough input, since we have a limited transmission buffer
    delay(2);

    uint8_t buf[64];
    int count = Serial.readBytes(buf, sizeof(buf));
    bleuart.write( buf, count );
  }
  char cmd[32];
  int cmd_idx = 0;
  // Forward from BLEUART to HW Serial
  while ( bleuart.available() )
  {
    uint8_t ch;
    ch = (uint8_t) bleuart.read();
    if (cmd_idx > 31) {
      Serial.println("WARN message too long");
    } else {
      cmd[cmd_idx] = ch;
      cmd_idx++;
    }
  }
  int end = cmd_idx > 31 ? 31 : cmd_idx;
  cmd[end] = '\0';

  handleCmd(cmd);
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);

  strip.setBrightness(DEFAULT_BRIGHTNESS);
  strip.show();
  setColor(COLOR_WAITING);
}

/**
   Callback invoked when a connection is dropped
   @param conn_handle connection where this event happens
   @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
*/
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.print("Disconnected, reason: "); Serial.println(reason, HEX);

  strip.setBrightness(0);
  strip.show();
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

void handleCmd(char cmd[]) {
  if (cmd[0] == '\0') return;

  if (cmd_in(cmd, cmdConfig.busy))
  {
    setColor(COLOR_BUSY);
    ack(cmd);
  }
  else if (cmd_in(cmd, cmdConfig.avail))
  {
    setColor(COLOR_FREE);
    ack(cmd);
  }
  else if (cmd_in(cmd, cmdConfig.wave)) {
    ack("...\n");
    rainbowCycle(5);
    setColor(appState.color);
    ack(cmd);
  }
  else if (strstr(cmd, "light ") != NULL)
  {
    if (!adjustBrightness(cmd))
    {
      ackUnknown(cmd);
      return;
    }
    ack(cmd);
  }
  else
  {
    ackUnknown(cmd);
  }
}

bool cmd_in(char cmd[], CmdList list)
{
  for (int i = 0; i < list.n; i++)
  {
    if (strcmp(cmd, list.list[i]) == 0) return true;
  }
  return false;
}

void setColor(int colorEnum)
{
  switch (colorEnum)
  {
    case COLOR_WAITING:
      colorWipe(strip.Color(38, 142, 212), 50);
      appState.color = COLOR_WAITING;
      break;
    case COLOR_BUSY:
      colorWipe(strip.Color(255, 0, 0), 50);
      appState.color = COLOR_BUSY;
      break;
    case COLOR_FREE:
      colorWipe(strip.Color(0, 255, 0), 50);
      appState.color = COLOR_FREE;
      break;
    default:
      colorWipe(strip.Color(255, 180, 41), 50);
      appState.color = COLOR_UNKNOWN;
  }
}

bool adjustBrightness(char* msg)
{
  char charNum[4];
  int charNumEnd = 0;
  for (int i = 0; msg[i] != '\0'; i++)
  {
    if (isdigit(msg[i]))
    {
      charNum[charNumEnd] = msg[i];
      charNumEnd++;
    }
  }
  charNum[charNumEnd] = '\0';
  int val = atoi(charNum);
//  Serial.println(val);
  if (val < 1 || val > 100) {
    return false;
  }
  strip.setBrightness(val);
  strip.show();
  return true;
}

void initCmdConfig()
{
  cmdConfig.busy.list[0] = "red\n";
  cmdConfig.busy.list[1] = "busy\n";
  cmdConfig.busy.list[2] = "b\n";
  cmdConfig.busy.n = 3;

  cmdConfig.avail.list[0] = "green\n";
  cmdConfig.avail.list[1] = "free\n";
  cmdConfig.avail.list[2] = "f\n";
  cmdConfig.avail.n = 3;

  cmdConfig.wave.list[0] = "wave\n";
  cmdConfig.wave.list[1] = "cycle\n";
  cmdConfig.wave.list[2] = "meow\n";
  cmdConfig.wave.n = 3;
}

void ack(const char* msg)
{
  char buf[32] = "ack: \0";
  if (strlen(buf) + strlen(msg) < 31)
  {
    strcat(buf, msg);
  }
  bleuart.write(buf, strlen(buf));
}

void ackUnknown(const char* msg)
{
  char buf[32] = "? '\0";
  if (strlen(msg) + 4 > 32)
  {
    bleuart.write(buf, strlen(buf));
    return;
  }
  strncat(buf, msg, strlen(msg)-1);
  strcat(buf, "'");
  strcat(buf, "\n");
  bleuart.write(buf, strlen(buf));
}
