# SmartParking

Sistema de estacionamento inteligente com ESP8266, sensor ultrassonico, LEDs indicadores e comunicacao MQTT. O dispositivo mede a distancia da vaga, identifica se ela esta livre ou ocupada, publica o status em um broker MQTT e permite comandos remotos para ligar/desligar o modo automatico ou controlar os LEDs manualmente.

## Funcionamento

O projeto usa um ESP8266 conectado ao Wi-Fi e a um broker MQTT, como o HiveMQ Cloud. A cada 5 segundos, o sensor ultrassonico mede a distancia ate o obstaculo na vaga.

Quando a distancia medida for menor ou igual a `8 cm`, a vaga e considerada `ocupada`. Quando for maior que `8 cm`, a vaga e considerada `livre`.

No modo automatico:

- LED verde ligado: vaga livre.
- LED vermelho ligado: vaga ocupada.
- LEDs desligados: erro na medicao.

No modo manual, o controle dos LEDs e feito por comandos MQTT.

## Estrutura do projeto

```text
SmartParking/
+-- README.md
`-- smartparking/
    +-- smartparking.ino
    `-- secrets.example.h
```

O arquivo `secrets.h` deve ser criado localmente a partir de `secrets.example.h`. Ele nao e versionado porque contem credenciais de Wi-Fi e MQTT.

## Requisitos

### Hardware

- Placa ESP8266, como NodeMCU ou Wemos D1 Mini.
- Sensor ultrassonico HC-SR04 ou equivalente.
- LED verde.
- LED vermelho.
- 2 resistores para os LEDs.
- Jumpers e protoboard.
- Divisor de tensao para o pino Echo do HC-SR04.

### Software

- Arduino IDE.
- Suporte a placas ESP8266 instalado na Arduino IDE.
- Biblioteca `PubSubClient`.
- Conta ou broker MQTT acessivel pela internet ou rede local.
- Cliente MQTT para testes, como MQTTX.

## Ligacao dos pinos

O sketch usa os seguintes GPIOs do ESP8266:

| Componente | GPIO | NodeMCU |
| --- | --- | --- |
| Trigger do sensor ultrassonico | GPIO12 | D6 |
| Echo do sensor ultrassonico | GPIO14 | D5 |
| LED verde | GPIO5 | D1 |
| LED vermelho | GPIO4 | D2 |

Observacao: muitos sensores HC-SR04 trabalham com sinal de Echo em 5 V. O ESP8266 usa 3,3 V nos GPIOs, entao proteja o pino Echo com divisor de tensao ou conversor de nivel logico.

## Instalacao

1. Clone ou baixe este repositorio.

```bash
git clone <url-do-repositorio>
cd SmartParking
```

2. Abra o projeto na Arduino IDE.

Abra o arquivo:

```text
smartparking/smartparking.ino
```

3. Instale o suporte ao ESP8266 na Arduino IDE.

Na Arduino IDE, acesse `Arquivo > Preferencias` e adicione a URL abaixo em `URLs adicionais para Gerenciadores de Placas`:

```text
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Depois acesse `Ferramentas > Placa > Gerenciador de Placas`, procure por `esp8266` e instale o pacote.

4. Instale a biblioteca MQTT.

Na Arduino IDE, acesse `Sketch > Incluir Biblioteca > Gerenciar Bibliotecas`, procure por `PubSubClient` e instale a biblioteca.

5. Configure as credenciais.

Copie o arquivo de exemplo:

```bash
cp smartparking/secrets.example.h smartparking/secrets.h
```

No Windows PowerShell:

```powershell
Copy-Item smartparking\secrets.example.h smartparking\secrets.h
```

Edite `smartparking/secrets.h` com os dados reais:

```cpp
#define WIFI_SSID "NOME_DA_REDE_WIFI"
#define WIFI_PASSWORD "SENHA_DA_REDE_WIFI"

#define MQTT_SERVER "HOST_DO_BROKER_MQTT"
#define MQTT_PORT 8883
#define MQTT_USER "USUARIO_MQTT"
#define MQTT_PASSWORD "SENHA_MQTT"

