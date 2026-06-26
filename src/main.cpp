#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <math.h>

#include "secrets.h"

const int pinPolegar   = 32;
const int pinIndicador = 36;
const int pinMedio     = 33;
const int pinAnelar    = 34;
const int pinMindinho  = 35;

const int MPU_ADDR = 0x68;

// Filtro de média móvel para suavizar leituras
#define FILTRO_TAMANHO 8
int bufferFiltro[5][FILTRO_TAMANHO];
int indiceFiltro[5] = {0, 0, 0, 0, 0};

// Variáveis para receber a calibração dinâmica
int limiarMeio[5]    = {0, 0, 0, 0, 0};
int limiarDobrado[5] = {0, 0, 0, 0, 0};

const bool MODO_CALIBRACAO = true;
const bool USAR_MPU = true;

const bool IGNORAR_INDICADOR = false;
const bool IGNORAR_MEDIO     = true; // Dedo médio com defeito, ignorar leituras.

#define OR_QUALQUER 0
#define OR_CIMA     1
#define OR_BAIXO    2
#define OR_LADO     3

#define MV_QUALQUER -1
#define MV_PARADO    0
#define MV_MOVENDO   1

struct Letra {
  String nome;
  int  dedos[5];
  int  orient;
  int  mov;
};

Letra tabela[] = {
  {"A", {    0,  8,  8,  8,  8},  OR_QUALQUER, MV_QUALQUER},
  {"B", {    8,  0,  0,  0,  0},  OR_QUALQUER, MV_QUALQUER},
  {"L", {    0,  0,  8,  8,  8},  OR_QUALQUER, MV_QUALQUER},
  {"W", {    8,  0,  0,  0,  8},  OR_QUALQUER, MV_QUALQUER},
  {"C", {    1,  1,  1,  1,  1},  OR_QUALQUER, MV_QUALQUER},
  {"X", {    8,  1,  2,  2,  2},  OR_QUALQUER, MV_QUALQUER},
  {"O", {    2,  2,  2,  2,  2},  OR_QUALQUER, MV_QUALQUER},
  {"D", {    8,  0,  8,  8,  8},  OR_CIMA,     MV_PARADO},
  {"Q", {    8,  0,  8,  8,  8},  OR_BAIXO,    MV_PARADO},
  {"U", {    8,  0,  0,  8,  8},  OR_QUALQUER, MV_PARADO},
  {"P", {    8,  0,  0,  8,  8},  OR_BAIXO,    MV_PARADO},
  {"J", {    8,  8,  8,  8,  0},  OR_QUALQUER, MV_MOVENDO},
  {"H", {    8,  0,  0,  8,  8},  OR_QUALQUER, MV_MOVENDO},
  {"I", {    8,  8,  8,  8,  0},  OR_QUALQUER, MV_PARADO},
  {"te amo", {    0,  0,  0,  1,  0},  OR_QUALQUER, MV_PARADO}, 
  {"sim",      {    8,  8,  8,  8,  8},  OR_QUALQUER, MV_MOVENDO}, 
  {"como",     {    1,  1,  1,  8,  8},  OR_QUALQUER, MV_MOVENDO}, 
  {"onde",     {    8,  0,  8,  8,  8},  OR_LADO,     MV_PARADO},  
  {"pare",     {    0,  0,  0,  0,  0},  OR_QUALQUER, MV_PARADO},  
  {"ola",      {    0,  0,  0,  0,  0},  OR_QUALQUER, MV_MOVENDO}, 
  {"não",      {    8,  0,  0,  8,  8},  OR_LADO,     MV_MOVENDO}, 
  {"obrigado", {    0,  0,  0,  0,  0},  OR_CIMA,     MV_PARADO},  
};

const int N_LETRAS = sizeof(tabela) / sizeof(tabela[0]);

WebServer server(80);

