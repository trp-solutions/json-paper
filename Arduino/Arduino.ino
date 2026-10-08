#include <WiFi.h>
#include <EEPROM.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp32-hal-psram.h>
#include <time.h>
#include "arduino_secrets.h"
#include <ArduinoJson.h>
#include "src/core/core.h"
#include "src/e-paper/epd_5in79g.h"
#include "src/e-paper/paper_command.h"
#include "src/network/request.h"
#include "src/util/clock.h"
#include "logo.h"
#include "templates/status.h"

char ssid[] = CONFIG_AP_SSID;
char pass[] = CONFIG_AP_PASSWORD;

#define RGB_LED_PIN 27
#define BUTTON_PIN 0
#define EEPROM_SIZE 512
#define SETTINGS_NAMESPACE "json-paper"
#define SETTINGS_VERSION 1
#define CLOCK_RETRY_SECONDS (15ULL * 60ULL)
// Temporary diagnostic setting. Change to 0 to restore dual-band selection.
#define FORCE_WIFI_2_4_GHZ 0

const char *DEFAULT_JSON_URL = "https://raw.githubusercontent.com/trp-solutions/json-paper/refs/heads/main/Server/hello-world.json";
const char *DEFAULT_CRON = "0 * * * *";
const char *COPENHAGEN_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";

int status = WL_IDLE_STATUS;
WiFiServer server(80);
Preferences preferences;

String getEerom = "";

String ssidAP = "";
String passAP = "";
String endpoint = DEFAULT_JSON_URL;
String cronExpression = DEFAULT_CRON;

String ssidName = "ssidInput";
String passName = "passInput";
String endpointName = "endpointInput";
String cronName = "cronInput";

bool isSaved = false;
bool connectFail = false;

bool configMode = false;
bool apStarted = false;
bool settingsLoaded = false;

// Button
int newBtnState;  // the current state of button
int prevBtnState;

struct CronSchedule {
  int minute;
  int hour;
  int dayOfMonth;
  int month;
  int dayOfWeek;
  bool anyMinute;
  bool anyHour;
  bool anyDayOfMonth;
  bool anyMonth;
  bool anyDayOfWeek;
};

void LogTitle(String title);
void loadSettings();
void ButtonClick();
void APConnect();
void updateLED();
void runRefreshCycle();
int paintCommands(const std::vector<PaperCommand>& commands);
String formValue(const String& body, const String& name);
bool isValidEndpoint(const String& value);
bool parseCron(String expression, CronSchedule& schedule, String& error);
time_t nextCronTime(const CronSchedule& schedule, time_t after);
bool saveSettings();
String htmlEscape(String value);
void printWiFiStatus();
void cssPrint(WiFiClient& client);
bool WiFiConnect();
String readEEPROM();
String urlDecode(String input);
void enterDeepSleep(uint64_t sleepSeconds);
void sleepUntilNextCron(time_t now);


void setup() {
  // Match the ESP32 boot log and the PlatformIO serial monitor.
  Serial.begin(115200);

  delay(1000);

  Serial.print("PSRAM detected: ");
  Serial.println(psramFound() ? "yes" : "no");

  Serial.print("PSRAM size: ");
  Serial.println(ESP.getPsramSize());

  Serial.print("Free PSRAM: ");
  Serial.println(ESP.getFreePsram());

  LogTitle("Start JSON-Paper");

  setenv("TZ", COPENHAGEN_TZ, 1);
  tzset();

  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialize EEPROM");
  }

  // Initialize the pushbutton pin as a pull-up input
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  prevBtnState = digitalRead(BUTTON_PIN);

  loadSettings();

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
    Serial.println("Configuration button woke the device");
    configMode = true;
  } else if (ssidAP.length() == 0) {
    Serial.println("No saved WiFi credentials; starting configuration mode");
    configMode = true;
  }
}


void loop() {
  if (configMode) {
    ButtonClick();
    APConnect();
    updateLED();
    return;
  }

  runRefreshCycle();
}


