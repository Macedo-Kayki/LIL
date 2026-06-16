#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <math.h>

#include "secrets.h"

const int pinPolegar   = 34;
const int pinIndicador = 35;
const int pinMedio     = 32;
const int pinAnelar    = 33;
const int pinMindinho  = 36;

const int MPU_ADDR = 0x68;

int limiarMeio[5]    = { 1400, 1400, 1400, 1400, 1400 };
int limiarDobrado[5] = { 2600, 2600, 2600, 2600, 2600 };

const bool MODO_CALIBRACAO = false;

#define OR_QUALQUER 0
#define OR_CIMA     1
#define OR_BAIXO    2
#define OR_LADO     3

#define MV_QUALQUER -1
#define MV_PARADO    0
#define MV_MOVENDO   1

struct Letra {
  char nome;
  int  dedos[5];
  int  orient;
  int  mov;
};

Letra tabela[] = {
  {'A', {    0,  8,  8,  8,  8},  OR_QUALQUER, MV_QUALQUER},
  {'B', {    8,  0,  0,  0,  0},  OR_QUALQUER, MV_QUALQUER},
  {'F', {    8,  8,  0,  0,  0},  OR_QUALQUER, MV_QUALQUER},
  {'L', {    0,  0,  8,  8,  8},  OR_QUALQUER, MV_QUALQUER},
  {'W', {    8,  0,  0,  0,  8},  OR_QUALQUER, MV_QUALQUER},
  {'Y', {    0,  8,  8,  8,  0},  OR_QUALQUER, MV_QUALQUER},
  {'S', {    2,  2,  2,  2,  2},  OR_QUALQUER, MV_QUALQUER},

  {'C', {    1,  1,  1,  1,  1},  OR_QUALQUER, MV_QUALQUER},
  {'X', {    8,  1,  2,  2,  2},  OR_QUALQUER, MV_QUALQUER},

  {'D', {    8,  0,  8,  8,  8},  OR_CIMA,     MV_PARADO},
  {'G', {    8,  0,  8,  8,  8},  OR_LADO,     MV_PARADO},
  {'Q', {    8,  0,  8,  8,  8},  OR_BAIXO,    MV_PARADO},
  {'U', {    8,  0,  0,  8,  8},  OR_QUALQUER, MV_PARADO},
  {'P', {    8,  0,  0,  8,  8},  OR_BAIXO,    MV_PARADO},

  {'J', {    8,  8,  8,  8,  0},  OR_QUALQUER, MV_MOVENDO},
  {'Z', {    8,  0,  8,  8,  8},  OR_QUALQUER, MV_MOVENDO},
  {'H', {    8,  0,  0,  8,  8},  OR_QUALQUER, MV_MOVENDO},

  {'I', {    8,  8,  8,  8,  0},  OR_QUALQUER, MV_PARADO},
};

const int N_LETRAS = sizeof(tabela) / sizeof(tabela[0]);

WebServer server(80);

int   valDedos[5];
int   estadoDedos[5];
char  letraAtual = '?';
char  letraConfirmada = '?';
float roll = 0, pitch = 0;
int16_t AcX, AcY, AcZ;
int16_t GyX, GyY, GyZ;
float gyroMag = 0;
int   orientacao = OR_LADO;
bool  emMovimento = false;

const float LIMIAR_MOV   = 60.0;
const int   LIMIAR_CIMA  = 45;
const int   LIMIAR_BAIXO = -45;
const unsigned long JANELA_MOV = 500;

unsigned long ultimoMovimento = 0;

char  ultimaLeitura = '?';
unsigned long tempoLeitura = 0;
const unsigned long TEMPO_ESTAVEL = 250;

void lerDedos() {
  valDedos[0] = analogRead(pinPolegar);
  valDedos[1] = analogRead(pinIndicador);
  valDedos[2] = analogRead(pinMedio);
  valDedos[3] = analogRead(pinAnelar);
  valDedos[4] = analogRead(pinMindinho);

  for (int i = 0; i < 5; i++) {
    if (valDedos[i] >= limiarDobrado[i])   estadoDedos[i] = 2;
    else if (valDedos[i] >= limiarMeio[i]) estadoDedos[i] = 1;
    else                                   estadoDedos[i] = 0;
  }
}

void lerMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);
  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();
  Wire.read(); Wire.read();
  GyX = Wire.read() << 8 | Wire.read();
  GyY = Wire.read() << 8 | Wire.read();
  GyZ = Wire.read() << 8 | Wire.read();

  roll  = atan2((float)AcY, (float)AcZ) * 180.0 / PI;
  pitch = atan2(-(float)AcX, sqrt((float)AcY * AcY + (float)AcZ * AcZ)) * 180.0 / PI;

  if (pitch > LIMIAR_CIMA)        orientacao = OR_CIMA;
  else if (pitch < LIMIAR_BAIXO)  orientacao = OR_BAIXO;
  else                            orientacao = OR_LADO;

  float gx = GyX / 131.0, gy = GyY / 131.0, gz = GyZ / 131.0;
  gyroMag = sqrt(gx * gx + gy * gy + gz * gz);

  if (gyroMag > LIMIAR_MOV) ultimoMovimento = millis();
  emMovimento = (millis() - ultimoMovimento) < JANELA_MOV;
}

