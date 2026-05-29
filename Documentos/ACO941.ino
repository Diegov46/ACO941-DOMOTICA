#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <EEPROM.h>

// ---------- WIFI / MQTT ----------
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.emqx.io";

WiFiClient espClient;
PubSubClient client(espClient);

const char* topic_pub_datos = "udb/grupo5/fase2/sensores";
const char* topic_pub_eventos = "udb/grupo5/fase2/eventos";
const char* topic_sub_control = "udb/grupo5/fase2/control";

// ---------- PINES ----------
#define DHTPIN 2
#define DHTTYPE DHT22
#define LDR_PIN 34
#define PIR_PIN 13
#define GAS_PIN 35

#define RELAY_PIN 26
#define LED_LUZ_PIN 14
#define LED_ALARMA_PIN 16
#define BUZZER_PIN 18
#define SERVO_PIN 25

#define ENCODER_CLK 32
#define ENCODER_DT 33

// ---------- OBJETOS ----------
DHT dht(DHTPIN, DHTTYPE);
Servo miServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- TECLADO 4x4 ----------
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {4, 5, 19, 23};
byte colPins[COLS] = {27, 15, 12, 17};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ---------- UMBRALES ----------
int UMBRAL_GAS = 3800;
int UMBRAL_LUZ = 1500;
float TEMP_ON = 30.0;
float TEMP_OFF = 25.0;

// ---------- ESTADOS ----------
bool alertaGas = false;
bool estadoVentilador = false;
bool estadoLuz = false;
bool puertaAbierta = false;
bool overrideManual = false;

float temperatura = 0;
float humedad = 0;
int gas = 0;
int luz = 0;
int movimiento = LOW;
bool dhtValido = false;

// ---------- TEMPORIZADORES ----------
unsigned long tiempoLCD = 0;
unsigned long tiempoTelemetria = 0;
unsigned long tiempoSensores = 0;
unsigned long tiempoMovimientoLuz = 0;
unsigned long tiempoMovimientoPuerta = 0;
unsigned long tiempoOverride = 0;
unsigned long tiempoMQTTReconnect = 0;

const unsigned long INTERVALO_LCD = 2000;
const unsigned long INTERVALO_TELEMETRIA = 5000;
const unsigned long INTERVALO_SENSORES = 1000;
const unsigned long RETARDO_LUZ = 30000;
const unsigned long RETARDO_PUERTA = 5000;
const unsigned long DURACION_OVERRIDE = 15000;
const unsigned long INTERVALO_RECONNECT = 5000;

// ---------- ENCODER ----------
int encoderPos = 0;
int lastEncoderCLK = HIGH;

// ---------- EEPROM ----------
#define EEPROM_SIZE 16
#define ADDR_GAS 0
#define ADDR_LUZ 4

void guardarUmbrales() {
  EEPROM.writeInt(ADDR_GAS, UMBRAL_GAS);
  EEPROM.writeInt(ADDR_LUZ, UMBRAL_LUZ);
  EEPROM.commit();
}

void cargarUmbrales() {
  EEPROM.begin(EEPROM_SIZE);

  int gasEEPROM = EEPROM.readInt(ADDR_GAS);
  int luzEEPROM = EEPROM.readInt(ADDR_LUZ);

  if (gasEEPROM >= 1000 && gasEEPROM <= 4095) {
    UMBRAL_GAS = gasEEPROM;
  }

  if (luzEEPROM >= 0 && luzEEPROM <= 4095) {
    UMBRAL_LUZ = luzEEPROM;
  }
}

void publicarEvento(String evento) {
  Serial.println("EVENTO: " + evento);
  if (client.connected()) {
    client.publish(topic_pub_eventos, evento.c_str());
  }
}