int EPD_5in79g_paint(void) {
  Serial.println("\r\nStarting process to draw EPD-picture\r\n");

  // Fetch and parse before allocating the full-screen framebuffer. Keeping
  // both the HTTP/JSON working memory and the 53 KB framebuffer alive at the
  // same time can exhaust the ESP32-C5 heap.
  Serial.println("Requesting drawing commands");
  std::vector<PaperCommand> commands = Request::RequestConfig(endpoint.c_str());
  return paintCommands(commands);
}


int paintCommands(const std::vector<PaperCommand>& commands) {
  if (DEV_Module_Init() != 0) {
    return -1;
  }

  EPD_5in79g_Init();

  // Create a new image cache named IMAGE_BW and fill it with white
  UBYTE *BlackImage;
  UWORD Imagesize = ((EPD_5in79G_WIDTH % 4 == 0) ? (EPD_5in79G_WIDTH / 4) : (EPD_5in79G_WIDTH / 4 + 1)) * EPD_5in79G_HEIGHT;
  if (psramFound()) {
    BlackImage = static_cast<UBYTE *>(ps_malloc(Imagesize));
  } else {
    BlackImage = static_cast<UBYTE *>(malloc(Imagesize));
  }
  if (BlackImage == NULL) {
    Serial.println("Failed to apply for black memory...\r\n");
    EPD_5in79g_Sleep();
    DEV_Module_Exit();
    return -1;
  }
  Serial.println(
    psramFound()
      ? "Framebuffer allocated in PSRAM"
      : "Framebuffer allocated in internal RAM"
  );

  Serial.println("\r\nCreate image with properties:");
  Paint_NewImage(BlackImage, EPD_5in79G_WIDTH, EPD_5in79G_HEIGHT, 0, WHITE);
  Serial.print("   • ");
  Serial.println("Width = " + String(EPD_5in79G_WIDTH) + "px");
  Serial.print("   • ");
  Serial.println("Height = " + String(EPD_5in79G_HEIGHT) + "px");
  Serial.print("   • ");
  Serial.println("Scale = 4");
  Paint_SetScale(4);

  // Select Image
  Serial.println("\r\nSelect image");
  Paint_SelectImage(BlackImage);

  // Clear the color of the picture
  Serial.println("Clear image frame buffer");
  Paint_Clear(WHITE);

  Serial.println("Drawing commands");
  for (const auto& command : commands) {
    execute_command(command);
  }

  Serial.println("\r\nDisplay to EPD");
  EPD_5in79g_Display(BlackImage);
  DEV_Delay_ms(3000);

  Serial.println("Goto Sleep...");
  EPD_5in79g_Sleep();
  free(BlackImage);
  BlackImage = NULL;

  // Important, at least 2s delay
  DEV_Delay_ms(2000);

  // Close 5V
  Serial.println("Close 5V, Module enters 0 power consumption...");
  DEV_Module_Exit();

  Serial.println("Finished process to draw EPD-picture\r\n");
  return 0;
}


void ButtonClick() {
  // Read the state of the button:
  newBtnState = digitalRead(BUTTON_PIN);

  if (prevBtnState == LOW && newBtnState == HIGH) {
    Serial.println("The button is released");
    configMode = true;
    connectFail = false;
  }

  prevBtnState = newBtnState;
}


void updateLED() {
  // Error → constantly on
  if (connectFail) {
    rgbLedWrite(RGB_LED_PIN, 255, 96, 0);
    return;
  }

  // Connected → off
  if (status == WL_CONNECTED) {
    rgbLedWrite(RGB_LED_PIN, 0, 0, 0);
    return;
  }

  // Flash (AP + connecting)
  rgbLedWrite(RGB_LED_PIN, 255, 96, 0);
  delay(1000);
  rgbLedWrite(RGB_LED_PIN, 0, 0, 0);
  delay(1000);
}


void LogTitle(String title) {
  int totalWidth = 52;
  int lineLength = (totalWidth - title.length()) / 2;

  String line = "";
  for (int i = 0; i < lineLength; i++) {
    line += "▬";
  }

  Serial.println();
  Serial.print(line);
  Serial.print(" ");
  Serial.print(title);
  Serial.print(" ");
  Serial.println(line);
}