char reconhecerLetra() {
  int movAtual = emMovimento ? MV_MOVENDO : MV_PARADO;

  for (int l = 0; l < N_LETRAS; l++) {
    bool bate = true;
    for (int d = 0; d < 5; d++) {
      int code = tabela[l].dedos[d];
      int s    = estadoDedos[d];
      bool ok;
      if      (code == 9) ok = true;
      else if (code == 8) ok = (s >= 1);
      else                ok = (s == code);
      if (!ok) { bate = false; break; }
    }
    if (!bate) continue;

    if (tabela[l].orient != OR_QUALQUER && tabela[l].orient != orientacao) continue;

    if (tabela[l].mov != MV_QUALQUER && tabela[l].mov != movAtual) continue;

    return tabela[l].nome;
  }
  return '?';
}

void atualizarLetraConfirmada() {
  if (letraAtual != ultimaLeitura) {
    ultimaLeitura = letraAtual;
    tempoLeitura = millis();
  } else if (millis() - tempoLeitura > TEMPO_ESTAVEL) {
    letraConfirmada = letraAtual;
  }
}

char letraEnviada = ' ';
unsigned long ultimoEnvioFB = 0;
const unsigned long HEARTBEAT_FB = 2000;

WiFiClientSecure clientFB;
HTTPClient       httpsFB;
bool             fbIniciado = false;

void enviarFirebase() {
  if (WiFi.status() != WL_CONNECTED) return;

  long soma = 0;
  for (int i = 0; i < 5; i++) soma += valDedos[i];
  int flexPct = (int)((soma / 5) * 100 / 4095);
  int movPct  = (int)(gyroMag * 100.0 / 300.0);
  if (movPct > 100) movPct = 100; if (movPct < 0) movPct = 0;
  int incl    = (int)fabs(pitch);

  String body = String("{\"letra\":\"") + letraConfirmada + "\""
              + ",\"flex\":"   + flexPct
              + ",\"mov\":"    + movPct
              + ",\"incl\":"   + incl
              + ",\"t\":"      + String(millis())
              + ",\"online\":true}";

  if (!fbIniciado) {
    clientFB.setInsecure();
    httpsFB.setReuse(true);
    String url = String(FB_URL) + "/luva.json";
    if (strlen(FB_AUTH) > 0) url += "?auth=" + String(FB_AUTH);
    if (!httpsFB.begin(clientFB, url)) {
      Serial.println("Firebase: falha ao conectar (confira FB_URL)");
      return;
    }
    httpsFB.addHeader("Content-Type", "application/json");
    fbIniciado = true;
  }

  int code = httpsFB.PUT(body);
  Serial.printf("Firebase: '%c' flex:%d mov:%d -> HTTP %d\n",
                letraConfirmada, flexPct, movPct, code);
  if (code <= 0) {
    httpsFB.end();
    fbIniciado = false;
  }
}

void handleDados() {
  String json = "{";
  json += "\"letra\":\"" + String(letraConfirmada) + "\",";
  json += "\"bruta\":\"" + String(letraAtual) + "\",";
  json += "\"dedos\":[" + String(valDedos[0]) + "," + String(valDedos[1]) + ","
        + String(valDedos[2]) + "," + String(valDedos[3]) + "," + String(valDedos[4]) + "],";
  json += "\"estado\":[" + String(estadoDedos[0]) + "," + String(estadoDedos[1]) + ","
        + String(estadoDedos[2]) + "," + String(estadoDedos[3]) + "," + String(estadoDedos[4]) + "],";
  json += "\"roll\":" + String(roll, 1) + ",";
  json += "\"pitch\":" + String(pitch, 1) + ",";
  json += "\"orient\":" + String(orientacao) + ",";
  json += "\"mov\":" + String(emMovimento ? "true" : "false") + ",";
  json += "\"gyro\":" + String(gyroMag, 0);
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(21, 22);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Serial.print("Conectando no Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) {
    delay(500); Serial.print("."); tentativas++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(">> Conectado! Enviando ao Firebase. (debug local: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/dados)");
  } else {
    Serial.println(">> Falhou. Confira SSID/SENHA. (Rede precisa ser 2.4GHz)");
  }

  server.on("/dados", handleDados);
  server.begin();
  Serial.println("Servidor web iniciado.");
}

void loop() {
  lerDedos();
  lerMPU();

  if (MODO_CALIBRACAO) {
    Serial.printf("POL:%4d IND:%4d MED:%4d ANE:%4d MIN:%4d | estados:%d%d%d%d%d | pitch:%.0f gyro:%.0f %s\n",
                  valDedos[0], valDedos[1], valDedos[2], valDedos[3], valDedos[4],
                  estadoDedos[0], estadoDedos[1], estadoDedos[2], estadoDedos[3], estadoDedos[4],
                  pitch, gyroMag, emMovimento ? "MOVENDO" : "parado");
    delay(150);
  } else {
    letraAtual = reconhecerLetra();
    atualizarLetraConfirmada();

    if (letraConfirmada != letraEnviada || (millis() - ultimoEnvioFB > HEARTBEAT_FB)) {
      enviarFirebase();
      letraEnviada = letraConfirmada;
      ultimoEnvioFB = millis();
    }
  }

  server.handleClient();
}