void setup_wifi() {
  lcd.clear();
  lcd.print("Conectando WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  lcd.clear();
  lcd.print("WiFi OK");
  Serial.println("\nWiFi conectado");
}

void aplicarFailSafeGas() {
  alertaGas = true;
  estadoVentilador = false;
  estadoLuz = false;

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_LUZ_PIN, LOW);
  digitalWrite(LED_ALARMA_PIN, HIGH);
  tone(BUZZER_PIN, 1000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PELIGRO GAS");
  lcd.setCursor(0, 1);
  lcd.print("EVACUAR");

  publicarEvento("ALERTA_GAS_ACTIVADA");
}

void resetAlertaGas() {
  if (gas <= UMBRAL_GAS) {
    alertaGas = false;
    noTone(BUZZER_PIN);
    digitalWrite(LED_ALARMA_PIN, LOW);

    lcd.clear();
    lcd.print("GAS OK");
    lcd.setCursor(0, 1);
    lcd.print("Sistema seguro");

    publicarEvento("ALERTA_GAS_RESTABLECIDA");
  } else {
    publicarEvento("RESET_RECHAZADO_GAS_ALTO");
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";

  for (int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }

  Serial.print("MQTT recibido: ");
  Serial.println(mensaje);

  if (mensaje == "RESET") {
    resetAlertaGas();
    return;
  }

  if (alertaGas) {
    publicarEvento("COMANDO_BLOQUEADO_POR_ALERTA_GAS");
    return;
  }

  overrideManual = true;
  tiempoOverride = millis();

  if (mensaje == "RELAY_ON") {
    estadoVentilador = true;
    digitalWrite(RELAY_PIN, HIGH);
  } else if (mensaje == "RELAY_OFF") {
    estadoVentilador = false;
    digitalWrite(RELAY_PIN, LOW);
  } else if (mensaje == "LED_ON") {
    estadoLuz = true;
    digitalWrite(LED_LUZ_PIN, HIGH);
  } else if (mensaje == "LED_OFF") {
    estadoLuz = false;
    digitalWrite(LED_LUZ_PIN, LOW);
  } else if (mensaje == "SERVO_OPEN") {
    miServo.write(90);
    puertaAbierta = true;
  } else if (mensaje == "SERVO_CLOSE") {
    miServo.write(0);
    puertaAbierta = false;
  }

  publicarEvento("COMANDO_MANUAL_EJECUTADO");
}

void reconnectMQTT() {
  if (client.connected()) return;

  unsigned long now = millis();

  if (now - tiempoMQTTReconnect >= INTERVALO_RECONNECT) {
    tiempoMQTTReconnect = now;

    Serial.print("Intentando MQTT...");

    String clientId = "ESP32_Grupo5_";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println(" conectado");
      client.subscribe(topic_sub_control);
      publicarEvento("ESP32_CONECTADO_MQTT");
    } else {
      Serial.print(" fallo rc=");
      Serial.println(client.state());
    }
  }
}

void leerSensores() {
  temperatura = dht.readTemperature();
  humedad = dht.readHumidity();

  dhtValido = !(isnan(temperatura) || isnan(humedad));

  gas = analogRead(GAS_PIN);
  luz = analogRead(LDR_PIN);
  movimiento = digitalRead(PIR_PIN);

  Serial.print("Gas: ");
  Serial.print(gas);
  Serial.print(" | Luz: ");
  Serial.print(luz);
  Serial.print(" | PIR: ");
  Serial.print(movimiento);
  Serial.print(" | Temp: ");
  Serial.print(temperatura);
  Serial.print(" | Hum: ");
  Serial.println(humedad);
}

void gestionarGas() {
  if (gas > UMBRAL_GAS && !alertaGas) {
    aplicarFailSafeGas();
  }

  if (alertaGas) {
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_LUZ_PIN, LOW);
    digitalWrite(LED_ALARMA_PIN, HIGH);
    tone(BUZZER_PIN, 1000);

    if (gas <= UMBRAL_GAS) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("GAS OK");
      lcd.setCursor(0, 1);
      lcd.print("Esperando RESET");
    }
  }
}

void gestionarTemperatura() {
  if (alertaGas || overrideManual) return;

  if (!dhtValido) {
    publicarEvento("ERROR_DHT22");
    return;
  }

  if (temperatura > TEMP_ON && !estadoVentilador) {
    estadoVentilador = true;
    digitalWrite(RELAY_PIN, HIGH);
    publicarEvento("Ventilador ON automatico");
  } else if (temperatura < TEMP_OFF && estadoVentilador) {
    estadoVentilador = false;
    digitalWrite(RELAY_PIN, LOW);
    publicarEvento("Ventilador OFF automatico");
  }
}

void gestionarIluminacion() {
  if (alertaGas || overrideManual) return;

  unsigned long now = millis();

  if (movimiento == HIGH) {
    tiempoMovimientoLuz = now;

    if (luz < UMBRAL_LUZ && !estadoLuz) {
      estadoLuz = true;
      digitalWrite(LED_LUZ_PIN, HIGH);
      publicarEvento("LED ON por baja luz y movimiento");
    } else if (luz >= UMBRAL_LUZ && estadoLuz) {
      estadoLuz = false;
      digitalWrite(LED_LUZ_PIN, LOW);
      publicarEvento("LED OFF por luz natural");
    }
  } else {
    if (estadoLuz && now - tiempoMovimientoLuz > RETARDO_LUZ) {
      estadoLuz = false;
      digitalWrite(LED_LUZ_PIN, LOW);
      publicarEvento("LED OFF por ausencia 30s");
    }
  }
}

void gestionarPuerta() {
  if (alertaGas || overrideManual) return;

  unsigned long now = millis();

  if (movimiento == HIGH) {
    tiempoMovimientoPuerta = now;

    if (!puertaAbierta) {
      miServo.write(90);
      puertaAbierta = true;
      publicarEvento("Puerta abierta por movimiento");
    }
  } else {
    if (puertaAbierta && now - tiempoMovimientoPuerta > RETARDO_PUERTA) {
      miServo.write(0);
      puertaAbierta = false;
      publicarEvento("Puerta cerrada despues de 5s");
    }
  }
}