void APConnect() {

  rgbLedWrite(RGB_LED_PIN, 0, 0, 0);

  if (!apStarted) {
    Serial.println("Start Access Point Web Server");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);

    // Print the SSID (SSID);
    Serial.println("Creating access point...");

    IPAddress apAddress(192, 48, 56, 2);
    IPAddress subnet(255, 255, 255, 0);
    if (!WiFi.softAPConfig(apAddress, apAddress, subnet) ||
        !WiFi.softAP(ssid, pass)) {
      Serial.println("Creating access point failed");
      if (!connectFail) paintCommands(Templates::otherError("Could not start the setup access point."));
      connectFail = true;
      updateLED();
      return;
    }

    apStarted = true;
    status = WL_IDLE_STATUS;
    delay(1000);

    // Start the web server on port 80
    server.begin();

    // You're connected now, so print out the status
    printWiFiStatus();
    paintCommands(Templates::setup(ssid, pass,
      std::string("http://") + WiFi.softAPIP().toString().c_str()));
  }


  WiFiClient client = server.available();

  if (client) {
    Serial.println("new client");
    String currentLine = "";
    int contentLength = 0;
    bool isPost = false;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        currentLine += c;

        if (c == '\n') {

          // POST
          if (currentLine.startsWith("POST /")) {
            isPost = true;
          }

          // Content-Length
          if (currentLine.startsWith("Content-Length:")) {
            contentLength = currentLine.substring(15).toInt();
          }

          if (currentLine == "\r\n") {


            String formError = "";
            if (isPost && contentLength > 0 && contentLength <= 2048) {
              String body = "";
              unsigned long bodyStartTime = millis();

              while (body.length() < contentLength &&
                     millis() - bodyStartTime < 5000) {
                if (client.available()) {
                  char c = client.read();
                  body += c;
                }
              }

              String candidateSsid = formValue(body, ssidName);
              String submittedPass = formValue(body, passName);
              String candidateEndpoint = formValue(body, endpointName);
              String candidateCron = formValue(body, cronName);
              candidateEndpoint.trim();
              candidateCron.trim();
              String candidatePass = submittedPass.length() == 0
                ? passAP
                : submittedPass;

              if (body.length() != contentLength) {
                formError = "The request body was incomplete.";
              } else if (candidateSsid.length() == 0 ||
                         candidateSsid.length() > 32) {
                formError = "SSID must contain between 1 and 32 characters.";
              } else if (candidatePass.length() < 8 ||
                         candidatePass.length() > 63) {
                formError = "Password must contain between 8 and 63 characters.";
              } else if (!isValidEndpoint(candidateEndpoint)) {
                formError = "Endpoint must be a valid HTTP or HTTPS URL.";
              } else {
                CronSchedule candidateSchedule;
                String cronError;
                if (!parseCron(candidateCron, candidateSchedule, cronError) ||
                    nextCronTime(candidateSchedule, 1704067200LL) == 0) {
                  formError = cronError.length() > 0
                    ? cronError
                    : "Cron schedule has no possible occurrence.";
                }
              }

              if (formError.length() == 0) {
                Serial.println("Saving submitted device configuration");
                ssidAP = candidateSsid;
                passAP = candidatePass;
                endpoint = candidateEndpoint;
                cronExpression = candidateCron;
                if (saveSettings()) {
                  isSaved = true;
                  connectFail = false;
                } else {
                  formError = "Could not save configuration.";
                }
              }
            } else if (isPost) {
              formError = "Configuration request is empty or too large.";
            }

            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html; charset=utf-8");
            client.println("Connection: close");
            client.println();


            client.println("<!DOCTYPE html>");
            client.println("<html lang='en'>");
            client.println("<head>");
            client.println("<meta charset='UTF-8' />");
            client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0' />");
            client.println("<title>WiFi Configuration</title>");
            client.println("<style>");
            cssPrint(client);
            client.println("</style>");


            client.println("<body>");
            client.println("<div class='container'>");

            if (!isSaved) {
              client.println("<form id='wifiForm' method='POST' action='/'>");
              client.println(logoSvg);
              client.println("<p>Device configuration</p>");
              if (connectFail) {
                client.println("<p class='error'>Could not connect. Check the network name and password, then try again.</p>");
              }
              if (formError.length() > 0) {
                client.println("<p class='error'>" + htmlEscape(formError) + "</p>");
              }
              client.println("<input id='ssid' type='text' maxlength='32' name='" + ssidName + "' placeholder='SSID' value='" + htmlEscape(ssidAP) + "' required>");
              client.println("<input id='password' type='password' maxlength='63' name='" + passName + "' placeholder='" + String(passAP.length() > 0 ? "Password (blank keeps saved)" : "Password") + "'>");
              client.println("<input id='endpoint' type='url' maxlength='512' name='" + endpointName + "' placeholder='https://example.com/display.json' value='" + htmlEscape(endpoint) + "' required>");
              client.println("<input id='cron' type='text' name='" + cronName + "' placeholder='0 * * * *' value='" + htmlEscape(cronExpression) + "' required>");
              client.println("<small>Cron: minute hour day month weekday. Use a number or * in each field. Sunday is 0.</small>");
              client.println("<input id='connectBtn' type='submit' value='Save' />");
              client.println("</form>");
            } else {
                client.println("<div class='wifi-status'>");
                client.println(logoSvg);
                client.println("<p>Device configuration</p>");
                client.println("<ul>");
                client.println("<li>SSID: " + htmlEscape(ssidAP) + "</li>");
                String hidePassAP = "";
                for (int i = 0; i < passAP.length(); i++) {
                  hidePassAP += "*";
                }
                client.println("<li>Password: " + hidePassAP + "</li>");
                client.println("<li>Endpoint: " + htmlEscape(endpoint) + "</li>");
                client.println("<li>Cron: " + htmlEscape(cronExpression) + "</li>");
                client.println("</ul>");
                client.println("<div class='loading-row' aria-label='Loading'>");
                client.println("<div class='spinner'></div>");
                client.println("</div>");
                client.println("<p><i>No orange light = Connected</i></p>");
                client.println("<p><i>Orange light = Not connected</i></p>");
                client.println("</div>");

                configMode = false;
            }
              client.println("</div>");

              client.println("</body>");
              client.println("</html>");
            break;
          }

          currentLine = "";
        }
      }
    }

    client.stop();
    Serial.println("client disconnected");

    if (isSaved) {
      server.end();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      apStarted = false;
      status = WL_IDLE_STATUS;
    }
  }
}


