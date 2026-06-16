# 🎯 Guia de Calibração da Luva

Roteiro prático pra ajustar os limiares com a luva na mão. Faça **com calma** —
é o que separa "reconhece tudo" de "erra direto".

---

## 0. Ligar o modo calibração

1. Em [`src/main.cpp`](src/main.cpp), coloque:
   ```cpp
   const bool MODO_CALIBRACAO = true;
   ```
2. Grave (`pio run --target upload`) e abra o Monitor Serial (`pio device monitor`, 115200).
3. Você vai ver uma linha rolando assim:
   ```txt
   POL:1203 IND:3110 MED: 980 ANE:2750 MIN:1500 | estados:02021 | pitch:12 gyro:8 parado
   ```
   - Os 5 primeiros = valor **bruto** de cada dedo (0–4095).
   - `estados` = como o código está interpretando agora: **0** esticado · **1** curvado · **2** fechado.
   - `pitch` = inclinação · `gyro` = movimento.

> 💡 A meta da calibração é fazer o `estados` mostrar o número certo pra cada posição do dedo.

---

## 1. Limiares dos dedos (o mais importante)

Para **cada dedo**, faça 3 posições e anote o valor bruto que aparece no Serial:

| Dedo      | Esticado (aberto) | Curvado (meio) | Fechado (total) |
| :-------- | :---------------: | :------------: | :-------------: |
| Polegar   |                   |                |                 |
| Indicador |                   |                |                 |
| Médio     |                   |                |                 |
| Anelar    |                   |                |                 |
| Mindinho  |                   |                |                 |

Agora calcule os **dois limiares** de cada dedo (ponto do meio entre as posições):

```txt
limiarMeio    = (Esticado + Curvado) / 2     → vira "curvado" a partir daqui
limiarDobrado = (Curvado + Fechado) / 2      → vira "fechado" a partir daqui
```

**Exemplo:** se o indicador lê `300` esticado, `1900` curvado e `3400` fechado:
- `limiarMeio = (300 + 1900) / 2 = 1100`
- `limiarDobrado = (1900 + 3400) / 2 = 2650`

Coloque os 5 valores de cada array em [`src/main.cpp`](src/main.cpp):
```cpp
int limiarMeio[5]    = { __ , __ , __ , __ , __ };  // {pol, ind, med, ane, min}
int limiarDobrado[5] = { __ , __ , __ , __ , __ };
```

> ✅ **Confira:** com os limiares ajustados, abra a mão → `estados` deve dar `00000`;
> curve todos → `11111`; feche o punho → `22222`. Se não der, ajuste o limiar daquele dedo.

---

## 2. Limiares de orientação (acelerômetro)

Olhe o `pitch` no Serial enquanto vira a mão:

| Posição da mão            | pitch lido | O que o código espera |
| :------------------------ | :--------: | :-------------------- |
| Dedos apontando pra cima  |            | `pitch > LIMIAR_CIMA`  (padrão 45) |
| Mão na horizontal (frente)|            | entre os dois |
| Dedos apontando pra baixo |            | `pitch < LIMIAR_BAIXO` (padrão -45) |

Se os valores forem diferentes, ajuste em [`src/main.cpp`](src/main.cpp):
```cpp
const int LIMIAR_CIMA  = 45;
const int LIMIAR_BAIXO = -45;
```

> Isso separa **D** (cima) / **G** (frente) / **Q** (baixo) e o **P** (U pra baixo).

---

## 3. Limiar de movimento (giroscópio)

Olhe o `gyro` e a palavra `parado/MOVENDO`:
- Mão **parada** → `gyro` baixo (uns 0–20).
- Mão **se mexendo** → `gyro` sobe (100+).

Ajuste o ponto de corte se precisar:
```cpp
const float LIMIAR_MOV = 60.0;  // graus/seg acima disso = "movendo"
```

> Isso separa **I/J**, **D/Z** e **U/H**.

---

## 4. Conferir as letras (checklist)

Desligue a calibração (`MODO_CALIBRACAO = false`), grave de novo e faça cada sinal.
Coluna "estados esperados" = o que o `estados` deve mostrar pra letra fechar
(0 esticado · 1 curvado · 2 fechado).

| Letra | Pol | Ind | Méd | Ane | Min | Orientação | Movimento |
| :---: | :-: | :-: | :-: | :-: | :-: | :--------- | :-------- |
| **A** |  0  |  2  |  2  |  2  |  2  | qualquer   | parado    |
| **B** |  2  |  0  |  0  |  0  |  0  | qualquer   | parado    |
| **C** |  1  |  1  |  1  |  1  |  1  | qualquer   | parado    |
| **D** |  2  |  0  |  2  |  2  |  2  | **cima**   | parado    |
| **F** |  2  |  2  |  0  |  0  |  0  | qualquer   | parado    |
| **G** |  2  |  0  |  2  |  2  |  2  | **frente** | parado    |
| **H** |  2  |  0  |  0  |  2  |  2  | qualquer   | **movendo** |
| **I** |  2  |  2  |  2  |  2  |  0  | qualquer   | parado    |
| **J** |  2  |  2  |  2  |  2  |  0  | qualquer   | **movendo** |
| **L** |  0  |  0  |  2  |  2  |  2  | qualquer   | parado    |
| **P** |  2  |  0  |  0  |  2  |  2  | **baixo**  | parado    |
| **Q** |  2  |  0  |  2  |  2  |  2  | **baixo**  | parado    |
| **S** |  2  |  2  |  2  |  2  |  2  | qualquer   | parado    |
| **U** |  2  |  0  |  0  |  2  |  2  | qualquer   | parado    |
| **W** |  2  |  0  |  0  |  0  |  2  | qualquer   | parado    |
| **X** |  2  |  1  |  2  |  2  |  2  | qualquer   | parado    |
| **Y** |  0  |  2  |  2  |  2  |  0  | qualquer   | parado    |
| **Z** |  2  |  0  |  2  |  2  |  2  | qualquer   | **movendo** |

> Os dedos com código `8` na tabela do firmware aceitam **curvado (1) ou fechado (2)** —
> por isso a maioria mostra `2` quando você fecha de verdade, mas `1` também é aceito.

---

## 5. Se uma letra não fechar

1. Volte pro `MODO_CALIBRACAO = true` e faça o gesto dela.
2. Compare o `estados` que aparece com a linha da tabela acima.
3. O dedo que estiver com número errado → ajuste o limiar dele:
   - Está dando `1` quando devia ser `2`? Abaixe o `limiarDobrado` desse dedo.
   - Está dando `2` quando devia ser `1` (caso do C/X)? Suba o `limiarDobrado`.
   - Está dando `1` quando devia ser `0`? Suba o `limiarMeio`.

---

### Ordem recomendada
Dedos → orientação → movimento → conferir letras. Os sinais mais chatos de acertar
são **C** e **X** (dependem do estado `1` curvado), então deixe eles por último.
