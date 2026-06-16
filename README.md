<div align="center">

# 🧤 SignGlove — Firmware ESP32

Luva tradutora de Libras: lê os sensores da mão, reconhece a **letra** e envia
para o **Firebase Realtime Database**, de onde a [interface web SignGlove](./signglove/)
exibe em tempo real.

<img src="https://img.shields.io/badge/ESP32-Arduino-22C55E?style=for-the-badge&labelColor=0F172A" />
<img src="https://img.shields.io/badge/PlatformIO-FF7F00?style=for-the-badge&labelColor=0F172A" />
<img src="https://img.shields.io/badge/Firebase-RTDB-FFCA28?style=for-the-badge&labelColor=0F172A" />

</div>

---

## 📦 O que este firmware faz

1. Lê **5 sensores de flexão** (um por dedo) e o **MPU6050** (acelerômetro + giroscópio).
2. Reconhece a letra combinando **pose dos dedos + orientação + movimento**.
3. Envia a letra (e os sensores) para o Firebase via **HTTPS**.
4. Sobe um endpoint local opcional `http://<ip>/dados` (JSON) para debug.

---

## 🔧 Hardware necessário

| Item | Qtd | Observação |
| :--- | :-: | :--- |
| ESP32 DevKit (esp32doit-devkit-v1) | 1 | |
| Sensor de flexão | 5 | um por dedo |
| Resistor 10kΩ | 5 | divisor de tensão de cada flex |
| MPU6050 | 1 | acelerômetro + giroscópio (I2C) |
| Protoboard + jumpers | — | |
| Bateria LiPo 3.7V | 1 | ver nota de alimentação |

---

## 🔌 Ligações

### Sensores de flexão (divisor de tensão)

Cada sensor precisa de um resistor de **10kΩ**:

```txt
3.3V ──[ sensor flex ]──┬──[ 10kΩ ]── GND
                        │
                        └──► pino ADC do ESP32
```

| Dedo      | GPIO | ADC  |
| :-------- | :--: | :--: |
| Polegar   |  34  | ADC1 |
| Indicador |  35  | ADC1 |
| Médio     |  32  | ADC1 |
| Anelar    |  33  | ADC1 |
| Mindinho  |  36  | ADC1 |

> ⚠️ Use **somente pinos do ADC1** (32, 33, 34, 35, 36, 39). O **ADC2** (GPIO 25,
> 26, 27...) **não funciona com o Wi-Fi ligado**.

### MPU6050 (I2C)

| MPU  | ESP32   |
| :--- | :------ |
| VCC  | 3.3V    |
| GND  | GND     |
| SDA  | GPIO 21 |
| SCL  | GPIO 22 |

### Alimentação (bateria 3.7V)

> ❗ **Não** ligue os 3.7V direto no pino `3V3`. Use um carregador/regulador
> (ex.: TP4056 + step-up para 5V no pino `VIN/5V`), ou mantenha no **cabo USB**
> durante os testes e só passe para a bateria no final.

### Montagem: protoboard ou luva?

Faça **nesta ordem**: monte e teste tudo na **protoboard** primeiro (com o ESP32 no
USB); valide cada sensor no modo calibração; depois fixe **só os sensores de flexão**
na luva (um por dedo) e leve os fios até a protoboard. Bateria só no final.

---

## ⚙️ Configuração

### 1. Segredos (Wi-Fi + Firebase)

As credenciais ficam em `include/secrets.h`, que **não vai para o Git**. Na primeira vez:

```bash
cp include/secrets.example.h include/secrets.h
```

E preencha:

```cpp
const char* WIFI_SSID  = "SUA_REDE_2.4GHz";
const char* WIFI_SENHA = "SUA_SENHA";
const char* FB_URL     = "https://SEU-PROJETO-default-rtdb.firebaseio.com";
const char* FB_AUTH    = ""; // vazio = regras públicas (demo)
```

> O Wi-Fi precisa ser **2.4GHz** (o ESP32 não enxerga redes 5GHz).

### 2. Firebase

Crie um **Realtime Database** em modo de teste e use a mesma `FB_URL` aqui e no
site. Regras (demo):

```json
{ "rules": { ".read": true, ".write": true } }
```

---

## ▶️ Compilar e gravar (PlatformIO)

```bash
pio run                  # compila
pio run --target upload  # grava no ESP32 (conectado via USB)
pio device monitor       # abre o Monitor Serial (115200)
```

No Serial, ao conectar, você verá:

```txt
>> Conectado! Enviando ao Firebase. (debug local: http://192.168.x.x/dados)
Firebase: 'A' flex:63 mov:12 -> HTTP 200
```

**HTTP 200** = gravou no banco com sucesso.

---

## 🎯 Calibração (faça antes de reconhecer letras)

1. Em [`src/main.cpp`](src/main.cpp), coloque `MODO_CALIBRACAO = true` e grave.
2. Abra o Monitor Serial. Ele mostra o valor bruto + o estado de cada dedo
   (`0` esticado / `1` curvado / `2` fechado), além de `pitch` e `gyro`.
3. Com a mão **aberta**, **meio-dobrada** e **fechada**, ajuste os dois limiares por dedo:
   * `limiarMeio[5]` → a partir de quando vira **curvado** (`1`)
   * `limiarDobrado[5]` → a partir de quando vira **fechado** (`2`)
4. Ajuste também `LIMIAR_CIMA/BAIXO` (orientação) e `LIMIAR_MOV` (movimento).
5. Volte `MODO_CALIBRACAO = false` e grave de novo.

> 📋 Passo a passo detalhado, com tabelas pra preencher e checklist de cada letra:
> **[CALIBRACAO.md](CALIBRACAO.md)**.

---

## 🔤 Letras reconhecidas (18)

O reconhecimento combina quatro fontes:

* **Pose dos dedos** (5 flex, binário): `A B F L W Y S`
* **Dedo curvado** (meia dobra, 3º estado): `C` (todos curvados) e `X` (gancho do indicador)
* **Pose + orientação** (acelerômetro): `D` (cima) · `G` (frente) · `Q` (baixo) · `U` / `P` (baixo)
* **Pose + movimento** (giroscópio): `I` parado / `J` movendo · `D`-pose `Z` movendo · `U`-pose `H` movendo

> `C` e `X` dependem do **limiar do curvado** (`limiarMeio[]`) bem calibrado. Se confundirem, ajuste-o.

Letras de fora pelo limite do hardware: `V`/`R` (mesma pose do `U` — sensor não vê dedos
juntos/separados/cruzados), `E`/`M`/`N`/`O`/`T` (viram punho/curva = `S`/`C`), e `K`
(depende da trajetória do movimento). A tabela completa e editável está em `src/main.cpp`.

---

## 📡 Dados enviados ao Firebase (`/luva`)

```json
{
  "letra": "A",     // letra reconhecida (ou "?")
  "flex": 63,       // flexão média dos dedos (%)
  "mov": 12,        // intensidade do movimento (%)
  "incl": 18,       // inclinação da mão (graus)
  "t": 84213,       // batimento (heartbeat)
  "online": true
}
```

A escrita acontece **quando a letra muda** ou a cada `HEARTBEAT_FB` (2s).

---

## 🗂️ Estrutura

```txt
LIL/
├── platformio.ini
├── include/
│   ├── secrets.h          # seus segredos (NÃO versionado)
│   └── secrets.example.h  # modelo (versionado)
├── src/
│   └── main.cpp           # firmware
├── signglove/             # interface web (ver README de lá)
└── README.md
```

---

<div align="center">
  <strong>SignGlove © 2026</strong> — Projeto IoT de acessibilidade
</div>