#define MQTT_TOPIC "inatel/smartparking/vaga01/status"
```

6. Selecione a placa e a porta.

Na Arduino IDE, selecione a placa ESP8266 usada no projeto em `Ferramentas > Placa`. Para NodeMCU, use normalmente `NodeMCU 1.0 (ESP-12E Module)`.

Depois selecione a porta serial em `Ferramentas > Porta`.

7. Envie o sketch para a placa.

Clique em `Upload` na Arduino IDE. Apos o envio, abra o Monitor Serial em `115200 baud` para acompanhar a conexao Wi-Fi, a conexao MQTT e as medicoes.

## Uso

Ao iniciar, o ESP8266:

1. Conecta ao Wi-Fi configurado.
2. Conecta ao broker MQTT.
3. Publica uma mensagem informando que esta online.
4. Inscreve-se no topico de comandos.
5. Publica periodicamente o status da vaga.

### Topicos MQTT

| Finalidade | Topico |
| --- | --- |
| Publicacao do status da vaga | `inatel/smartparking/vaga01/status` |
| Recebimento de comandos | `inatel/smartparking/vaga01/comando` |
| Resposta aos comandos | `inatel/smartparking/vaga01/resposta` |

O topico de status pode ser alterado em `secrets.h` pela constante `MQTT_TOPIC`.

### Payload de status

Exemplo de mensagem publicada quando a vaga esta livre:

```json
{
  "vaga": "vaga01",
  "sistema": "ligado",
  "modo": "automatico",
  "status": "livre",
  "distance_cm": 23.45,
  "distance_inch": 9.23
}
```

Exemplo de mensagem publicada quando a vaga esta ocupada:

```json
{
  "vaga": "vaga01",
  "sistema": "ligado",
  "modo": "automatico",
  "status": "ocupada",
  "distance_cm": 6.50,
  "distance_inch": 2.56
}
```

Em caso de falha na medicao, o status publicado sera `erro`.

### Comandos MQTT

Publique os comandos no topico:

```text
inatel/smartparking/vaga01/comando
```

Comandos aceitos:

| Comando | Acao |
| --- | --- |
| `SISTEMA_LIGAR` | Liga o modo automatico. |
| `SISTEMA_DESLIGAR` | Desliga o modo automatico e libera o modo manual. |
| `VERDE_ON` ou `VERDE` | Liga o LED verde no modo manual. |
| `VERDE_OFF` | Desliga o LED verde no modo manual. |
| `VERMELHO_ON` ou `VERMELHO` | Liga o LED vermelho no modo manual. |
| `VERMELHO_OFF` | Desliga o LED vermelho no modo manual. |
| `AMBOS_ON` | Liga os dois LEDs no modo manual. |
| `DESLIGAR` ou `LEDS_OFF` | Desliga os dois LEDs no modo manual. |

Os comandos manuais de LED so funcionam depois de enviar `SISTEMA_DESLIGAR`. Se o sistema automatico estiver ligado, o ESP8266 ignora comandos manuais e publica uma resposta de alerta.

### Teste com MQTTX

1. Crie uma conexao no MQTTX usando o mesmo host, porta, usuario e senha configurados em `secrets.h`.
2. Use conexao segura/TLS se estiver usando HiveMQ Cloud na porta `8883`.
3. Inscreva-se nos topicos:

```text
inatel/smartparking/vaga01/status
inatel/smartparking/vaga01/resposta
```

4. Publique comandos no topico:

```text
inatel/smartparking/vaga01/comando
```

Exemplo de fluxo para testar o modo manual:

```text
SISTEMA_DESLIGAR
VERDE_ON
VERMELHO_ON
LEDS_OFF
SISTEMA_LIGAR
```

## Ajustes

### Alterar distancia para vaga ocupada

No arquivo `smartparking/smartparking.ino`, altere a constante:

```cpp
const float DISTANCIA_OCUPADA_CM = 8.0;
```

### Alterar intervalo de publicacao

No arquivo `smartparking/smartparking.ino`, altere:

```cpp
const unsigned long publishInterval = 5000;
```

O valor esta em milissegundos.

### Alterar topicos de comando e resposta

No arquivo `smartparking/smartparking.ino`, altere:

```cpp
const char* mqtt_command_topic = "inatel/smartparking/vaga01/comando";
const char* mqtt_response_topic = "inatel/smartparking/vaga01/resposta";
```

## Solucao de problemas

### O ESP8266 nao conecta ao Wi-Fi

- Verifique `WIFI_SSID` e `WIFI_PASSWORD` em `secrets.h`.
- Confirme se a rede Wi-Fi e 2,4 GHz, pois o ESP8266 nao suporta 5 GHz.
- Abra o Monitor Serial em `115200 baud` para ver as mensagens de conexao.

### O ESP8266 nao conecta ao MQTT

- Verifique `MQTT_SERVER`, `MQTT_PORT`, `MQTT_USER` e `MQTT_PASSWORD`.
- Se estiver usando HiveMQ Cloud, mantenha a porta `8883`.
- Confirme se o usuario MQTT tem permissao para publicar e assinar os topicos usados.

### As medicoes falham ou ficam instaveis

- Verifique a alimentacao do sensor ultrassonico.
- Confira os pinos Trigger e Echo.
- Proteja o pino Echo do ESP8266 quando o sensor trabalhar em 5 V.
- Ajuste a posicao do sensor para evitar reflexos ruins ou superficies muito inclinadas.

## Observacoes de seguranca

- Nao envie o arquivo `secrets.h` para o repositorio.
- Nao publique credenciais reais em prints, issues ou commits.
- O sketch usa `espClient.setInsecure()`, portanto nao valida o certificado TLS do broker. Para uso em producao, configure validacao de certificado.