bool WiFiConnect() {

  rgbLedWrite(RGB_LED_PIN, 0, 0, 0);
  if (status != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
#if FORCE_WIFI_2_4_GHZ
    esp_err_t bandResult = esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY);
    if (bandResult == ESP_OK) {
      Serial.println("WiFi restricted to 2.4 GHz");
    } else {
      Serial.print("Could not restrict WiFi band; error: ");
      Serial.println(static_cast<int>(bandResult));
    }
#endif
    WiFi.disconnect(false, true);
    delay(500);
  }

  if (ssidAP.length() == 0) {
    Serial.println("No saved WiFi credentials; starting configuration mode");
    configMode = true;
    connectFail = false;
    return false;
  }

  if (!isSaved) Serial.print("\n");
  // Attempt to connect to WiFi network:
  Serial.println("Trying to connect to WiFi...");
  Serial.println("   • SSID: " + ssidAP);
  String hidePassAP = "";
  for (int i = 0; i < passAP.length(); i++) {
    hidePassAP += "*";
  }
  Serial.println("   • Password: " + hidePassAP);

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  // Connect to WPA/WPA2 network:
  status = WiFi.begin(ssidAP.c_str(), passAP.c_str());

  const unsigned long wifiTimeoutMs = 20000;
  const unsigned long wifiStartTime = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - wifiStartTime < wifiTimeoutMs) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  status = WiFi.status();

  if (status == WL_CONNECTED) {
    Serial.println("✓ Connected");
    Serial.print("   • IP: ");
    Serial.println(WiFi.localIP());
    connectFail = false;
  } else {
    Serial.println("✗ Error no connection");
    Serial.print("   • WiFi status: ");
    Serial.println(static_cast<int>(status));
    connectFail = true;
  }

  isSaved = false;
  updateLED();
  return status == WL_CONNECTED;
}