void leerTeclado() {
  char key = keypad.getKey();

  if (!key) return;

  Serial.print("Tecla: ");
  Serial.println(key);

  if (key == '#') {
    resetAlertaGas();
    return;
  }

  if (alertaGas) {
    publicarEvento("TECLA_BLOQUEADA_ALERTA_GAS");
    return;
  }

  overrideManual = true;
  tiempoOverride = millis();

  if (key == '1') {
    estadoVentilador = !estadoVentilador;
    digitalWrite(RELAY_PIN, estadoVentilador ? HIGH : LOW);
    publicarEvento("Ventilador alternado por teclado");
  } else if (key == '2') {
    miServo.write(90);
    puertaAbierta = true;
    publicarEvento("Puerta abierta por teclado");
  } else if (key == '3') {
    miServo.write(0);
    puertaAbierta = false;
    publicarEvento("Puerta cerrada por teclado");
  } else if (key == '4') {
    estadoLuz = !estadoLuz;
    digitalWrite(LED_LUZ_PIN, estadoLuz ? HIGH : LOW);
    publicarEvento("LED alternado por teclado");
  } else if (key == 'A') {
    UMBRAL_GAS += 100;
    if (UMBRAL_GAS > 4095) UMBRAL_GAS = 4095;
    guardarUmbrales();
    publicarEvento("Umbral gas aumentado");
  } else if (key == 'B') {
    UMBRAL_GAS -= 100;
    if (UMBRAL_GAS < 1000) UMBRAL_GAS = 1000;
    guardarUmbrales();
    publicarEvento("Umbral gas reducido");
  }
}

void leerEncoder() {
  int currentCLK = digitalRead(ENCODER_CLK);

  if (currentCLK != lastEncoderCLK && currentCLK == HIGH) {
    if (digitalRead(ENCODER_DT) != currentCLK) {
      encoderPos++;
      UMBRAL_GAS += 50;
    } else {
      encoderPos--;
      UMBRAL_GAS -= 50;
    }

    if (UMBRAL_GAS < 1000) UMBRAL_GAS = 1000;
    if (UMBRAL_GAS > 4095) UMBRAL_GAS = 4095;

    guardarUmbrales();

    Serial.print("Encoder Pos: ");
    Serial.print(encoderPos);
    Serial.print(" | Umbral gas: ");
    Serial.println(UMBRAL_GAS);
  }

  lastEncoderCLK = currentCLK;
}

void actualizarLCD() {
  if (alertaGas) return;

  lcd.clear();
  lcd.setCursor(0, 0);

  if (dhtValido) {
    lcd.print("T:");
    lcd.print(temperatura, 1);
    lcd.print(" H:");
    lcd.print(humedad, 0);
    lcd.print("%");
  } else {
    lcd.print("Error DHT22");
  }

  lcd.setCursor(0, 1);
  lcd.print("G:");
  lcd.print(gas);
  lcd.print(" L:");
  lcd.print(luz);
}

void enviarTelemetria() {
  String jsonPayload = "{";
  jsonPayload += "\"temp\":" + String(temperatura, 1) + ",";
  jsonPayload += "\"hum\":" + String(humedad, 1) + ",";
  jsonPayload += "\"gas\":" + String(gas) + ",";
  jsonPayload += "\"luz\":" + String(luz) + ",";
  jsonPayload += "\"mov\":" + String(movimiento) + ",";
  jsonPayload += "\"alertaGas\":" + String(alertaGas ? 1 : 0) + ",";
  jsonPayload += "\"ventilador\":" + String(estadoVentilador ? 1 : 0) + ",";
  jsonPayload += "\"ledLuz\":" + String(estadoLuz ? 1 : 0) + ",";
  jsonPayload += "\"puerta\":" + String(puertaAbierta ? 1 : 0) + ",";
  jsonPayload += "\"umbralGas\":" + String(UMBRAL_GAS);
  jsonPayload += "}";

  Serial.println(jsonPayload);

  if (client.connected()) {
    client.publish(topic_pub_datos, jsonPayload.c_str());
  }
}

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.print("Iniciando...");

  cargarUmbrales();

  dht.begin();

  pinMode(LDR_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(GAS_PIN, INPUT);
  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_LUZ_PIN, OUTPUT);
  pinMode(LED_ALARMA_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  miServo.attach(SERVO_PIN);
  miServo.write(0);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_LUZ_PIN, LOW);
  digitalWrite(LED_ALARMA_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  lcd.clear();
  lcd.print("Sistema Listo");

  publicarEvento("SISTEMA_INICIADO");
}

void loop() {
  unsigned long now = millis();

  reconnectMQTT();
  client.loop();

  leerTeclado();
  leerEncoder();

  if (overrideManual && now - tiempoOverride > DURACION_OVERRIDE) {
    overrideManual = false;
    publicarEvento("Control automatico restaurado");
  }

  if (now - tiempoSensores >= INTERVALO_SENSORES) {
    tiempoSensores = now;

    leerSensores();
    gestionarGas();

    if (!alertaGas) {
      gestionarTemperatura();
      gestionarIluminacion();
      gestionarPuerta();
    }
  }

  if (now - tiempoLCD >= INTERVALO_LCD) {
    tiempoLCD = now;
    actualizarLCD();
  }

  if (now - tiempoTelemetria >= INTERVALO_TELEMETRIA) {
    tiempoTelemetria = now;
    enviarTelemetria();
  }

  delay(100);
}