/**
 * Module: sknSensors-Rcwl_Dht11.ino
 *  Data:  data/homie/config.json, 
 *         data/homie/ui_bundle.gz
 *  Date:  02/10/2019
 *  
 *  Description:
 *  At sensorsInterval (5 min) send Temperature, Humidity.  Send Motion if off.
 *  At Motion transistion to HIGH, send Motion on.
 *  
 * Customization:
 *  1. Change/Update NODE_* defines to indicate the intended room placement            -- OPTIONAL
 *  2. Change/Update device_id: in config.json to indicate the intended room placement -- SUGGESTED
 *  3. Ensure senors are are wired properly or change DHT_PIN and RCWL_PIN             -- REQUIRED
 */
#include <Homie.h>
#include <DHT_U.h>

#ifdef ESP8266
extern "C" {
#include "user_interface.h" // to set CPU Freq
}
#endif

#define NODE_MOTION_NAME      "Humans Present"
#define NODE_TEMPERATURE_NAME "Room Temperature"
#define NODE_HUMIDITY_NAME    "Room Humidity"
#define NODE_SENSOR_INTERVAL  60

#define FW_NAME "sknSensors-Rcwl_Dht11"
#define FW_VERSION  "0.2.4"

#define DHTTYPE DHT11
#define DHT_PIN   D6    // GPIO12
#define RCWL_PIN  D5    // GPIO14

String            gsMotionString      = "false";
volatile float    gfTemperature       = 0.0f, 
                  gfHumidity          = 0.0f;
volatile byte     gbMotion            = LOW,
                  gbLastMotion        = LOW;  
volatile uint32_t gulLastSensorsSend  = 0;

sensors_event_t event;

DHT_Unified dht(DHT_PIN, DHTTYPE);

// HomieNode(const char* id, const char* name, const char* type, bool range = false, uint16_t lower = 0, uint16_t upper = 0, const HomieInternals::NodeInputHandler& nodeInputHandler = [](const HomieRange& range, const String& property, const String& value) { return false; });
HomieNode rcwlNode("motion", NODE_MOTION_NAME, "motion");
HomieNode temperatureNode("temperature",  NODE_TEMPERATURE_NAME, "temperature");
HomieNode humidityNode("humidity", NODE_HUMIDITY_NAME, "humidity");

HomieSetting<long> sensorsIntervalSetting("sensorsInterval", "The interval in seconds used for sensor readings.");

bool broadcastHandler(const String& level, const String& value) {
  Homie.getLogger() << "Received broadcast level " << level << ": " << value << endl;
  return true;
}

void setupHandler() {    
  pinMode(RCWL_PIN, INPUT);  
  dht.begin();
  yield();  
}

void loopHandler() {
  gbMotion = digitalRead(RCWL_PIN); // true for 2 seconds per presence trigger

  if (gbLastMotion == LOW && gbMotion == HIGH) { 
    gsMotionString = "true";
    gbLastMotion = gbMotion;   //toggle to hold
    rcwlNode.setProperty("motion").send(gsMotionString);         
    Homie.getLogger() << "Humans.Present?: " << gsMotionString << endl;
  }
    
  if (millis() - gulLastSensorsSend >= (sensorsIntervalSetting.get() * 1000UL) || gulLastSensorsSend == 0) {
    dht.temperature().getEvent(&event);
    if( !isnan(event.temperature) ){
     gfTemperature = ((event.temperature * 1.8) + 32.0);
     Homie.getLogger() << "Temperature: " << gfTemperature << " °F" << endl;
     temperatureNode.setProperty("degrees").send(String(gfTemperature));
    
     dht.humidity().getEvent(&event);
     if( !isnan(event.relative_humidity) ){
       gfHumidity = event.relative_humidity;
       Homie.getLogger() << "Humidity: " << gfHumidity << " %" << endl;
       humidityNode.setProperty("percent").send(String(gfHumidity));
     }     
    }
    
    if (gbLastMotion == HIGH && gbMotion == LOW) {
      gbLastMotion = gbMotion;                             // release hold
      gsMotionString = "false";
      rcwlNode.setProperty("motion").send(gsMotionString);  
    }
    Homie.getLogger() << "Humans?: " << gsMotionString << endl;
    
    gulLastSensorsSend = millis();  // required to be inside decision block
  } // if time to send
}


void setup() {
  delay(50);
  system_update_cpu_freq(SYS_CPU_160MHZ);
  yield();
  
  Serial.begin(115200);
  Serial << endl << endl;
  yield();
  
  Homie_setFirmware(FW_NAME, FW_VERSION);
  Homie_setBrand("sknSensors");
  
  Homie.setSetupFunction(setupHandler)
       .setLoopFunction(loopHandler)
       .setBroadcastHandler(broadcastHandler);
                                    
  sensorsIntervalSetting.setDefaultValue(NODE_SENSOR_INTERVAL)       // more than zero & less than one hour
                        .setValidator([] (long lValue) { return (lValue > 0) && (lValue <= 3600);});
  
  rcwlNode.advertise("motion").setName("Motion")
                              .setDatatype("boolean")
                              .setUnit("true,false");

  temperatureNode.advertise("degrees").setName("Degrees")
                                     .setDatatype("float")
                                     .setUnit("ºF");

  humidityNode.advertise("percent").setName("Percent")
                                  .setDatatype("float")
                                  .setUnit("%");  
  Homie.setup();
}

void loop() {
   // put your main code here, to run repeatedly:
   Homie.loop();
}