int   valDedos[5];
int   estadoDedos[5];
String  letraAtual = "?";
String  letraConfirmada = "?";
float roll = 0, pitch = 0;
int16_t AcX, AcY, AcZ, GyX, GyY, GyZ;
float gyroMag = 0;
int   orientacao = OR_LADO;
bool  emMovimento = false;
bool  mpuOk = false;

const float LIMIAR_MOV   = 60.0;
const int   LIMIAR_CIMA  = 45;
const int   LIMIAR_BAIXO = -45;
const unsigned long JANELA_MOV = 500;
unsigned long ultimoMovimento = 0;

String  ultimaLeitura = "?";
unsigned long tempoLeitura = 0;
const unsigned long TEMPO_ESTAVEL = 250;

String letraEnviada = "";
unsigned long ultimoEnvioFB = 0;
const unsigned long HEARTBEAT_FB = 2000;
WiFiClientSecure clientFB;
HTTPClient       httpsFB;
bool             fbIniciado = false;

// Função isolada apenas para ler e alimentar o filtro de média móvel
void lerSensoresBruto() {
  int raw[5];
  raw[0] = analogRead(pinPolegar);
  raw[1] = analogRead(pinIndicador);
  raw[2] = analogRead(pinMedio);
  raw[3] = analogRead(pinAnelar);
  raw[4] = analogRead(pinMindinho);

  for (int i = 0; i < 5; i++) {
    bufferFiltro[i][indiceFiltro[i]] = raw[i];
    indiceFiltro[i] = (indiceFiltro[i] + 1) % FILTRO_TAMANHO;
    
    long soma = 0;
    for (int j = 0; j < FILTRO_TAMANHO; j++) {
      soma += bufferFiltro[i][j];
    }
    valDedos[i] = soma / FILTRO_TAMANHO;
  }
}

// Rotina dinâmica que roda apenas no setup
void calibrarSensores() {
  Serial.println("\n========================================");
  Serial.println("  INICIANDO CALIBRACAO DA LUVA SENSORIAL");
  Serial.println("========================================");
  
  // Passo 1: Mão Aberta
  Serial.println("\n[PASSO 1] ABRA A MAO COMPLETAMENTE e mantenha os dedos esticados!");
  Serial.println("Capturando em 5 segundos...");
  delay(1000); Serial.println("4..."); delay(1000); Serial.println("3...");
  delay(1000); Serial.println("2..."); delay(1000); Serial.println("1..."); delay(1000);
  
  int valAberto[5];
  // Roda algumas vezes para encher o buffer do filtro com os dados atuais
  for(int i = 0; i < 20; i++) { lerSensoresBruto(); delay(20); }
  for(int i = 0; i < 5; i++) valAberto[i] = valDedos[i];
  Serial.println("-> Valores de MAO ABERTA salvos!");

  // Passo 2: Mão Fechada
  Serial.println("\n[PASSO 2] FECHE A MAO COMPLETAMENTE (faca um punho forte)!");
  Serial.println("Capturando em 5 segundos...");
  delay(1000); Serial.println("4..."); delay(1000); Serial.println("3...");
  delay(1000); Serial.println("2..."); delay(1000); Serial.println("1..."); delay(1000);
  
  int valFechado[5];
  for(int i = 0; i < 20; i++) { lerSensoresBruto(); delay(20); }
  for(int i = 0; i < 5; i++) valFechado[i] = valDedos[i];
  Serial.println("-> Valores de MAO FECHADA salvos!\n");

  // Calcula os limites dinamicamente (Considerando que ao fechar a resistência diminui)
  for(int i = 0; i < 5; i++) {
    int diferenca = valAberto[i] - valFechado[i];
    
    // Se o dedo está ignorado ou o sensor travado, previne divisão por zero ou bugs
    if (diferenca < 10 && diferenca > -10) diferenca = 50; 
    
    // Limiar Meio: Caiu 35% a partir do estado aberto
    limiarMeio[i] = valAberto[i] - (diferenca * 0.35);
    // Limiar Dobrado: Caiu 75% a partir do estado aberto
    limiarDobrado[i] = valAberto[i] - (diferenca * 0.75);
    
    Serial.printf("Dedo %d -> Ab: %d | Meio: %d | Fchd: %d | Lim: %d\n", 
                  i, valAberto[i], limiarMeio[i], limiarDobrado[i], valFechado[i]);
  }
  Serial.println("========================================");
  Serial.println("   CALIBRACAO CONCLUIDA! PODE USAR!     ");
  Serial.println("========================================\n");
}