void loadSettings() {
  ssidAP = "";
  passAP = "";
  endpoint = DEFAULT_JSON_URL;
  cronExpression = DEFAULT_CRON;

  if (preferences.begin(SETTINGS_NAMESPACE, false)) {
    int version = preferences.getInt("version", 0);
    if (version == SETTINGS_VERSION) {
      ssidAP = preferences.getString("ssid", "");
      passAP = preferences.getString("password", "");
      endpoint = preferences.getString("endpoint", DEFAULT_JSON_URL);
      cronExpression = preferences.getString("cron", DEFAULT_CRON);
      settingsLoaded = true;
    }
    preferences.end();
  }

  if (!settingsLoaded) {
    // Migrate credentials saved by the previous raw-form EEPROM format.
    getEerom = readEEPROM();
    String legacySsid = formValue(getEerom, ssidName);
    String legacyPass = formValue(getEerom, passName);
    if (legacySsid.length() > 0) {
      Serial.println("Migrating legacy EEPROM configuration");
      ssidAP = legacySsid;
      passAP = legacyPass;
      settingsLoaded = saveSettings();
    }
  }

#if USE_HARDCODED_WIFI
  if (ssidAP.length() == 0) {
    ssidAP = WIFI_SSID;
    passAP = WIFI_PASSWORD;
  }
#endif
}


bool saveSettings() {
  if (!preferences.begin(SETTINGS_NAMESPACE, false)) {
    Serial.println("Failed to open settings storage");
    return false;
  }

  // Mark the record invalid until every value has been written.
  preferences.putInt("version", 0);
  bool saved =
    preferences.putString("ssid", ssidAP) > 0 &&
    preferences.putString("password", passAP) > 0 &&
    preferences.putString("endpoint", endpoint) > 0 &&
    preferences.putString("cron", cronExpression) > 0;
  if (saved) {
    preferences.putInt("version", SETTINGS_VERSION);
  }
  preferences.end();

  if (!saved) {
    Serial.println("Failed to save device configuration");
  }
  return saved;
}


String formValue(const String& body, const String& name) {
  String marker = name + "=";
  int start = 0;

  while (start <= body.length()) {
    int end = body.indexOf('&', start);
    if (end == -1) end = body.length();
    String field = body.substring(start, end);
    if (field.startsWith(marker)) {
      return urlDecode(field.substring(marker.length()));
    }
    start = end + 1;
  }

  return "";
}


String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  return value;
}


bool isValidEndpoint(const String& value) {
  if (value.length() == 0 || value.length() > 512) return false;

  int schemeLength = 0;
  if (value.startsWith("http://")) {
    schemeLength = 7;
  } else if (value.startsWith("https://")) {
    schemeLength = 8;
  } else {
    return false;
  }

  int hostEnd = value.indexOf('/', schemeLength);
  if (hostEnd == -1) hostEnd = value.length();
  if (hostEnd == schemeLength) return false;

  for (int i = schemeLength; i < value.length(); i++) {
    if (isWhitespace(value[i])) return false;
  }
  return true;
}


bool parseCronField(
  const String& field,
  int minimum,
  int maximum,
  int& value,
  bool& wildcard
) {
  if (field == "*") {
    wildcard = true;
    value = minimum;
    return true;
  }

  wildcard = false;
  if (field.length() == 0) return false;
  for (int i = 0; i < field.length(); i++) {
    if (!isDigit(field[i])) return false;
  }

  long parsed = field.toInt();
  if (parsed < minimum || parsed > maximum) return false;
  value = static_cast<int>(parsed);
  return true;
}


