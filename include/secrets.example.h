// =====================================================================
//  MODELO de secrets.h  (este pode ir pro Git, NAO tem segredos)
//  Como usar: copie este arquivo para "secrets.h" e preencha seus dados.
//      cp include/secrets.example.h include/secrets.h
// =====================================================================
#ifndef SECRETS_H
#define SECRETS_H

// --- Wi-Fi (rede 2.4GHz) ---
const char* WIFI_SSID  = "NOME_DA_SUA_REDE";
const char* WIFI_SENHA = "SENHA_DA_SUA_REDE";

// --- Firebase Realtime Database (URL sem barra no final) ---
const char* FB_URL  = "https://SEU-PROJETO-default-rtdb.firebaseio.com";
const char* FB_AUTH = ""; // vazio = regras publicas (demo); ou o Database secret

#endif