void lerDedos() {
  lerSensoresBruto(); // Puxa os valores limpos do filtro

  for (int i = 0; i < 5; i++) {
    // Usa as variáveis que foram preenchidas no setup()
    if (valDedos[i] <= limiarDobrado[i])   estadoDedos[i] = 2; // Fechado
    else if (valDedos[i] <= limiarMeio[i]) estadoDedos[i] = 1; // Curvado
    else                                   estadoDedos[i] = 0; // Aberto
  }
}

void lerMPU() {
  if (!USAR_MPU) {
    orientacao = OR_LADO;
    emMovimento = false;
    gyroMag = 0;
    pitch = 0;
    return;
  }
  
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

String reconhecerLetra() {
  int movAtual = emMovimento ? MV_MOVENDO : MV_PARADO;

  for (int l = 0; l < N_LETRAS; l++) {
    bool bate = true;
    for (int d = 0; d < 5; d++) {
      if (IGNORAR_INDICADOR && d == 1) continue;
      if (IGNORAR_MEDIO && d == 2) continue;
      int code = tabela[l].dedos[d];
      int s    = estadoDedos[d];
      bool ok;
      if      (code == 9) ok = true;
      else if (code == 8) ok = (s >= 1);
      else                ok = (s == code);
      if (!ok) { bate = false; break; }
    }
    if (!bate) continue;

    if (USAR_MPU) {
      if (tabela[l].orient != OR_QUALQUER && tabela[l].orient != orientacao) continue;
      if (tabela[l].mov != MV_QUALQUER && tabela[l].mov != movAtual) continue;
    }

    return tabela[l].nome;
  }
  return "?";
}

void atualizarLetraConfirmada() {
  if (letraAtual != ultimaLeitura) {
    ultimaLeitura = letraAtual;
    tempoLeitura = millis();
  } else if (millis() - tempoLeitura > TEMPO_ESTAVEL) {
    letraConfirmada = letraAtual;
  }
}

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
  Serial.printf("Firebase: '%s' flex:%d mov:%d -> HTTP %d\n",
                letraConfirmada.c_str(), flexPct, movPct, code);
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

  // Inicializa o ADC antes de ler qualquer coisa
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Chama a nossa nova rotina de calibração dinâmica!
  calibrarSensores();

  if (USAR_MPU) {
    pinMode(21, INPUT_PULLUP);
    pinMode(22, INPUT_PULLUP);
    Wire.begin(21, 22);
    Wire.setClock(100000);

    Serial.println("Procurando dispositivos I2C...");
    byte achados = 0;
    for (byte addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.printf("  >> dispositivo I2C em 0x%02X\n", addr);
        achados++;
      }
    }
    if (achados == 0)
      Serial.println("  Nenhum dispositivo I2C achado (SDA=21? SCL=22? VCC? GND?).");

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);
    Wire.write(0);
    byte erroMPU = Wire.endTransmission(true);
    mpuOk = (erroMPU == 0);
    if (mpuOk) Serial.println("MPU6050 OK");
    else       Serial.printf("MPU6050 NAO respondeu em 0x%02X. Veja o endereco achado acima.\n", MPU_ADDR);
  } else {
    Serial.println("MPU6050 desabilitado - funcionando apenas com sensores de flexão.");
    mpuOk = false;
  }

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