bool parseCron(
  String expression,
  CronSchedule& schedule,
  String& error
) {
  expression.trim();
  String fields[5];
  int fieldCount = 0;
  int position = 0;

  while (position < expression.length()) {
    while (position < expression.length() &&
           isWhitespace(expression[position])) {
      position++;
    }
    if (position >= expression.length()) break;
    if (fieldCount >= 5) {
      error = "Cron must contain exactly five fields.";
      return false;
    }

    int end = position;
    while (end < expression.length() &&
           !isWhitespace(expression[end])) {
      end++;
    }
    fields[fieldCount++] = expression.substring(position, end);
    position = end;
  }

  if (fieldCount != 5) {
    error = "Cron must contain exactly five fields.";
    return false;
  }

  if (!parseCronField(fields[0], 0, 59, schedule.minute,
                      schedule.anyMinute) ||
      !parseCronField(fields[1], 0, 23, schedule.hour,
                      schedule.anyHour) ||
      !parseCronField(fields[2], 1, 31, schedule.dayOfMonth,
                      schedule.anyDayOfMonth) ||
      !parseCronField(fields[3], 1, 12, schedule.month,
                      schedule.anyMonth) ||
      !parseCronField(fields[4], 0, 6, schedule.dayOfWeek,
                      schedule.anyDayOfWeek)) {
    error = "Cron fields must be * or a number within the documented range.";
    return false;
  }

  error = "";
  return true;
}


bool cronMatches(const CronSchedule& schedule, const struct tm& localTime) {
  bool minuteMatches =
    schedule.anyMinute || schedule.minute == localTime.tm_min;
  bool hourMatches =
    schedule.anyHour || schedule.hour == localTime.tm_hour;
  bool monthMatches =
    schedule.anyMonth || schedule.month == localTime.tm_mon + 1;
  bool dayOfMonthMatches =
    schedule.anyDayOfMonth || schedule.dayOfMonth == localTime.tm_mday;
  bool dayOfWeekMatches =
    schedule.anyDayOfWeek || schedule.dayOfWeek == localTime.tm_wday;

  bool dayMatches;
  if (schedule.anyDayOfMonth && schedule.anyDayOfWeek) {
    dayMatches = true;
  } else if (schedule.anyDayOfMonth) {
    dayMatches = dayOfWeekMatches;
  } else if (schedule.anyDayOfWeek) {
    dayMatches = dayOfMonthMatches;
  } else {
    dayMatches = dayOfMonthMatches || dayOfWeekMatches;
  }

  return minuteMatches && hourMatches && monthMatches && dayMatches;
}


time_t nextCronTime(const CronSchedule& schedule, time_t after) {
  time_t candidate = after - (after % 60) + 60;
  const int maxMinutes = 8 * 366 * 24 * 60;

  for (int checked = 0; checked < maxMinutes; checked++, candidate += 60) {
    struct tm localTime;
    localtime_r(&candidate, &localTime);
    if (cronMatches(schedule, localTime)) {
      return candidate;
    }
  }

  return 0;
}


bool ensureClockIsValid() {
  configTzTime(
    COPENHAGEN_TZ,
    "pool.ntp.org",
    "time.nist.gov",
    "time.cloudflare.com"
  );

  if (time(nullptr) >= DeviceClock::validEpoch) {
    Serial.println("Using retained clock while NTP synchronizes");
    return true;
  }

  Serial.print("Synchronizing clock");
  unsigned long started = millis();
  while (millis() - started < 10000) {
    if (time(nullptr) >= DeviceClock::validEpoch) {
      Serial.println("\nClock synchronized");
      return true;
    }
    Serial.print(".");
    delay(250);
  }

  Serial.println("\nNo trustworthy clock available");
  return false;
}


void enterDeepSleep(uint64_t sleepSeconds) {
  if (sleepSeconds == 0) sleepSeconds = CLOCK_RETRY_SECONDS;

  Serial.print("Deep sleeping for ");
  Serial.print(static_cast<unsigned long long>(sleepSeconds));
  Serial.println(" seconds");
  Serial.flush();

  server.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  rgbLedWrite(RGB_LED_PIN, 0, 0, 0);
  pinMode(EPD_PWR_PIN, OUTPUT);
  digitalWrite(EPD_PWR_PIN, LOW);

  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
  esp_deep_sleep_enable_gpio_wakeup(
    1ULL << BUTTON_PIN,
    ESP_GPIO_WAKEUP_GPIO_LOW
  );
  esp_deep_sleep_start();
}


void sleepUntilNextCron(time_t now) {
  CronSchedule schedule;
  String error;
  if (!parseCron(cronExpression, schedule, error)) {
    Serial.println(("Stored cron is invalid: " + error).c_str());
    paintCommands(Templates::otherError("The saved refresh schedule is invalid."));
    enterDeepSleep(CLOCK_RETRY_SECONDS);
    return;
  }

  time_t next = nextCronTime(schedule, now);
  if (next == 0 || next <= now) {
    Serial.println("Could not calculate the next cron occurrence");
    paintCommands(Templates::otherError("Could not calculate the next refresh time."));
    enterDeepSleep(CLOCK_RETRY_SECONDS);
    return;
  }

  struct tm nextLocal;
  localtime_r(&next, &nextLocal);
  char nextText[40];
  strftime(nextText, sizeof(nextText), "%Y-%m-%d %H:%M %Z", &nextLocal);
  Serial.print("Next scheduled refresh: ");
  Serial.println(nextText);

  enterDeepSleep(static_cast<uint64_t>(next - now));
}


void runRefreshCycle() {
  if (!WiFiConnect()) {
    if (configMode) return;
    paintCommands(Templates::otherError("Could not connect to WiFi. Check the network settings."));
    time_t now = time(nullptr);
    if (now >= DeviceClock::validEpoch) {
      sleepUntilNextCron(now);
    } else {
      enterDeepSleep(CLOCK_RETRY_SECONDS);
    }
    return;
  }

  if (!ensureClockIsValid()) {
    paintCommands(Templates::otherError("Could not synchronize the clock. Will retry later."));
    enterDeepSleep(CLOCK_RETRY_SECONDS);
    return;
  }

  EPD_5in79g_paint();
  sleepUntilNextCron(time(nullptr));
}


void printWiFiStatus() {
  // Print the SSID of the network you're attached to:
  Serial.print("   • SSID: ");
  Serial.println(ssid);

  // Print your WiFi shield's IP address:
  Serial.print("   • IP: ");
  IPAddress ip = WiFi.softAPIP();
  Serial.println(ip);

  // Print where to go in a browser:
  Serial.print("Open webserver website: http://");
  Serial.println(ip);
}


void clearEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, '\0');
  }
  EEPROM.commit();
}


void saveEEPROM(String value) {
  size_t length = min(static_cast<size_t>(value.length()),
                      static_cast<size_t>(EEPROM_SIZE - 1));
  for (size_t i = 0; i < length; i++) {
    EEPROM.write(i, value[i]);
  }
  EEPROM.write(length, '\0');
  EEPROM.commit();
}


String readEEPROM() {
  char data[EEPROM_SIZE];
  size_t length = 0;

  while (length < EEPROM_SIZE - 1) {
    char value = static_cast<char>(EEPROM.read(length));
    if (value == '\0' || value == static_cast<char>(0xff)) break;
    data[length++] = value;
  }
  data[length] = '\0';

  return String(data);
}


String urlDecode(String input) {
  String output = "";

  for (int i = 0; i < input.length(); i++) {
    if (input[i] == '%') {
      String hex = input.substring(i + 1, i + 3);
      char decodedChar = (char)strtol(hex.c_str(), NULL, 16);
      output += decodedChar;
      i += 2;
    } else if (input[i] == '+') {
      output += ' ';
    } else {
      output += input[i];
    }
  }

  return output;
}


void cssPrint(WiFiClient& client) {
  client.println(R"CSS(
    body {
      margin: 0;
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      font-family: "Montserrat", sans-serif;
    }

    .container {
      display: flex;
      justify-content: center;
      align-items: center;
    }

    form {
      min-height: 389.325px;
    }

    .wifi-status {
      min-height: 362.538px;
    }

    form,
    .wifi-status {
      width: min(420px, calc(100vw - 32px));
      background: hsl(237.05deg 10.48% 30.48% / 42%);
      background: hsl(237.05deg 33.32% 19.96% / 26%);
      padding: 3em;
      border-radius: 20px;
      border-left: 1px solid rgba(255, 255, 255, 0.3);
      border-top: 1px solid rgba(255, 255, 255, 0.3);
      backdrop-filter: blur(10px);
      box-shadow: 20px 20px 40px -6px rgba(0, 0, 0, 0.2);
      text-align: center;
      transition: all 0.2s ease-in-out;
    }

    svg {
      display: block;
      width: 70%;
      margin: 0 auto 50px auto;
      user-select: none;
    }

    form p,
    .wifi-status > p:first-of-type {
      font-weight: bold;
      color: hsl(227 6% 41% / 1);
      font-size: 1.2rem;
      text-align: left;
      margin: 0 auto 20px auto;
      width: 215px;
    }

    form .error {
      color: #b42318;
      font-size: 0.9rem;
      font-weight: 600;
    }

    form small {
      display: block;
      width: 215px;
      margin: -1em auto 2em auto;
      color: hsl(227 6% 41% / 1);
      line-height: 1.35;
      text-align: left;
    }

    form input {
      background: transparent;
      width: 215px;
      padding: 1em;
      margin: 0 auto 2em auto;
      border: none;
      border-left: 1px solid rgba(255, 255, 255, 0.3);
      border-top: 1px solid rgba(255, 255, 255, 0.3);
      border-radius: 5000px;
      backdrop-filter: blur(5px);
      box-shadow: 4px 4px 60px rgba(0, 0, 0, 0.2);
      color: #fff;
      font-family: "Montserrat", sans-serif;
      font-weight: 500;
      transition: all 0.2s ease-in-out;
      text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.2);
      outline: none;
      display: block;
    }

    form input:hover {
      background: rgb(0 0 0 / 10%);
      box-shadow: 4px 4px 60px 8px rgba(0, 0, 0, 0.2);
    }

    form input:focus {
      background: rgb(0 0 0 / 10%);
      box-shadow: 4px 4px 60px 8px rgba(0, 0, 0, 0.2);
    }

    form input[type="submit"] {
      margin-top: 10px;
      width: 150px;
      font-size: 1rem;
    }

    form input[type="submit"]:hover {
      cursor: pointer;
    }

    form input[type="submit"]:active {
      background: rgba(255, 255, 255, 0.2);
    }

    ::placeholder {
      font-family: "Montserrat", sans-serif;
      font-weight: 400;
      color: #fff;
      text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.4);
    }

    .wifi-status {
      color: #fff;
      text-align: center;
    }

    .wifi-status > * {
      width: 215px;
      margin: 0 auto 16px auto;
      display: block;
    }

    .wifi-status svg {
      width: 70%;
      margin: 0 auto 40px auto;
    }

    .wifi-status h1 {
      font-size: 1.2rem;
      font-weight: bold;
      color: hsl(227 6% 41% / 1);
      text-align: left;
      line-height: 1.3;
    }

    .wifi-status h3 {
      margin-top: 0;
      font-size: 1rem;
      color: #d1d5db;
      font-weight: 600;
      text-align: left;
    }

    .wifi-status ul {
      padding: 0 0 0 20px;
      text-align: left;
      list-style-position: outside;
    }

    .wifi-status li {
      margin: 0 0 10px 0;
      color: #000000;
      line-height: 1.4;
      word-break: break-word;
    }

    .wifi-status span,
    .wifi-status p,
    .wifi-status i {
      text-align: left;
      color: hsl(227 6% 41% / 1);
      line-height: 1.45;
    }

    .loading-row {
      justify-content: center;
      align-items: center;
      width: 197px;
      margin: 18px auto 10px auto;
      min-height: 40px;
    }

    .spinner {
      width: 32px;
      height: 32px;
      border: 4px solid rgba(255, 255, 255, 0.25);
      border-top: 4px solid #ffffff;
      border-radius: 50%;
      animation: spin 1s linear infinite;
    }

    @keyframes spin {
      0% {
        transform: rotate(0deg);
      }
      100% {
        transform: rotate(360deg);
      }
    }

  )CSS");
}
